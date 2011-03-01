// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/native_backend_kwallet_x.h"

#include <sstream>

#include "base/logging.h"
#include "base/pickle.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "content/browser/browser_thread.h"

using std::string;
using std::vector;
using webkit_glue::PasswordForm;

// We could localize these strings, but then changing your locale would cause
// you to lose access to all your stored passwords. Maybe best not to do that.
const char* NativeBackendKWallet::kAppId = "Chrome";
const char* NativeBackendKWallet::kKWalletFolder = "Chrome Form Data";

const char* NativeBackendKWallet::kKWalletServiceName = "org.kde.kwalletd";
const char* NativeBackendKWallet::kKWalletPath = "/modules/kwalletd";
const char* NativeBackendKWallet::kKWalletInterface = "org.kde.KWallet";
const char* NativeBackendKWallet::kKLauncherServiceName = "org.kde.klauncher";
const char* NativeBackendKWallet::kKLauncherPath = "/KLauncher";
const char* NativeBackendKWallet::kKLauncherInterface = "org.kde.KLauncher";

NativeBackendKWallet::NativeBackendKWallet()
    : error_(NULL),
      connection_(NULL),
      proxy_(NULL) {
}

NativeBackendKWallet::~NativeBackendKWallet() {
  if (proxy_)
    g_object_unref(proxy_);
}

bool NativeBackendKWallet::Init() {
  // Get a connection to the session bus.
  connection_ = dbus_g_bus_get(DBUS_BUS_SESSION, &error_);
  if (CheckError())
    return false;

  if (!InitWallet()) {
    // kwalletd may not be running. Try to start it and try again.
    if (!StartKWalletd() || !InitWallet())
      return false;
  }

  return true;
}

bool NativeBackendKWallet::StartKWalletd() {
  // Sadly kwalletd doesn't use DBUS activation, so we have to make a call to
  // klauncher to start it.
  DBusGProxy* klauncher_proxy =
      dbus_g_proxy_new_for_name(connection_, kKLauncherServiceName,
                                kKLauncherPath, kKLauncherInterface);

  char* empty_string_list = NULL;
  int ret = 1;
  char* error = NULL;
  dbus_g_proxy_call(klauncher_proxy, "start_service_by_desktop_name", &error_,
                    G_TYPE_STRING,  "kwalletd",          // serviceName
                    G_TYPE_STRV,    &empty_string_list,  // urls
                    G_TYPE_STRV,    &empty_string_list,  // envs
                    G_TYPE_STRING,  "",                  // startup_id
                    G_TYPE_BOOLEAN, (gboolean) false,    // blind
                    G_TYPE_INVALID,
                    G_TYPE_INT,     &ret,                // result
                    G_TYPE_STRING,  NULL,                // dubsName
                    G_TYPE_STRING,  &error,              // error
                    G_TYPE_INT,     NULL,                // pid
                    G_TYPE_INVALID);

  if (error && *error) {
    LOG(ERROR) << "Error launching kwalletd: " << error;
    ret = 1;  // Make sure we return false after freeing.
  }

  g_free(error);
  g_object_unref(klauncher_proxy);

  if (CheckError() || ret != 0)
    return false;
  return true;
}

bool NativeBackendKWallet::InitWallet() {
  // Make a proxy to KWallet.
  proxy_ = dbus_g_proxy_new_for_name(connection_, kKWalletServiceName,
                                     kKWalletPath, kKWalletInterface);

  // Check KWallet is enabled.
  gboolean is_enabled = false;
  dbus_g_proxy_call(proxy_, "isEnabled", &error_,
                    G_TYPE_INVALID,
                    G_TYPE_BOOLEAN, &is_enabled,
                    G_TYPE_INVALID);
  if (CheckError() || !is_enabled)
    return false;

  // Get the wallet name.
  char* wallet_name = NULL;
  dbus_g_proxy_call(proxy_, "networkWallet", &error_,
                    G_TYPE_INVALID,
                    G_TYPE_STRING, &wallet_name,
                    G_TYPE_INVALID);
  if (CheckError() || !wallet_name)
    return false;

  wallet_name_.assign(wallet_name);
  g_free(wallet_name);

  return true;
}

bool NativeBackendKWallet::AddLogin(const PasswordForm& form) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;

  PasswordFormList forms;
  GetLoginsList(&forms, form.signon_realm, wallet_handle);

  forms.push_back(new PasswordForm(form));
  bool ok = SetLoginsList(forms, form.signon_realm, wallet_handle);

  STLDeleteElements(&forms);
  return ok;
}

bool NativeBackendKWallet::UpdateLogin(const PasswordForm& form) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;

  PasswordFormList forms;
  GetLoginsList(&forms, form.signon_realm, wallet_handle);

  for (size_t i = 0; i < forms.size(); ++i) {
    if (CompareForms(form, *forms[i], true))
      *forms[i] = form;
  }

  bool ok = SetLoginsList(forms, form.signon_realm, wallet_handle);

  STLDeleteElements(&forms);
  return ok;
}

bool NativeBackendKWallet::RemoveLogin(const PasswordForm& form) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;

  PasswordFormList all_forms;
  GetLoginsList(&all_forms, form.signon_realm, wallet_handle);

  PasswordFormList kept_forms;
  kept_forms.reserve(all_forms.size());
  for (size_t i = 0; i < all_forms.size(); ++i) {
    if (CompareForms(form, *all_forms[i], false))
      delete all_forms[i];
    else
      kept_forms.push_back(all_forms[i]);
  }

  // Update the entry in the wallet, possibly deleting it.
  bool ok = SetLoginsList(kept_forms, form.signon_realm, wallet_handle);

  STLDeleteElements(&kept_forms);
  return ok;
}

bool NativeBackendKWallet::RemoveLoginsCreatedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;

  // We could probably also use readEntryList here.
  char** realm_list = NULL;
  dbus_g_proxy_call(proxy_, "entryList", &error_,
                    G_TYPE_INT,     wallet_handle,             // handle
                    G_TYPE_STRING,  kKWalletFolder,            // folder
                    G_TYPE_STRING,  kAppId,                    // appid
                    G_TYPE_INVALID,
                    G_TYPE_STRV,    &realm_list,
                    G_TYPE_INVALID);
  if (CheckError())
    return false;

  bool ok = true;
  for (char** realm = realm_list; *realm; ++realm) {
    GArray* byte_array = NULL;
    dbus_g_proxy_call(proxy_, "readEntry", &error_,
                      G_TYPE_INT,     wallet_handle,           // handle
                      G_TYPE_STRING,  kKWalletFolder,          // folder
                      G_TYPE_STRING,  *realm,                  // key
                      G_TYPE_STRING,  kAppId,                  // appid
                      G_TYPE_INVALID,
                      DBUS_TYPE_G_UCHAR_ARRAY, &byte_array,
                      G_TYPE_INVALID);

    if (CheckError() || !byte_array ||
        !CheckSerializedValue(byte_array, *realm)) {
      continue;
    }

    string signon_realm(*realm);
    Pickle pickle(byte_array->data, byte_array->len);
    PasswordFormList all_forms;
    DeserializeValue(signon_realm, pickle, &all_forms);
    g_array_free(byte_array, true);

    PasswordFormList kept_forms;
    kept_forms.reserve(all_forms.size());
    for (size_t i = 0; i < all_forms.size(); ++i) {
      if (delete_begin <= all_forms[i]->date_created &&
          (delete_end.is_null() || all_forms[i]->date_created < delete_end)) {
        delete all_forms[i];
      } else {
        kept_forms.push_back(all_forms[i]);
      }
    }

    if (!SetLoginsList(kept_forms, signon_realm, wallet_handle))
      ok = false;
    STLDeleteElements(&kept_forms);
  }
  g_strfreev(realm_list);
  return ok;
}

bool NativeBackendKWallet::GetLogins(const PasswordForm& form,
                                     PasswordFormList* forms) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;
  return GetLoginsList(forms, form.signon_realm, wallet_handle);
}

bool NativeBackendKWallet::GetLoginsCreatedBetween(const base::Time& get_begin,
                                                   const base::Time& get_end,
                                                   PasswordFormList* forms) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;
  return GetLoginsList(forms, get_begin, get_end, wallet_handle);
}

bool NativeBackendKWallet::GetAutofillableLogins(PasswordFormList* forms) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;
  return GetLoginsList(forms, true, wallet_handle);
}

bool NativeBackendKWallet::GetBlacklistLogins(PasswordFormList* forms) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;
  return GetLoginsList(forms, false, wallet_handle);
}

bool NativeBackendKWallet::GetLoginsList(PasswordFormList* forms,
                                         const string& signon_realm,
                                         int wallet_handle) {
  // Is there an entry in the wallet?
  gboolean has_entry = false;
  dbus_g_proxy_call(proxy_, "hasEntry", &error_,
                    G_TYPE_INT,     wallet_handle,         // handle
                    G_TYPE_STRING,  kKWalletFolder,        // folder
                    G_TYPE_STRING,  signon_realm.c_str(),  // key
                    G_TYPE_STRING,  kAppId,                // appid
                    G_TYPE_INVALID,
                    G_TYPE_BOOLEAN, &has_entry,
                    G_TYPE_INVALID);

  if (CheckError())
    return false;
  if (!has_entry) {
    // This is not an error. There just isn't a matching entry.
    return true;
  }

  GArray* byte_array = NULL;
  dbus_g_proxy_call(proxy_, "readEntry", &error_,
                    G_TYPE_INT,     wallet_handle,         // handle
                    G_TYPE_STRING,  kKWalletFolder,        // folder
                    G_TYPE_STRING,  signon_realm.c_str(),  // key
                    G_TYPE_STRING,  kAppId,                // appid
                    G_TYPE_INVALID,
                    DBUS_TYPE_G_UCHAR_ARRAY, &byte_array,
                    G_TYPE_INVALID);

  if (CheckError() || !byte_array)
    return false;
  if (!CheckSerializedValue(byte_array, signon_realm.c_str())) {
    // This is weird, but we choose not to call it an error. There's an invalid
    // entry somehow, but by pretending it just doesn't exist, we make it easier
    // to repair without having to delete it using kwalletmanager (that is, by
    // just saving a new password within this realm to overwrite it).
    g_array_free(byte_array, true);
    return true;
  }

  Pickle pickle(byte_array->data, byte_array->len);
  DeserializeValue(signon_realm, pickle, forms);
  g_array_free(byte_array, true);

  return true;
}

bool NativeBackendKWallet::GetLoginsList(PasswordFormList* forms,
                                         bool autofillable,
                                         int wallet_handle) {
  PasswordFormList all_forms;
  if (!GetAllLogins(&all_forms, wallet_handle))
    return false;

  // We have to read all the entries, and then filter them here.
  forms->reserve(forms->size() + all_forms.size());
  for (size_t i = 0; i < all_forms.size(); ++i) {
    if (all_forms[i]->blacklisted_by_user == !autofillable)
      forms->push_back(all_forms[i]);
    else
      delete all_forms[i];
  }

  return true;
}

bool NativeBackendKWallet::GetLoginsList(PasswordFormList* forms,
                                         const base::Time& begin,
                                         const base::Time& end,
                                         int wallet_handle) {
  PasswordFormList all_forms;
  if (!GetAllLogins(&all_forms, wallet_handle))
    return false;

  // We have to read all the entries, and then filter them here.
  forms->reserve(forms->size() + all_forms.size());
  for (size_t i = 0; i < all_forms.size(); ++i) {
    if (begin <= all_forms[i]->date_created &&
        (end.is_null() || all_forms[i]->date_created < end)) {
      forms->push_back(all_forms[i]);
    } else {
      delete all_forms[i];
    }
  }

  return true;
}

bool NativeBackendKWallet::GetAllLogins(PasswordFormList* forms,
                                        int wallet_handle) {
  // We could probably also use readEntryList here.
  char** realm_list = NULL;
  dbus_g_proxy_call(proxy_, "entryList", &error_,
                    G_TYPE_INT,     wallet_handle,             // handle
                    G_TYPE_STRING,  kKWalletFolder,            // folder
                    G_TYPE_STRING,  kAppId,                    // appid
                    G_TYPE_INVALID,
                    G_TYPE_STRV,    &realm_list,
                    G_TYPE_INVALID);
  if (CheckError())
    return false;

  for (char** realm = realm_list; *realm; ++realm) {
    GArray* byte_array = NULL;
    dbus_g_proxy_call(proxy_, "readEntry", &error_,
                      G_TYPE_INT,     wallet_handle,           // handle
                      G_TYPE_STRING,  kKWalletFolder,          // folder
                      G_TYPE_STRING,  *realm,                  // key
                      G_TYPE_STRING,  kAppId,                  // appid
                      G_TYPE_INVALID,
                      DBUS_TYPE_G_UCHAR_ARRAY, &byte_array,
                      G_TYPE_INVALID);

    if (CheckError() || !byte_array ||
        !CheckSerializedValue(byte_array, *realm)) {
      continue;
    }

    Pickle pickle(byte_array->data, byte_array->len);
    DeserializeValue(*realm, pickle, forms);
    g_array_free(byte_array, true);
  }
  g_strfreev(realm_list);
  return true;
}

bool NativeBackendKWallet::SetLoginsList(const PasswordFormList& forms,
                                         const string& signon_realm,
                                         int wallet_handle) {
  if (forms.empty()) {
    // No items left? Remove the entry from the wallet.
    int ret = 0;
    dbus_g_proxy_call(proxy_, "removeEntry", &error_,
                      G_TYPE_INT,     wallet_handle,         // handle
                      G_TYPE_STRING,  kKWalletFolder,        // folder
                      G_TYPE_STRING,  signon_realm.c_str(),  // key
                      G_TYPE_STRING,  kAppId,                // appid
                      G_TYPE_INVALID,
                      G_TYPE_INT,     &ret,
                      G_TYPE_INVALID);
    CheckError();
    if (ret != 0)
      LOG(ERROR) << "Bad return code " << ret << " from kwallet removeEntry";
    return ret == 0;
  }

  Pickle value;
  SerializeValue(forms, &value);

  // Convert the pickled bytes to a GByteArray.
  GArray* byte_array = g_array_sized_new(false, false, sizeof(char),
                                         value.size());
  g_array_append_vals(byte_array, value.data(), value.size());

  // Make the call.
  int ret = 0;
  dbus_g_proxy_call(proxy_, "writeEntry", &error_,
                    G_TYPE_INT,           wallet_handle,         // handle
                    G_TYPE_STRING,        kKWalletFolder,        // folder
                    G_TYPE_STRING,        signon_realm.c_str(),  // key
                    DBUS_TYPE_G_UCHAR_ARRAY, byte_array,         // value
                    G_TYPE_STRING,        kAppId,                // appid
                    G_TYPE_INVALID,
                    G_TYPE_INT,           &ret,
                    G_TYPE_INVALID);
  g_array_free(byte_array, true);

  CheckError();
  if (ret != 0)
    LOG(ERROR) << "Bad return code " << ret << " from kwallet writeEntry";
  return ret == 0;
}

bool NativeBackendKWallet::CompareForms(const PasswordForm& a,
                                        const PasswordForm& b,
                                        bool update_check) {
  // An update check doesn't care about the submit element.
  if (!update_check && a.submit_element != b.submit_element)
    return false;
  return a.origin           == b.origin &&
         a.password_element == b.password_element &&
         a.signon_realm     == b.signon_realm &&
         a.username_element == b.username_element &&
         a.username_value   == b.username_value;
}

void NativeBackendKWallet::SerializeValue(const PasswordFormList& forms,
                                          Pickle* pickle) {
  pickle->WriteInt(kPickleVersion);
  pickle->WriteSize(forms.size());
  for (PasswordFormList::const_iterator it = forms.begin() ;
       it != forms.end() ; ++it) {
    const PasswordForm* form = *it;
    pickle->WriteInt(form->scheme);
    pickle->WriteString(form->origin.spec());
    pickle->WriteString(form->action.spec());
    pickle->WriteString16(form->username_element);
    pickle->WriteString16(form->username_value);
    pickle->WriteString16(form->password_element);
    pickle->WriteString16(form->password_value);
    pickle->WriteString16(form->submit_element);
    pickle->WriteBool(form->ssl_valid);
    pickle->WriteBool(form->preferred);
    pickle->WriteBool(form->blacklisted_by_user);
    pickle->WriteInt64(form->date_created.ToTimeT());
  }
}

bool NativeBackendKWallet::CheckSerializedValue(const GArray* byte_array,
                                                const char* realm) {
  const Pickle::Header* header =
      reinterpret_cast<const Pickle::Header*>(byte_array->data);
  if (byte_array->len < sizeof(*header) ||
      header->payload_size > byte_array->len - sizeof(*header)) {
    LOG(WARNING) << "Invalid KWallet entry detected! (realm: " << realm << ")";
    return false;
  }
  return true;
}

void NativeBackendKWallet::DeserializeValue(const string& signon_realm,
                                            const Pickle& pickle,
                                            PasswordFormList* forms) {
  void* iter = NULL;

  int version = -1;
  pickle.ReadInt(&iter, &version);
  if (version != kPickleVersion) {
    // This is the only version so far, so anything else is an error.
    return;
  }

  size_t count = 0;
  pickle.ReadSize(&iter, &count);

  forms->reserve(forms->size() + count);
  for (size_t i = 0; i < count; ++i) {
    PasswordForm* form = new PasswordForm();
    form->signon_realm.assign(signon_realm);

    int scheme = 0;
    pickle.ReadInt(&iter, &scheme);
    form->scheme = static_cast<PasswordForm::Scheme>(scheme);
    ReadGURL(pickle, &iter, &form->origin);
    ReadGURL(pickle, &iter, &form->action);
    pickle.ReadString16(&iter, &form->username_element);
    pickle.ReadString16(&iter, &form->username_value);
    pickle.ReadString16(&iter, &form->password_element);
    pickle.ReadString16(&iter, &form->password_value);
    pickle.ReadString16(&iter, &form->submit_element);
    pickle.ReadBool(&iter, &form->ssl_valid);
    pickle.ReadBool(&iter, &form->preferred);
    pickle.ReadBool(&iter, &form->blacklisted_by_user);
    int64 date_created = 0;
    pickle.ReadInt64(&iter, &date_created);
    form->date_created = base::Time::FromTimeT(date_created);
    forms->push_back(form);
  }
}

void NativeBackendKWallet::ReadGURL(const Pickle& pickle, void** iter,
                                    GURL* url) {
  string url_string;
  if (!pickle.ReadString(iter, &url_string)) {
    LOG(ERROR) << "Failed to read url string";
    *url = GURL();
    return;
  }
  *url = GURL(url_string);
}

bool NativeBackendKWallet::CheckError() {
  if (error_) {
    LOG(ERROR) << "Failed to complete KWallet call: " << error_->message;
    g_error_free(error_);
    error_ = NULL;
    return true;
  }
  return false;
}

int NativeBackendKWallet::WalletHandle() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  // Open the wallet.
  int handle = kInvalidKWalletHandle;
  dbus_g_proxy_call(proxy_, "open", &error_,
                    G_TYPE_STRING, wallet_name_.c_str(),  // wallet
                    G_TYPE_INT64,  0LL,                   // wid
                    G_TYPE_STRING, kAppId,                // appid
                    G_TYPE_INVALID,
                    G_TYPE_INT,    &handle,
                    G_TYPE_INVALID);
  if (CheckError() || handle == kInvalidKWalletHandle)
    return kInvalidKWalletHandle;

  // Check if our folder exists.
  gboolean has_folder = false;
  dbus_g_proxy_call(proxy_, "hasFolder", &error_,
                    G_TYPE_INT,    handle,          // handle
                    G_TYPE_STRING, kKWalletFolder,  // folder
                    G_TYPE_STRING, kAppId,          // appid
                    G_TYPE_INVALID,
                    G_TYPE_BOOLEAN, &has_folder,
                    G_TYPE_INVALID);
  if (CheckError())
    return kInvalidKWalletHandle;

  // Create it if it didn't.
  if (!has_folder) {
    gboolean success = false;
    dbus_g_proxy_call(proxy_, "createFolder", &error_,
                      G_TYPE_INT,    handle,          // handle
                      G_TYPE_STRING, kKWalletFolder,  // folder
                      G_TYPE_STRING, kAppId,          // appid
                      G_TYPE_INVALID,
                      G_TYPE_BOOLEAN, &success,
                      G_TYPE_INVALID);
    if (CheckError() || !success)
      return kInvalidKWalletHandle;
  }

  return handle;
}
