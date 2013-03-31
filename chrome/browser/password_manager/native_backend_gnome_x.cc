// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/native_backend_gnome_x.h"

#include <dlfcn.h>
#include <gnome-keyring.h>

#include <map>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;
using content::PasswordForm;

#define GNOME_KEYRING_DEFINE_POINTER(name) \
  typeof(&::gnome_keyring_##name) GnomeKeyringLoader::gnome_keyring_##name;
GNOME_KEYRING_FOR_EACH_FUNC(GNOME_KEYRING_DEFINE_POINTER)
#undef GNOME_KEYRING_DEFINE_POINTER

bool GnomeKeyringLoader::keyring_loaded = false;

#if defined(DLOPEN_GNOME_KEYRING)

#define GNOME_KEYRING_FUNCTION_INFO(name) \
  {"gnome_keyring_"#name, reinterpret_cast<void**>(&gnome_keyring_##name)},
const GnomeKeyringLoader::FunctionInfo GnomeKeyringLoader::functions[] = {
  GNOME_KEYRING_FOR_EACH_FUNC(GNOME_KEYRING_FUNCTION_INFO)
  {NULL, NULL}
};
#undef GNOME_KEYRING_FUNCTION_INFO

/* Load the library and initialize the function pointers. */
bool GnomeKeyringLoader::LoadGnomeKeyring() {
  if (keyring_loaded)
    return true;

  void* handle = dlopen("libgnome-keyring.so.0", RTLD_NOW | RTLD_GLOBAL);
  if (!handle) {
    // We wanted to use GNOME Keyring, but we couldn't load it. Warn, because
    // either the user asked for this, or we autodetected it incorrectly. (Or
    // the system has broken libraries, which is also good to warn about.)
    LOG(WARNING) << "Could not load libgnome-keyring.so.0: " << dlerror();
    return false;
  }

  for (size_t i = 0; functions[i].name; ++i) {
    dlerror();
    *functions[i].pointer = dlsym(handle, functions[i].name);
    const char* error = dlerror();
    if (error) {
      LOG(ERROR) << "Unable to load symbol "
                 << functions[i].name << ": " << error;
      dlclose(handle);
      return false;
    }
  }

  keyring_loaded = true;
  // We leak the library handle. That's OK: this function is called only once.
  return true;
}

#else  // defined(DLOPEN_GNOME_KEYRING)

bool GnomeKeyringLoader::LoadGnomeKeyring() {
  if (keyring_loaded)
    return true;
#define GNOME_KEYRING_ASSIGN_POINTER(name) \
  gnome_keyring_##name = &::gnome_keyring_##name;
  GNOME_KEYRING_FOR_EACH_FUNC(GNOME_KEYRING_ASSIGN_POINTER)
#undef GNOME_KEYRING_ASSIGN_POINTER
  keyring_loaded = true;
  return true;
}

#endif  // defined(DLOPEN_GNOME_KEYRING)

namespace {

const char kGnomeKeyringAppString[] = "chrome";

// Convert the attributes of a given keyring entry into a new PasswordForm.
// Note: does *not* get the actual password, as that is not a key attribute!
// Returns NULL if the attributes are for the wrong application.
PasswordForm* FormFromAttributes(GnomeKeyringAttributeList* attrs) {
  // Read the string and int attributes into the appropriate map.
  std::map<std::string, std::string> string_attr_map;
  std::map<std::string, uint32_t> uint_attr_map;
  for (guint i = 0; i < attrs->len; ++i) {
    GnomeKeyringAttribute attr = gnome_keyring_attribute_list_index(attrs, i);
    if (attr.type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING)
      string_attr_map[attr.name] = attr.value.string;
    else if (attr.type == GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32)
      uint_attr_map[attr.name] = attr.value.integer;
  }
  // Check to make sure this is a password we care about.
  const std::string& app_value = string_attr_map["application"];
  if (!base::StringPiece(app_value).starts_with(kGnomeKeyringAppString))
    return NULL;

  PasswordForm* form = new PasswordForm();
  form->origin = GURL(string_attr_map["origin_url"]);
  form->action = GURL(string_attr_map["action_url"]);
  form->username_element = UTF8ToUTF16(string_attr_map["username_element"]);
  form->username_value = UTF8ToUTF16(string_attr_map["username_value"]);
  form->password_element = UTF8ToUTF16(string_attr_map["password_element"]);
  form->submit_element = UTF8ToUTF16(string_attr_map["submit_element"]);
  form->signon_realm = string_attr_map["signon_realm"];
  form->ssl_valid = uint_attr_map["ssl_valid"];
  form->preferred = uint_attr_map["preferred"];
  int64 date_created = 0;
  bool date_ok = base::StringToInt64(string_attr_map["date_created"],
                                     &date_created);
  DCHECK(date_ok);
  form->date_created = base::Time::FromTimeT(date_created);
  form->blacklisted_by_user = uint_attr_map["blacklisted_by_user"];
  form->scheme = static_cast<PasswordForm::Scheme>(uint_attr_map["scheme"]);

  return form;
}

// Parse all the results from the given GList into a PasswordFormList, and free
// the GList. PasswordForms are allocated on the heap, and should be deleted by
// the consumer.
void ConvertFormList(GList* found,
                     NativeBackendGnome::PasswordFormList* forms) {
  GList* element = g_list_first(found);
  while (element != NULL) {
    GnomeKeyringFound* data = static_cast<GnomeKeyringFound*>(element->data);
    GnomeKeyringAttributeList* attrs = data->attributes;

    PasswordForm* form = FormFromAttributes(attrs);
    if (form) {
      if (data->secret) {
        form->password_value = UTF8ToUTF16(data->secret);
      } else {
        LOG(WARNING) << "Unable to access password from list element!";
      }
      forms->push_back(form);
    } else {
      LOG(WARNING) << "Could not initialize PasswordForm from attributes!";
    }

    element = g_list_next(element);
  }
}

// Schema is analagous to the fields in PasswordForm.
const GnomeKeyringPasswordSchema kGnomeSchema = {
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

// Sadly, PasswordStore goes to great lengths to switch from the originally
// calling thread to the DB thread, and to provide an asynchronous API to
// callers while using a synchronous (virtual) API provided by subclasses like
// PasswordStoreX -- but GNOME Keyring really wants to be on the GLib main
// thread, which is the UI thread to us. So we end up having to switch threads
// again, possibly back to the very same thread (in case the UI thread is the
// caller, e.g. in the password management UI), and *block* the DB thread
// waiting for a response from the UI thread to provide the synchronous API
// PasswordStore expects of us. (It will then in turn switch back to the
// original caller to send the asynchronous reply to the original request.)

// This class represents a call to a GNOME Keyring method. A RunnableMethod
// should be posted to the UI thread to call one of its action methods, and then
// a WaitResult() method should be called to wait for the result. Each instance
// supports only one outstanding method at a time, though multiple instances may
// be used in parallel.
class GKRMethod : public GnomeKeyringLoader {
 public:
  typedef NativeBackendGnome::PasswordFormList PasswordFormList;

  GKRMethod() : event_(false, false), result_(GNOME_KEYRING_RESULT_CANCELLED) {}

  // Action methods. These call gnome_keyring_* functions. Call from UI thread.
  // See GetProfileSpecificAppString() for more information on the app string.
  void AddLogin(const PasswordForm& form, const char* app_string);
  void AddLoginSearch(const PasswordForm& form, const char* app_string);
  void UpdateLoginSearch(const PasswordForm& form, const char* app_string);
  void RemoveLogin(const PasswordForm& form, const char* app_string);
  void GetLogins(const PasswordForm& form, const char* app_string);
  void GetLoginsList(uint32_t blacklisted_by_user, const char* app_string);
  void GetAllLogins(const char* app_string);

  // Use after AddLogin, RemoveLogin.
  GnomeKeyringResult WaitResult();

  // Use after AddLoginSearch, UpdateLoginSearch, GetLogins, GetLoginsList,
  // GetAllLogins.
  GnomeKeyringResult WaitResult(PasswordFormList* forms);

 private:
  // All these callbacks are called on UI thread.
  static void OnOperationDone(GnomeKeyringResult result, gpointer data);

  static void OnOperationGetList(GnomeKeyringResult result, GList* list,
                                 gpointer data);

  base::WaitableEvent event_;
  GnomeKeyringResult result_;
  NativeBackendGnome::PasswordFormList forms_;
};

void GKRMethod::AddLogin(const PasswordForm& form, const char* app_string) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  time_t date_created = form.date_created.ToTimeT();
  // If we are asked to save a password with 0 date, use the current time.
  // We don't want to actually save passwords as though on January 1, 1970.
  if (!date_created)
    date_created = time(NULL);
  gnome_keyring_store_password(
      &kGnomeSchema,
      NULL,  // Default keyring.
      form.origin.spec().c_str(),  // Display name.
      UTF16ToUTF8(form.password_value).c_str(),
      OnOperationDone,
      this,  // data
      NULL,  // destroy_data
      "origin_url", form.origin.spec().c_str(),
      "action_url", form.action.spec().c_str(),
      "username_element", UTF16ToUTF8(form.username_element).c_str(),
      "username_value", UTF16ToUTF8(form.username_value).c_str(),
      "password_element", UTF16ToUTF8(form.password_element).c_str(),
      "submit_element", UTF16ToUTF8(form.submit_element).c_str(),
      "signon_realm", form.signon_realm.c_str(),
      "ssl_valid", form.ssl_valid,
      "preferred", form.preferred,
      "date_created", base::Int64ToString(date_created).c_str(),
      "blacklisted_by_user", form.blacklisted_by_user,
      "scheme", form.scheme,
      "application", app_string,
      NULL);
}

void GKRMethod::AddLoginSearch(const PasswordForm& form,
                               const char* app_string) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Search GNOME Keyring for matching passwords to update.
  gnome_keyring_find_itemsv(
      GNOME_KEYRING_ITEM_GENERIC_SECRET,
      OnOperationGetList,
      this,  // data
      NULL,  // destroy_data
      "origin_url", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      form.origin.spec().c_str(),
      "username_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      UTF16ToUTF8(form.username_element).c_str(),
      "username_value", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      UTF16ToUTF8(form.username_value).c_str(),
      "password_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      UTF16ToUTF8(form.password_element).c_str(),
      "submit_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      UTF16ToUTF8(form.submit_element).c_str(),
      "signon_realm", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      form.signon_realm.c_str(),
      "application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      app_string,
      NULL);
}

void GKRMethod::UpdateLoginSearch(const PasswordForm& form,
                                  const char* app_string) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Search GNOME Keyring for matching passwords to update.
  gnome_keyring_find_itemsv(
      GNOME_KEYRING_ITEM_GENERIC_SECRET,
      OnOperationGetList,
      this,  // data
      NULL,  // destroy_data
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
      app_string,
      NULL);
}

void GKRMethod::RemoveLogin(const PasswordForm& form, const char* app_string) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // We find forms using the same fields as LoginDatabase::RemoveLogin().
  gnome_keyring_delete_password(
      &kGnomeSchema,
      OnOperationDone,
      this,  // data
      NULL,  // destroy_data
      "origin_url", form.origin.spec().c_str(),
      "action_url", form.action.spec().c_str(),
      "username_element", UTF16ToUTF8(form.username_element).c_str(),
      "username_value", UTF16ToUTF8(form.username_value).c_str(),
      "password_element", UTF16ToUTF8(form.password_element).c_str(),
      "submit_element", UTF16ToUTF8(form.submit_element).c_str(),
      "signon_realm", form.signon_realm.c_str(),
      "application", app_string,
      NULL);
}

void GKRMethod::GetLogins(const PasswordForm& form, const char* app_string) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Search GNOME Keyring for matching passwords.
  gnome_keyring_find_itemsv(
      GNOME_KEYRING_ITEM_GENERIC_SECRET,
      OnOperationGetList,
      this,  // data
      NULL,  // destroy_data
      "signon_realm", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      form.signon_realm.c_str(),
      "application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      app_string,
      NULL);
}

void GKRMethod::GetLoginsList(uint32_t blacklisted_by_user,
                              const char* app_string) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Search GNOME Keyring for matching passwords.
  gnome_keyring_find_itemsv(
      GNOME_KEYRING_ITEM_GENERIC_SECRET,
      OnOperationGetList,
      this,  // data
      NULL,  // destroy_data
      "blacklisted_by_user", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32,
      blacklisted_by_user,
      "application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      app_string,
      NULL);
}

void GKRMethod::GetAllLogins(const char* app_string) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // We need to search for something, otherwise we get no results - so
  // we search for the fixed application string.
  gnome_keyring_find_itemsv(
      GNOME_KEYRING_ITEM_GENERIC_SECRET,
      OnOperationGetList,
      this,  // data
      NULL,  // destroy_data
      "application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      app_string,
      NULL);
}

GnomeKeyringResult GKRMethod::WaitResult() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  event_.Wait();
  return result_;
}

GnomeKeyringResult GKRMethod::WaitResult(PasswordFormList* forms) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  event_.Wait();
  if (forms->empty()) {
    // Normal case. Avoid extra allocation by swapping.
    forms->swap(forms_);
  } else {
    // Rare case. Append forms_ to *forms.
    forms->insert(forms->end(), forms_.begin(), forms_.end());
    forms_.clear();
  }
  return result_;
}

// static
void GKRMethod::OnOperationDone(GnomeKeyringResult result, gpointer data) {
  GKRMethod* method = static_cast<GKRMethod*>(data);
  method->result_ = result;
  method->event_.Signal();
}

// static
void GKRMethod::OnOperationGetList(GnomeKeyringResult result, GList* list,
                                   gpointer data) {
  GKRMethod* method = static_cast<GKRMethod*>(data);
  method->result_ = result;
  method->forms_.clear();
  // |list| will be freed after this callback returns, so convert it now.
  ConvertFormList(list, &method->forms_);
  method->event_.Signal();
}

}  // namespace

NativeBackendGnome::NativeBackendGnome(LocalProfileId id, PrefService* prefs)
    : profile_id_(id), prefs_(prefs) {
  // TODO(mdm): after a few more releases, remove the code which is now dead due
  // to the true || here, and simplify this code. We don't do it yet to make it
  // easier to revert if necessary.
  if (true || PasswordStoreX::PasswordsUseLocalProfileId(prefs)) {
    app_string_ = GetProfileSpecificAppString();
    // We already did the migration previously. Don't try again.
    migrate_tried_ = true;
  } else {
    app_string_ = kGnomeKeyringAppString;
    migrate_tried_ = false;
  }
}

NativeBackendGnome::~NativeBackendGnome() {
}

bool NativeBackendGnome::Init() {
  return LoadGnomeKeyring() && gnome_keyring_is_available();
}

bool NativeBackendGnome::RawAddLogin(const PasswordForm& form) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  GKRMethod method;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&GKRMethod::AddLogin,
                                     base::Unretained(&method),
                                     form, app_string_.c_str()));
  GnomeKeyringResult result = method.WaitResult();
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring save failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
  // Successful write. Try migration if necessary.
  if (!migrate_tried_)
    MigrateToProfileSpecificLogins();
  return true;
}

bool NativeBackendGnome::AddLogin(const PasswordForm& form) {
  // Based on LoginDatabase::AddLogin(), we search for an existing match based
  // on origin_url, username_element, username_value, password_element, submit
  // element, and signon_realm first, remove that, and then add the new entry.
  // We'd add the new one first, and then delete the original, but then the
  // delete might actually delete the newly-added entry!
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  GKRMethod method;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&GKRMethod::AddLoginSearch,
                                     base::Unretained(&method),
                                     form, app_string_.c_str()));
  PasswordFormList forms;
  GnomeKeyringResult result = method.WaitResult(&forms);
  if (result != GNOME_KEYRING_RESULT_OK &&
      result != GNOME_KEYRING_RESULT_NO_MATCH) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
  if (forms.size() > 0) {
    if (forms.size() > 1) {
      LOG(WARNING) << "Adding login when there are " << forms.size()
                   << " matching logins already! Will replace only the first.";
    }

    // We try migration before updating the existing logins, since otherwise
    // we'd do it after making some but not all of the changes below.
    if (forms.size() > 0 && !migrate_tried_)
      MigrateToProfileSpecificLogins();

    RemoveLogin(*forms[0]);
    for (size_t i = 0; i < forms.size(); ++i)
      delete forms[i];
  }
  return RawAddLogin(form);
}

bool NativeBackendGnome::UpdateLogin(const PasswordForm& form) {
  // Based on LoginDatabase::UpdateLogin(), we search for forms to update by
  // origin_url, username_element, username_value, password_element, and
  // signon_realm. We then compare the result to the updated form. If they
  // differ in any of the action, password_value, ssl_valid, or preferred
  // fields, then we remove the original, and then add the new entry. We'd add
  // the new one first, and then delete the original, but then the delete might
  // actually delete the newly-added entry!
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  GKRMethod method;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&GKRMethod::UpdateLoginSearch,
                                     base::Unretained(&method),
                                     form, app_string_.c_str()));
  PasswordFormList forms;
  GnomeKeyringResult result = method.WaitResult(&forms);
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }

  // We try migration before updating the existing logins, since otherwise
  // we'd do it after making some but not all of the changes below.
  if (forms.size() > 0 && !migrate_tried_)
    MigrateToProfileSpecificLogins();

  bool ok = true;
  for (size_t i = 0; i < forms.size(); ++i) {
    if (forms[i]->action != form.action ||
        forms[i]->password_value != form.password_value ||
        forms[i]->ssl_valid != form.ssl_valid ||
        forms[i]->preferred != form.preferred) {
      RemoveLogin(*forms[i]);
    }
  }
  for (size_t i = 0; i < forms.size(); ++i) {
    if (forms[i]->action != form.action ||
        forms[i]->password_value != form.password_value ||
        forms[i]->ssl_valid != form.ssl_valid ||
        forms[i]->preferred != form.preferred) {
      forms[i]->action = form.action;
      forms[i]->password_value = form.password_value;
      forms[i]->ssl_valid = form.ssl_valid;
      forms[i]->preferred = form.preferred;
      if (!RawAddLogin(*forms[i]))
        ok = false;
    }
    delete forms[i];
  }
  return ok;
}

bool NativeBackendGnome::RemoveLogin(const PasswordForm& form) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  GKRMethod method;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&GKRMethod::RemoveLogin,
                                     base::Unretained(&method),
                                     form, app_string_.c_str()));
  GnomeKeyringResult result = method.WaitResult();
  if (result != GNOME_KEYRING_RESULT_OK) {
    // Warning, not error, because this can sometimes happen due to the user
    // racing with the daemon to delete the password a second time.
    LOG(WARNING) << "Keyring delete failed: "
                 << gnome_keyring_result_to_message(result);
    return false;
  }
  // Successful write. Try migration if necessary. Note that presumably if we've
  // been asked to delete a login, it's because we returned it previously; thus,
  // this will probably never happen since we'd have already tried migration.
  if (!migrate_tried_)
    MigrateToProfileSpecificLogins();
  return true;
}

bool NativeBackendGnome::RemoveLoginsCreatedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  bool ok = true;
  // We could walk the list and delete items as we find them, but it is much
  // easier to build the list and use RemoveLogin() to delete them.
  PasswordFormList forms;
  if (!GetAllLogins(&forms))
    return false;
  // No need to try migration here: GetAllLogins() does it.

  for (size_t i = 0; i < forms.size(); ++i) {
    if (delete_begin <= forms[i]->date_created &&
        (delete_end.is_null() || forms[i]->date_created < delete_end)) {
      if (!RemoveLogin(*forms[i]))
        ok = false;
    }
    delete forms[i];
  }
  return ok;
}

bool NativeBackendGnome::GetLogins(const PasswordForm& form,
                                   PasswordFormList* forms) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  GKRMethod method;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&GKRMethod::GetLogins,
                                     base::Unretained(&method),
                                     form, app_string_.c_str()));
  GnomeKeyringResult result = method.WaitResult(forms);
  if (result == GNOME_KEYRING_RESULT_NO_MATCH)
    return true;
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
  // Successful read of actual data. Try migration if necessary.
  if (!migrate_tried_)
    MigrateToProfileSpecificLogins();
  return true;
}

bool NativeBackendGnome::GetLoginsCreatedBetween(const base::Time& get_begin,
                                                 const base::Time& get_end,
                                                 PasswordFormList* forms) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  // We could walk the list and add items as we find them, but it is much
  // easier to build the list and then filter the results.
  PasswordFormList all_forms;
  if (!GetAllLogins(&all_forms))
    return false;
  // No need to try migration here: GetAllLogins() does it.

  forms->reserve(forms->size() + all_forms.size());
  for (size_t i = 0; i < all_forms.size(); ++i) {
    if (get_begin <= all_forms[i]->date_created &&
        (get_end.is_null() || all_forms[i]->date_created < get_end)) {
      forms->push_back(all_forms[i]);
    } else {
      delete all_forms[i];
    }
  }

  return true;
}

bool NativeBackendGnome::GetAutofillableLogins(PasswordFormList* forms) {
  return GetLoginsList(forms, true);
}

bool NativeBackendGnome::GetBlacklistLogins(PasswordFormList* forms) {
  return GetLoginsList(forms, false);
}

bool NativeBackendGnome::GetLoginsList(PasswordFormList* forms,
                                       bool autofillable) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  uint32_t blacklisted_by_user = !autofillable;

  GKRMethod method;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&GKRMethod::GetLoginsList,
                                     base::Unretained(&method),
                                     blacklisted_by_user, app_string_.c_str()));
  GnomeKeyringResult result = method.WaitResult(forms);
  if (result == GNOME_KEYRING_RESULT_NO_MATCH)
    return true;
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
  // Successful read of actual data. Try migration if necessary.
  if (!migrate_tried_)
    MigrateToProfileSpecificLogins();
  return true;
}

bool NativeBackendGnome::GetAllLogins(PasswordFormList* forms) {
  GKRMethod method;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&GKRMethod::GetAllLogins,
                                     base::Unretained(&method),
                                     app_string_.c_str()));
  GnomeKeyringResult result = method.WaitResult(forms);
  if (result == GNOME_KEYRING_RESULT_NO_MATCH)
    return true;
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
  // Successful read of actual data. Try migration if necessary.
  if (!migrate_tried_)
    MigrateToProfileSpecificLogins();
  return true;
}

std::string NativeBackendGnome::GetProfileSpecificAppString() const {
  // Originally, the application string was always just "chrome" and used only
  // so that we had *something* to search for since GNOME Keyring won't search
  // for nothing. Now we use it to distinguish passwords for different profiles.
  return base::StringPrintf("%s-%d", kGnomeKeyringAppString, profile_id_);
}

void NativeBackendGnome::MigrateToProfileSpecificLogins() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  DCHECK(!migrate_tried_);
  DCHECK_EQ(app_string_, kGnomeKeyringAppString);

  // Record the fact that we've attempted migration already right away, so that
  // we don't get recursive calls back to MigrateToProfileSpecificLogins().
  migrate_tried_ = true;

  // First get all the logins, using the old app string.
  PasswordFormList forms;
  if (!GetAllLogins(&forms))
    return;

  // Now switch to a profile-specific app string.
  app_string_ = GetProfileSpecificAppString();

  // Try to add all the logins with the new app string.
  bool ok = true;
  for (size_t i = 0; i < forms.size(); ++i) {
    if (!RawAddLogin(*forms[i]))
      ok = false;
    delete forms[i];
  }

  if (ok) {
    // All good! Keep the new app string and set a persistent pref.
    // NOTE: We explicitly don't delete the old passwords yet. They are
    // potentially shared with other profiles and other user data dirs!
    // Each other profile must be able to migrate the shared data as well,
    // so we must leave it alone. After a few releases, we'll add code to
    // delete them, and eventually remove this migration code.
    // TODO(mdm): follow through with the plan above.
    PasswordStoreX::SetPasswordsUseLocalProfileId(prefs_);
  } else {
    // We failed to migrate for some reason. Use the old app string.
    app_string_ = kGnomeKeyringAppString;
  }
}
