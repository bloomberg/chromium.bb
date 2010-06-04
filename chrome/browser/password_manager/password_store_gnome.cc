// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_gnome.h"

#include <string>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"

using std::map;
using std::string;
using std::vector;
using webkit_glue::PasswordForm;

#define GNOME_KEYRING_APPLICATION_CHROME "chrome"

// Schema is analagous to the fields in PasswordForm.
const GnomeKeyringPasswordSchema PasswordStoreGnome::kGnomeSchema = {
  GNOME_KEYRING_ITEM_GENERIC_SECRET, {
    { "origin_url", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "action_url", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "username_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "username_value", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "password_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "submit_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "signon_realm", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "ssl_valid", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32 },
    { "preferred", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32 },
    { "date_created", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "blacklisted_by_user", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32 },
    { "scheme", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32 },
    // This field is always "chrome" so that we can search for it.
    { "application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { NULL }
  }
};

PasswordStoreGnome::PasswordStoreGnome(LoginDatabase* login_db,
                                       Profile* profile,
                                       WebDataService* web_data_service) {
}

PasswordStoreGnome::~PasswordStoreGnome() {
}

bool PasswordStoreGnome::Init() {
  return PasswordStore::Init() && gnome_keyring_is_available();
}

void PasswordStoreGnome::AddLoginImpl(const PasswordForm& form) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  AddLoginHelper(form, base::Time::Now());
}

void PasswordStoreGnome::UpdateLoginImpl(const PasswordForm& form) {
  // Based on LoginDatabase::UpdateLogin(), we search for forms to update by
  // origin_url, username_element, username_value, password_element, and
  // signon_realm. We then compare the result to the updated form. If they
  // differ in any of the action, password_value, ssl_valid, or preferred
  // fields, then we add a new login with those fields updated and only delete
  // the original on success.
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  GList* found = NULL;
  // Search gnome keyring for matching passwords.
  GnomeKeyringResult result = gnome_keyring_find_itemsv_sync(
      GNOME_KEYRING_ITEM_GENERIC_SECRET,
      &found,
      "origin_url", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      form.origin.spec().c_str(),
      "username_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      UTF16ToUTF8(form.username_element).c_str(),
      "username_value", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      UTF16ToUTF8(form.username_value).c_str(),
      "password_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      UTF16ToUTF8(form.password_element).c_str(),
      "signon_realm", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      form.signon_realm.c_str(),
      "application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      GNOME_KEYRING_APPLICATION_CHROME,
      NULL);
  vector<PasswordForm*> forms;
  if (result == GNOME_KEYRING_RESULT_OK) {
    FillFormVector(found, &forms);
    for (size_t i = 0; i < forms.size(); ++i) {
      if (forms[i]->action != form.action ||
          forms[i]->password_value != form.password_value ||
          forms[i]->ssl_valid != form.ssl_valid ||
          forms[i]->preferred != form.preferred) {
        PasswordForm updated = *forms[i];
        updated.action = form.action;
        updated.password_value = form.password_value;
        updated.ssl_valid = form.ssl_valid;
        updated.preferred = form.preferred;
        if (AddLoginHelper(updated, updated.date_created))
          RemoveLoginImpl(*forms[i]);
      }
      delete forms[i];
    }
  } else {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
  }
}

void PasswordStoreGnome::RemoveLoginImpl(const PasswordForm& form) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  // We find forms using the same fields as LoginDatabase::RemoveLogin().
  GnomeKeyringResult result = gnome_keyring_delete_password_sync(
      &kGnomeSchema,
      "origin_url", form.origin.spec().c_str(),
      "action_url", form.action.spec().c_str(),
      "username_element", UTF16ToUTF8(form.username_element).c_str(),
      "username_value", UTF16ToUTF8(form.username_value).c_str(),
      "password_element", UTF16ToUTF8(form.password_element).c_str(),
      "submit_element", UTF16ToUTF8(form.submit_element).c_str(),
      "signon_realm", form.signon_realm.c_str(),
      NULL);
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring delete failed: "
               << gnome_keyring_result_to_message(result);
  }
}

void PasswordStoreGnome::RemoveLoginsCreatedBetweenImpl(
    const base::Time& delete_begin,
    const base::Time& delete_end) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  GList* found = NULL;
  // Search GNOME keyring for all passwords, then delete the ones in the range.
  // We need to search for something, otherwise we get no results - so we search
  // for the fixed application string.
  GnomeKeyringResult result = gnome_keyring_find_itemsv_sync(
      GNOME_KEYRING_ITEM_GENERIC_SECRET,
      &found,
      "application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      GNOME_KEYRING_APPLICATION_CHROME,
      NULL);
  if (result == GNOME_KEYRING_RESULT_OK) {
    // We could walk the list and delete items as we find them, but it is much
    // easier to build the vector and use RemoveLoginImpl() to delete them.
    vector<PasswordForm*> forms;
    FillFormVector(found, &forms);
    for (size_t i = 0; i < forms.size(); ++i) {
      if (delete_begin <= forms[i]->date_created &&
          (delete_end.is_null() || forms[i]->date_created < delete_end)) {
        RemoveLoginImpl(*forms[i]);
      }
      delete forms[i];
    }
  } else if (result != GNOME_KEYRING_RESULT_NO_MATCH) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
  }
}

void PasswordStoreGnome::GetLoginsImpl(GetLoginsRequest* request,
                                       const PasswordForm& form) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  GList* found = NULL;
  // Search gnome keyring for matching passwords.
  GnomeKeyringResult result = gnome_keyring_find_itemsv_sync(
      GNOME_KEYRING_ITEM_GENERIC_SECRET,
      &found,
      "signon_realm", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      form.signon_realm.c_str(),
      "application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      GNOME_KEYRING_APPLICATION_CHROME,
      NULL);
  vector<PasswordForm*> forms;
  if (result == GNOME_KEYRING_RESULT_OK) {
    FillFormVector(found, &forms);
  } else if (result != GNOME_KEYRING_RESULT_NO_MATCH) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
  }
  NotifyConsumer(request, forms);
}

void PasswordStoreGnome::GetAutofillableLoginsImpl(
    GetLoginsRequest* request) {
  std::vector<PasswordForm*> forms;
  FillAutofillableLogins(&forms);
  NotifyConsumer(request, forms);
}

void PasswordStoreGnome::GetBlacklistLoginsImpl(
    GetLoginsRequest* request) {
  std::vector<PasswordForm*> forms;
  FillBlacklistLogins(&forms);
  NotifyConsumer(request, forms);
}

bool PasswordStoreGnome::FillAutofillableLogins(
    std::vector<PasswordForm*>* forms) {
  return FillSomeLogins(true, forms);
}

bool PasswordStoreGnome::FillBlacklistLogins(
    std::vector<PasswordForm*>* forms) {
  return FillSomeLogins(false, forms);
}

bool PasswordStoreGnome::AddLoginHelper(const PasswordForm& form,
                                        const base::Time& date_created) {
  GnomeKeyringResult result = gnome_keyring_store_password_sync(
      &kGnomeSchema,
      NULL,  // Default keyring.
      form.origin.spec().c_str(),  // Display name.
      UTF16ToUTF8(form.password_value).c_str(),
      "origin_url", form.origin.spec().c_str(),
      "action_url", form.action.spec().c_str(),
      "username_element", UTF16ToUTF8(form.username_element).c_str(),
      "username_value", UTF16ToUTF8(form.username_value).c_str(),
      "password_element", UTF16ToUTF8(form.password_element).c_str(),
      "submit_element", UTF16ToUTF8(form.submit_element).c_str(),
      "signon_realm", form.signon_realm.c_str(),
      "ssl_valid", form.ssl_valid,
      "preferred", form.preferred,
      "date_created", Int64ToString(date_created.ToTimeT()).c_str(),
      "blacklisted_by_user", form.blacklisted_by_user,
      "scheme", form.scheme,
      "application", GNOME_KEYRING_APPLICATION_CHROME,
      NULL);

  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring save failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
  return true;
}

bool PasswordStoreGnome::FillSomeLogins(
    bool autofillable,
    std::vector<PasswordForm*>* forms) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  GList* found = NULL;
  uint32_t blacklisted_by_user = !autofillable;
  // Search gnome keyring for matching passwords.
  GnomeKeyringResult result = gnome_keyring_find_itemsv_sync(
      GNOME_KEYRING_ITEM_GENERIC_SECRET,
      &found,
      "blacklisted_by_user", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32,
      blacklisted_by_user,
      "application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      GNOME_KEYRING_APPLICATION_CHROME,
      NULL);
  if (result == GNOME_KEYRING_RESULT_OK) {
    FillFormVector(found, forms);
  } else if (result != GNOME_KEYRING_RESULT_NO_MATCH) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
  return true;
}

void PasswordStoreGnome::FillFormVector(GList* found,
    std::vector<PasswordForm*>* forms) {
  GList* element = g_list_first(found);
  while (element != NULL) {
    GnomeKeyringFound* data = static_cast<GnomeKeyringFound*>(element->data);
    char* password = data->secret;

    GnomeKeyringAttributeList* attributes = data->attributes;
    // Read the string and int attributes into the appropriate map.
    map<string, string> string_attribute_map;
    map<string, uint32> uint_attribute_map;
    for (unsigned int i = 0; i < attributes->len; ++i) {
      GnomeKeyringAttribute attribute =
          gnome_keyring_attribute_list_index(attributes, i);
      if (attribute.type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING) {
        string_attribute_map[string(attribute.name)] =
            string(attribute.value.string);
      } else if (attribute.type == GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32) {
        uint_attribute_map[string(attribute.name)] = attribute.value.integer;
      }
    }

    PasswordForm* form = new PasswordForm();
    form->origin = GURL(string_attribute_map["origin_url"]);
    form->action = GURL(string_attribute_map["action_url"]);
    form->username_element =
        UTF8ToUTF16(string(string_attribute_map["username_element"]));
    form->username_value =
        UTF8ToUTF16(string(string_attribute_map["username_value"]));
    form->password_element =
        UTF8ToUTF16(string(string_attribute_map["password_element"]));
    form->password_value = UTF8ToUTF16(string(password));
    form->submit_element =
        UTF8ToUTF16(string(string_attribute_map["submit_element"]));
    form->signon_realm = string_attribute_map["signon_realm"];
    form->ssl_valid = uint_attribute_map["ssl_valid"];
    form->preferred = uint_attribute_map["preferred"];
    string date = string_attribute_map["date_created"];
    int64 date_created = 0;
    bool date_ok = StringToInt64(date, &date_created);
    DCHECK(date_ok);
    DCHECK(date_created != 0);
    form->date_created = base::Time::FromTimeT(date_created);
    form->blacklisted_by_user = uint_attribute_map["blacklisted_by_user"];
    form->scheme = static_cast<PasswordForm::Scheme>(
        uint_attribute_map["scheme"]);

    forms->push_back(form);

    element = g_list_next(element);
  }
  gnome_keyring_found_list_free(found);
}
