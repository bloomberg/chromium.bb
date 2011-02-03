// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/native_backend_gnome_x.h"

#include <dbus/dbus-glib.h>
#include <dlfcn.h>
#include <gnome-keyring.h>

#include <map>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/browser_thread.h"

using webkit_glue::PasswordForm;

namespace {

// Many of the gnome_keyring_* functions use variable arguments, which makes
// them difficult if not impossible to wrap in C. Therefore, we want the
// actual uses below to either call the functions directly (if we are linking
// against libgnome-keyring), or call them via appropriately-typed function
// pointers (if we are dynamically loading libgnome-keyring).

// Thus, instead of making a wrapper class with two implementations, we use
// the preprocessor to rename the calls below in the dynamic load case, and
// provide a function to initialize a set of function pointers that have the
// alternate names. We also make sure the types are correct, since otherwise
// dynamic loading like this would leave us vulnerable to signature changes.

#if defined(DLOPEN_GNOME_KEYRING)

// Call a given parameter with the name of each function we use from GNOME
// Keyring.
#define GNOME_KEYRING_FOR_EACH_FUNC(F)          \
  F(is_available)                               \
  F(store_password)                             \
  F(delete_password)                            \
  F(find_itemsv)                                \
  F(result_to_message)                          \
  F(list_item_ids)                              \
  F(item_get_attributes)                        \
  F(item_get_info)                              \
  F(item_info_get_secret)

// Define the actual function pointers that we'll use in application code.
#define GNOME_KEYRING_DEFINE_WRAPPER(name) \
  typeof(&gnome_keyring_##name) wrap_gnome_keyring_##name;
GNOME_KEYRING_FOR_EACH_FUNC(GNOME_KEYRING_DEFINE_WRAPPER)
#undef GNOME_KEYRING_DEFINE_WRAPPER

// Make it easy to initialize the function pointers above with a loop below.
#define GNOME_KEYRING_FUNCTION(name) \
  {"gnome_keyring_"#name, reinterpret_cast<void**>(&wrap_gnome_keyring_##name)},
const struct {
  const char* name;
  void** pointer;
} gnome_keyring_functions[] = {
  GNOME_KEYRING_FOR_EACH_FUNC(GNOME_KEYRING_FUNCTION)
  {NULL, NULL}
};
#undef GNOME_KEYRING_FUNCTION

#undef GNOME_KEYRING_FOR_EACH_FUNC

// Allow application code below to use the normal function names, but actually
// end up using the function pointers above instead.
#define gnome_keyring_is_available \
    wrap_gnome_keyring_is_available
#define gnome_keyring_store_password \
    wrap_gnome_keyring_store_password
#define gnome_keyring_delete_password \
    wrap_gnome_keyring_delete_password
#define gnome_keyring_find_itemsv \
    wrap_gnome_keyring_find_itemsv
#define gnome_keyring_result_to_message \
    wrap_gnome_keyring_result_to_message
#define gnome_keyring_list_item_ids \
    wrap_gnome_keyring_list_item_ids
#define gnome_keyring_item_get_attributes \
  wrap_gnome_keyring_item_get_attributes
#define gnome_keyring_item_get_info \
  wrap_gnome_keyring_item_get_info
#define gnome_keyring_item_info_get_secret \
  wrap_gnome_keyring_item_info_get_secret

/* Load the library and initialize the function pointers. */
bool LoadGnomeKeyring() {
  void* handle = dlopen("libgnome-keyring.so.0", RTLD_NOW | RTLD_GLOBAL);
  if (!handle) {
    // We wanted to use GNOME Keyring, but we couldn't load it. Warn, because
    // either the user asked for this, or we autodetected it incorrectly. (Or
    // the system has broken libraries, which is also good to warn about.)
    LOG(WARNING) << "Could not load libgnome-keyring.so.0: " << dlerror();
    return false;
  }
  for (size_t i = 0; gnome_keyring_functions[i].name; ++i) {
    dlerror();
    *gnome_keyring_functions[i].pointer =
        dlsym(handle, gnome_keyring_functions[i].name);
    const char* error = dlerror();
    if (error) {
      LOG(ERROR) << "Unable to load symbol "
                 << gnome_keyring_functions[i].name << ": " << error;
      dlclose(handle);
      return false;
    }
  }
  // We leak the library handle. That's OK: this function is called only once.
  return true;
}

// Older versions of GNOME Keyring have bugs that prevent them from working
// correctly with the find_itemsv API. (In particular, the non-pageable memory
// allocator is rather busted.) There is no official way to check the version,
// nor could we figure out any reasonable unofficial way to do it. So we work
// around it by using a much slower API.
#define GNOME_KEYRING_WORK_AROUND_MEMORY_CORRUPTION

#else  // !defined(DLOPEN_GNOME_KEYRING)

bool LoadGnomeKeyring() {
  // We don't need to do anything here. When linking directly, we also assume
  // that whoever is compiling this code has checked that the version is OK.
  return true;
}

#endif  // !defined(DLOPEN_GNOME_KEYRING)

#define GNOME_KEYRING_APPLICATION_CHROME "chrome"

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
  if (string_attr_map["application"] != GNOME_KEYRING_APPLICATION_CHROME)
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
class GKRMethod {
 public:
  typedef NativeBackendGnome::PasswordFormList PasswordFormList;

  GKRMethod() : event_(false, false), result_(GNOME_KEYRING_RESULT_CANCELLED) {}

  // Action methods. These call gnome_keyring_* functions. Call from UI thread.
  void AddLogin(const PasswordForm& form);
  void UpdateLoginSearch(const PasswordForm& form);
  void RemoveLogin(const PasswordForm& form);
  void GetLogins(const PasswordForm& form);
#if !defined(GNOME_KEYRING_WORK_AROUND_MEMORY_CORRUPTION)
  void GetLoginsList(uint32_t blacklisted_by_user);
  void GetAllLogins();
#else
  void GetItemIds();
  void GetItemAttrs(guint id);
  void GetItemInfo(guint id);
#endif

  // Use after AddLogin, RemoveLogin.
  GnomeKeyringResult WaitResult();

  // Use after UpdateLoginSearch, GetLogins, GetLoginsList, GetAllLogins.
  GnomeKeyringResult WaitResult(PasswordFormList* forms);

#if defined(GNOME_KEYRING_WORK_AROUND_MEMORY_CORRUPTION)
  // Use after GetItemIds().
  GnomeKeyringResult WaitResult(std::vector<guint>* item_ids);

  // Use after GetItemAttrs().
  GnomeKeyringResult WaitResult(PasswordForm** form);

  // Use after GetItemInfo().
  GnomeKeyringResult WaitResult(string16* password);
#endif

 private:
  // All these callbacks are called on UI thread.
  static void OnOperationDone(GnomeKeyringResult result, gpointer data);

  static void OnOperationGetList(GnomeKeyringResult result, GList* list,
                                 gpointer data);

#if defined(GNOME_KEYRING_WORK_AROUND_MEMORY_CORRUPTION)
  static void OnOperationGetIds(GnomeKeyringResult result, GList* list,
                                gpointer data);

  static void OnOperationGetAttrs(GnomeKeyringResult result,
                                  GnomeKeyringAttributeList* attrs,
                                  gpointer data);

  static void OnOperationGetInfo(GnomeKeyringResult result,
                                 GnomeKeyringItemInfo* info,
                                 gpointer data);
#endif

  base::WaitableEvent event_;
  GnomeKeyringResult result_;
  NativeBackendGnome::PasswordFormList forms_;
#if defined(GNOME_KEYRING_WORK_AROUND_MEMORY_CORRUPTION)
  std::vector<guint> item_ids_;
  scoped_ptr<PasswordForm> form_;
  string16 password_;
#endif
};

void GKRMethod::AddLogin(const PasswordForm& form) {
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
      "application", GNOME_KEYRING_APPLICATION_CHROME,
      NULL);
}

void GKRMethod::UpdateLoginSearch(const PasswordForm& form) {
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
      GNOME_KEYRING_APPLICATION_CHROME,
      NULL);
}

void GKRMethod::RemoveLogin(const PasswordForm& form) {
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
      NULL);
}

void GKRMethod::GetLogins(const PasswordForm& form) {
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
      GNOME_KEYRING_APPLICATION_CHROME,
      NULL);
}

#if !defined(GNOME_KEYRING_WORK_AROUND_MEMORY_CORRUPTION)
void GKRMethod::GetLoginsList(uint32_t blacklisted_by_user) {
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
      GNOME_KEYRING_APPLICATION_CHROME,
      NULL);
}

void GKRMethod::GetAllLogins() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // We need to search for something, otherwise we get no results - so
  // we search for the fixed application string.
  gnome_keyring_find_itemsv(
      GNOME_KEYRING_ITEM_GENERIC_SECRET,
      OnOperationGetList,
      this,  // data
      NULL,  // destroy_data
      "application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      GNOME_KEYRING_APPLICATION_CHROME,
      NULL);
}
#else
void GKRMethod::GetItemIds() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  gnome_keyring_list_item_ids(NULL, OnOperationGetIds, this, NULL);
}

void GKRMethod::GetItemAttrs(guint id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  gnome_keyring_item_get_attributes(NULL, id, OnOperationGetAttrs, this, NULL);
}

void GKRMethod::GetItemInfo(guint id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  gnome_keyring_item_get_info(NULL, id, OnOperationGetInfo, this, NULL);
}
#endif

GnomeKeyringResult GKRMethod::WaitResult() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  event_.Wait();
  return result_;
}

GnomeKeyringResult GKRMethod::WaitResult(PasswordFormList* forms) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  event_.Wait();
  forms->swap(forms_);
  return result_;
}

#if defined(GNOME_KEYRING_WORK_AROUND_MEMORY_CORRUPTION)
GnomeKeyringResult GKRMethod::WaitResult(std::vector<guint>* item_ids) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  event_.Wait();
  item_ids->swap(item_ids_);
  return result_;
}

GnomeKeyringResult GKRMethod::WaitResult(PasswordForm** form) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  event_.Wait();
  *form = form_.release();
  return result_;
}

GnomeKeyringResult GKRMethod::WaitResult(string16* password) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  event_.Wait();
  *password = password_;
  return result_;
}
#endif

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

#if defined(GNOME_KEYRING_WORK_AROUND_MEMORY_CORRUPTION)
// static
void GKRMethod::OnOperationGetIds(GnomeKeyringResult result, GList* list,
                                  gpointer data) {
  GKRMethod* method = static_cast<GKRMethod*>(data);
  method->result_ = result;
  method->item_ids_.clear();
  // |list| will be freed after this callback returns, so save it now.
  for (GList* i = list; i; i = i->next) {
    guint id = GPOINTER_TO_UINT(i->data);
    method->item_ids_.push_back(id);
  }
  method->event_.Signal();
}

// static
void GKRMethod::OnOperationGetAttrs(GnomeKeyringResult result,
                                    GnomeKeyringAttributeList* attrs,
                                    gpointer data) {
  GKRMethod* method = static_cast<GKRMethod*>(data);
  method->result_ = result;
  // |attrs| will be freed after this callback returns, so convert it now.
  if (result == GNOME_KEYRING_RESULT_OK)
    method->form_.reset(FormFromAttributes(attrs));
  method->event_.Signal();
}

// static
void GKRMethod::OnOperationGetInfo(GnomeKeyringResult result,
                                   GnomeKeyringItemInfo* info,
                                   gpointer data) {
  GKRMethod* method = static_cast<GKRMethod*>(data);
  method->result_ = result;
  // |info| will be freed after this callback returns, so use it now.
  if (result == GNOME_KEYRING_RESULT_OK) {
    char* secret = gnome_keyring_item_info_get_secret(info);
    if (secret) {
      method->password_ = UTF8ToUTF16(secret);
      // gnome_keyring_item_info_get_secret() allocates and returns a new copy
      // of the secret, so we have to free it afterward.
      free(secret);
    } else {
      LOG(WARNING) << "Unable to access password from item info!";
    }
  }
  method->event_.Signal();
}
#endif

}  // namespace

// GKRMethod isn't reference counted, but it always outlasts runnable
// methods against it because the caller waits for those methods to run.
template<>
struct RunnableMethodTraits<GKRMethod> {
  void RetainCallee(GKRMethod*) {}
  void ReleaseCallee(GKRMethod*) {}
};

NativeBackendGnome::NativeBackendGnome() {
}

NativeBackendGnome::~NativeBackendGnome() {
}

bool NativeBackendGnome::Init() {
  return LoadGnomeKeyring() && gnome_keyring_is_available();
}

bool NativeBackendGnome::AddLogin(const PasswordForm& form) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  GKRMethod method;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          NewRunnableMethod(&method,
                                            &GKRMethod::AddLogin,
                                            form));
  GnomeKeyringResult result = method.WaitResult();
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring save failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
  return true;
}

bool NativeBackendGnome::UpdateLogin(const PasswordForm& form) {
  // Based on LoginDatabase::UpdateLogin(), we search for forms to update by
  // origin_url, username_element, username_value, password_element, and
  // signon_realm. We then compare the result to the updated form. If they
  // differ in any of the action, password_value, ssl_valid, or preferred
  // fields, then we add a new login with those fields updated and only delete
  // the original on success.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  GKRMethod method;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                         NewRunnableMethod(&method,
                                           &GKRMethod::UpdateLoginSearch,
                                           form));
  PasswordFormList forms;
  GnomeKeyringResult result = method.WaitResult(&forms);
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
  bool ok = true;
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
      if (AddLogin(updated))
        RemoveLogin(*forms[i]);
      else
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
                          NewRunnableMethod(&method,
                                            &GKRMethod::RemoveLogin,
                                            form));
  GnomeKeyringResult result = method.WaitResult();
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring delete failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
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
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(&method, &GKRMethod::GetLogins, form));
  GnomeKeyringResult result = method.WaitResult(forms);
  if (result == GNOME_KEYRING_RESULT_NO_MATCH)
    return true;
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
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

#if !defined(GNOME_KEYRING_WORK_AROUND_MEMORY_CORRUPTION)
  GKRMethod method;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          NewRunnableMethod(&method,
                                            &GKRMethod::GetLoginsList,
                                            blacklisted_by_user));
  GnomeKeyringResult result = method.WaitResult(forms);
  if (result == GNOME_KEYRING_RESULT_NO_MATCH)
    return true;
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
  return true;
#else
  PasswordFormList all_forms;
  if (!GetAllLogins(&all_forms))
    return false;
  // Now manually filter the results for the values we care about.
  for (size_t i = 0; i < all_forms.size(); ++i) {
    if (all_forms[i]->blacklisted_by_user == blacklisted_by_user)
      forms->push_back(all_forms[i]);
    else
      delete all_forms[i];
  }
  return true;
#endif
}

bool NativeBackendGnome::GetAllLogins(PasswordFormList* forms) {
  GKRMethod method;
#if !defined(GNOME_KEYRING_WORK_AROUND_MEMORY_CORRUPTION)
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          NewRunnableMethod(&method,
                                            &GKRMethod::GetAllLogins));
  GnomeKeyringResult result = method.WaitResult(forms);
  if (result == GNOME_KEYRING_RESULT_NO_MATCH)
    return true;
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
  return true;
#else
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          NewRunnableMethod(&method,
                                            &GKRMethod::GetItemIds));
  std::vector<guint> item_ids;
  GnomeKeyringResult result = method.WaitResult(&item_ids);
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring itemid list failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }

  // We can parallelize getting the item attributes.
  GKRMethod* methods = new GKRMethod[item_ids.size()];
  for (size_t i = 0; i < item_ids.size(); ++i) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            NewRunnableMethod(&methods[i],
                                              &GKRMethod::GetItemAttrs,
                                              item_ids[i]));
  }

  bool success = true;

  // We can also parallelize getting the item info (i.e. passwords).
  PasswordFormList all_forms;
  all_forms.resize(item_ids.size());
  for (size_t i = 0; i < item_ids.size(); ++i) {
    result = methods[i].WaitResult(&all_forms[i]);
    if (result != GNOME_KEYRING_RESULT_OK) {
      LOG(ERROR) << "Keyring get item attributes failed: "
                 << gnome_keyring_result_to_message(result);
      // We explicitly do not break out here. We must wait on all the other
      // methods first, and we may have already posted new methods. So, we just
      // note the failure and continue.
      success = false;
    }
    if (all_forms[i]) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              NewRunnableMethod(&methods[i],
                                                &GKRMethod::GetItemInfo,
                                                item_ids[i]));
    }
  }

  // Now just wait for all the passwords to come in.
  for (size_t i = 0; i < item_ids.size(); ++i) {
    if (!all_forms[i])
      continue;
    result = methods[i].WaitResult(&all_forms[i]->password_value);
    if (result != GNOME_KEYRING_RESULT_OK) {
      LOG(ERROR) << "Keyring get item info failed: "
                 << gnome_keyring_result_to_message(result);
      delete all_forms[i];
      all_forms[i] = NULL;
      // We explicitly do not break out here (see above).
      success = false;
    }
  }

  delete[] methods;

  if (success) {
    // If we succeeded, output all the forms.
    for (size_t i = 0; i < item_ids.size(); ++i) {
      if (all_forms[i])
        forms->push_back(all_forms[i]);
    }
  } else {
    // Otherwise, free them.
    for (size_t i = 0; i < item_ids.size(); ++i)
      delete all_forms[i];
  }

  return success;
#endif
}
