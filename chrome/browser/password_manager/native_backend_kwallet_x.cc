// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/native_backend_kwallet_x.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/grit/chromium_strings.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "ui/base/l10n/l10n_util.h"

using autofill::PasswordForm;
using content::BrowserThread;

namespace {

// We could localize this string, but then changing your locale would cause
// you to lose access to all your stored passwords. Maybe best not to do that.
// Name of the folder to store passwords in.
const char kKWalletFolder[] = "Chrome Form Data";

// DBus service, path, and interface names for klauncher and kwalletd.
const char kKWalletServiceName[] = "org.kde.kwalletd";
const char kKWalletPath[] = "/modules/kwalletd";
const char kKWalletInterface[] = "org.kde.KWallet";
const char kKLauncherServiceName[] = "org.kde.klauncher";
const char kKLauncherPath[] = "/KLauncher";
const char kKLauncherInterface[] = "org.kde.KLauncher";

// Compares two PasswordForms and returns true if they are the same.
// If |update_check| is false, we only check the fields that are checked by
// LoginDatabase::UpdateLogin() when updating logins; otherwise, we check the
// fields that are checked by LoginDatabase::RemoveLogin() for removing them.
bool CompareForms(const autofill::PasswordForm& a,
                  const autofill::PasswordForm& b,
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

// Checks a serialized list of PasswordForms for sanity. Returns true if OK.
// Note that |realm| is only used for generating a useful warning message.
bool CheckSerializedValue(const uint8_t* byte_array,
                          size_t length,
                          const std::string& realm) {
  const Pickle::Header* header =
      reinterpret_cast<const Pickle::Header*>(byte_array);
  if (length < sizeof(*header) ||
      header->payload_size > length - sizeof(*header)) {
    LOG(WARNING) << "Invalid KWallet entry detected (realm: " << realm << ")";
    return false;
  }
  return true;
}

// Convenience function to read a GURL from a Pickle. Assumes the URL has
// been written as a UTF-8 string. Returns true on success.
bool ReadGURL(PickleIterator* iter, bool warn_only, GURL* url) {
  std::string url_string;
  if (!iter->ReadString(&url_string)) {
    if (!warn_only)
      LOG(ERROR) << "Failed to deserialize URL.";
    *url = GURL();
    return false;
  }
  *url = GURL(url_string);
  return true;
}

void LogDeserializationWarning(int version,
                               std::string signon_realm,
                               bool warn_only) {
  if (warn_only) {
    LOG(WARNING) << "Failed to deserialize version " << version
                 << " KWallet entry (realm: " << signon_realm
                 << ") with native architecture size; will try alternate "
                 << "size.";
  } else {
    LOG(ERROR) << "Failed to deserialize version " << version
               << " KWallet entry (realm: " << signon_realm << ")";
  }
}

}  // namespace

NativeBackendKWallet::NativeBackendKWallet(LocalProfileId id)
    : profile_id_(id),
      kwallet_proxy_(NULL),
      app_name_(l10n_util::GetStringUTF8(IDS_PRODUCT_NAME)) {
  folder_name_ = GetProfileSpecificFolderName();
}

NativeBackendKWallet::~NativeBackendKWallet() {
  // This destructor is called on the thread that is destroying the Profile
  // containing the PasswordStore that owns this NativeBackend. Generally that
  // won't be the DB thread; it will be the UI thread. So we post a message to
  // shut it down on the DB thread, and it will be destructed afterward when the
  // scoped_refptr<dbus::Bus> goes out of scope. The NativeBackend will be
  // destroyed before that occurs, but that's OK.
  if (session_bus_.get()) {
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
                            base::Bind(&dbus::Bus::ShutdownAndBlock,
                                       session_bus_.get()));
  }
}

bool NativeBackendKWallet::Init() {
  // Without the |optional_bus| parameter, a real bus will be instantiated.
  return InitWithBus(scoped_refptr<dbus::Bus>());
}

bool NativeBackendKWallet::InitWithBus(scoped_refptr<dbus::Bus> optional_bus) {
  // We must synchronously do a few DBus calls to figure out if initialization
  // succeeds, but later, we'll want to do most work on the DB thread. So we
  // have to do the initialization on the DB thread here too, and wait for it.
  bool success = false;
  base::WaitableEvent event(false, false);
  // NativeBackendKWallet isn't reference counted, but we wait for InitWithBus
  // to finish, so we can safely use base::Unretained here.
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
                          base::Bind(&NativeBackendKWallet::InitOnDBThread,
                                     base::Unretained(this),
                                     optional_bus, &event, &success));

  // This ScopedAllowWait should not be here. http://crbug.com/125331
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  event.Wait();
  return success;
}

void NativeBackendKWallet::InitOnDBThread(scoped_refptr<dbus::Bus> optional_bus,
                                          base::WaitableEvent* event,
                                          bool* success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  DCHECK(!session_bus_.get());
  if (optional_bus.get()) {
    // The optional_bus parameter is given when this method is called in tests.
    session_bus_ = optional_bus;
  } else {
    // Get a (real) connection to the session bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SESSION;
    options.connection_type = dbus::Bus::PRIVATE;
    session_bus_ = new dbus::Bus(options);
  }
  kwallet_proxy_ =
      session_bus_->GetObjectProxy(kKWalletServiceName,
                                   dbus::ObjectPath(kKWalletPath));
  // kwalletd may not be running. If we get a temporary failure initializing it,
  // try to start it and then try again. (Note the short-circuit evaluation.)
  const InitResult result = InitWallet();
  *success = (result == INIT_SUCCESS ||
              (result == TEMPORARY_FAIL &&
               StartKWalletd() && InitWallet() == INIT_SUCCESS));
  event->Signal();
}

bool NativeBackendKWallet::StartKWalletd() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  // Sadly kwalletd doesn't use DBus activation, so we have to make a call to
  // klauncher to start it.
  dbus::ObjectProxy* klauncher =
      session_bus_->GetObjectProxy(kKLauncherServiceName,
                                   dbus::ObjectPath(kKLauncherPath));

  dbus::MethodCall method_call(kKLauncherInterface,
                               "start_service_by_desktop_name");
  dbus::MessageWriter builder(&method_call);
  std::vector<std::string> empty;
  builder.AppendString("kwalletd");     // serviceName
  builder.AppendArrayOfStrings(empty);  // urls
  builder.AppendArrayOfStrings(empty);  // envs
  builder.AppendString(std::string());  // startup_id
  builder.AppendBool(false);            // blind
  scoped_ptr<dbus::Response> response(
      klauncher->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response.get()) {
    LOG(ERROR) << "Error contacting klauncher to start kwalletd";
    return false;
  }
  dbus::MessageReader reader(response.get());
  int32_t ret = -1;
  std::string dbus_name;
  std::string error;
  int32_t pid = -1;
  if (!reader.PopInt32(&ret) || !reader.PopString(&dbus_name) ||
      !reader.PopString(&error) || !reader.PopInt32(&pid)) {
    LOG(ERROR) << "Error reading response from klauncher to start kwalletd: "
               << response->ToString();
    return false;
  }
  if (!error.empty() || ret) {
    LOG(ERROR) << "Error launching kwalletd: error '" << error << "' "
               << " (code " << ret << ")";
    return false;
  }

  return true;
}

NativeBackendKWallet::InitResult NativeBackendKWallet::InitWallet() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  {
    // Check that KWallet is enabled.
    dbus::MethodCall method_call(kKWalletInterface, "isEnabled");
    scoped_ptr<dbus::Response> response(
        kwallet_proxy_->CallMethodAndBlock(
            &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
    if (!response.get()) {
      LOG(ERROR) << "Error contacting kwalletd (isEnabled)";
      return TEMPORARY_FAIL;
    }
    dbus::MessageReader reader(response.get());
    bool enabled = false;
    if (!reader.PopBool(&enabled)) {
      LOG(ERROR) << "Error reading response from kwalletd (isEnabled): "
                 << response->ToString();
      return PERMANENT_FAIL;
    }
    // Not enabled? Don't use KWallet. But also don't warn here.
    if (!enabled) {
      VLOG(1) << "kwalletd reports that KWallet is not enabled.";
      return PERMANENT_FAIL;
    }
  }

  {
    // Get the wallet name.
    dbus::MethodCall method_call(kKWalletInterface, "networkWallet");
    scoped_ptr<dbus::Response> response(
        kwallet_proxy_->CallMethodAndBlock(
            &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
    if (!response.get()) {
      LOG(ERROR) << "Error contacting kwalletd (networkWallet)";
      return TEMPORARY_FAIL;
    }
    dbus::MessageReader reader(response.get());
    if (!reader.PopString(&wallet_name_)) {
      LOG(ERROR) << "Error reading response from kwalletd (networkWallet): "
                 << response->ToString();
      return PERMANENT_FAIL;
    }
  }

  return INIT_SUCCESS;
}

password_manager::PasswordStoreChangeList NativeBackendKWallet::AddLogin(
    const PasswordForm& form) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return password_manager::PasswordStoreChangeList();

  ScopedVector<autofill::PasswordForm> forms;
  GetLoginsList(&forms.get(), form.signon_realm, wallet_handle);

  // We search for a login to update, rather than unconditionally appending the
  // login, because in some cases (especially involving sync) we can be asked to
  // add a login that already exists. In these cases we want to just update.
  bool updated = false;
  password_manager::PasswordStoreChangeList changes;
  for (size_t i = 0; i < forms.size(); ++i) {
    // Use the more restrictive removal comparison, so that we never have
    // duplicate logins that would all be removed together by RemoveLogin().
    if (CompareForms(form, *forms[i], false)) {
      changes.push_back(password_manager::PasswordStoreChange(
          password_manager::PasswordStoreChange::REMOVE, *forms[i]));
      *forms[i] = form;
      updated = true;
    }
  }
  if (!updated)
    forms.push_back(new PasswordForm(form));
  changes.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::ADD, form));

  bool ok = SetLoginsList(forms.get(), form.signon_realm, wallet_handle);
  if (!ok)
    changes.clear();

  return changes;
}

bool NativeBackendKWallet::UpdateLogin(
    const PasswordForm& form,
    password_manager::PasswordStoreChangeList* changes) {
  DCHECK(changes);
  changes->clear();
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;

  ScopedVector<autofill::PasswordForm> forms;
  GetLoginsList(&forms.get(), form.signon_realm, wallet_handle);

  bool updated = false;
  for (size_t i = 0; i < forms.size(); ++i) {
    if (CompareForms(form, *forms[i], true)) {
      *forms[i] = form;
      updated = true;
    }
  }
  if (!updated)
    return true;

  if (SetLoginsList(forms.get(), form.signon_realm, wallet_handle)) {
    changes->push_back(password_manager::PasswordStoreChange(
        password_manager::PasswordStoreChange::UPDATE, form));
    return true;
  }

  return false;
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
    base::Time delete_begin,
    base::Time delete_end,
    password_manager::PasswordStoreChangeList* changes) {
  return RemoveLoginsBetween(
      delete_begin, delete_end, CREATION_TIMESTAMP, changes);
}

bool NativeBackendKWallet::RemoveLoginsSyncedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    password_manager::PasswordStoreChangeList* changes) {
  return RemoveLoginsBetween(delete_begin, delete_end, SYNC_TIMESTAMP, changes);
}

bool NativeBackendKWallet::GetLogins(const PasswordForm& form,
                                     PasswordFormList* forms) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;
  return GetLoginsList(forms, form.signon_realm, wallet_handle);
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
                                         const std::string& signon_realm,
                                         int wallet_handle) {
  // Is there an entry in the wallet?
  {
    dbus::MethodCall method_call(kKWalletInterface, "hasEntry");
    dbus::MessageWriter builder(&method_call);
    builder.AppendInt32(wallet_handle);  // handle
    builder.AppendString(folder_name_);  // folder
    builder.AppendString(signon_realm);  // key
    builder.AppendString(app_name_);     // appid
    scoped_ptr<dbus::Response> response(
        kwallet_proxy_->CallMethodAndBlock(
            &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
    if (!response.get()) {
      LOG(ERROR) << "Error contacting kwalletd (hasEntry)";
      return false;
    }
    dbus::MessageReader reader(response.get());
    bool has_entry = false;
    if (!reader.PopBool(&has_entry)) {
      LOG(ERROR) << "Error reading response from kwalletd (hasEntry): "
                 << response->ToString();
      return false;
    }
    if (!has_entry) {
      // This is not an error. There just isn't a matching entry.
      return true;
    }
  }

  {
    dbus::MethodCall method_call(kKWalletInterface, "readEntry");
    dbus::MessageWriter builder(&method_call);
    builder.AppendInt32(wallet_handle);  // handle
    builder.AppendString(folder_name_);  // folder
    builder.AppendString(signon_realm);  // key
    builder.AppendString(app_name_);     // appid
    scoped_ptr<dbus::Response> response(
        kwallet_proxy_->CallMethodAndBlock(
            &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
    if (!response.get()) {
      LOG(ERROR) << "Error contacting kwalletd (readEntry)";
      return false;
    }
    dbus::MessageReader reader(response.get());
    const uint8_t* bytes = NULL;
    size_t length = 0;
    if (!reader.PopArrayOfBytes(&bytes, &length)) {
      LOG(ERROR) << "Error reading response from kwalletd (readEntry): "
                 << response->ToString();
      return false;
    }
    if (!bytes)
      return false;
    if (!CheckSerializedValue(bytes, length, signon_realm)) {
      // This is weird, but we choose not to call it an error. There is an
      // invalid entry somehow, but by just ignoring it, we make it easier to
      // repair without having to delete it using kwalletmanager (that is, by
      // just saving a new password within this realm to overwrite it).
      return true;
    }

    // Can't we all just agree on whether bytes are signed or not? Please?
    Pickle pickle(reinterpret_cast<const char*>(bytes), length);
    PasswordFormList all_forms;
    DeserializeValue(signon_realm, pickle, forms);
  }

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

bool NativeBackendKWallet::GetAllLogins(PasswordFormList* forms,
                                        int wallet_handle) {
  // We could probably also use readEntryList here.
  std::vector<std::string> realm_list;
  {
    dbus::MethodCall method_call(kKWalletInterface, "entryList");
    dbus::MessageWriter builder(&method_call);
    builder.AppendInt32(wallet_handle);  // handle
    builder.AppendString(folder_name_);  // folder
    builder.AppendString(app_name_);     // appid
    scoped_ptr<dbus::Response> response(
        kwallet_proxy_->CallMethodAndBlock(
            &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
    if (!response.get()) {
      LOG(ERROR) << "Error contacting kwalletd (entryList)";
      return false;
    }
    dbus::MessageReader reader(response.get());
    if (!reader.PopArrayOfStrings(&realm_list)) {
      LOG(ERROR) << "Error reading response from kwalletd (entryList): "
                 << response->ToString();
      return false;
    }
  }

  for (size_t i = 0; i < realm_list.size(); ++i) {
    const std::string& signon_realm = realm_list[i];
    dbus::MethodCall method_call(kKWalletInterface, "readEntry");
    dbus::MessageWriter builder(&method_call);
    builder.AppendInt32(wallet_handle);  // handle
    builder.AppendString(folder_name_);  // folder
    builder.AppendString(signon_realm);  // key
    builder.AppendString(app_name_);     // appid
    scoped_ptr<dbus::Response> response(
        kwallet_proxy_->CallMethodAndBlock(
            &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
    if (!response.get()) {
      LOG(ERROR) << "Error contacting kwalletd (readEntry)";
      continue;
    }
    dbus::MessageReader reader(response.get());
    const uint8_t* bytes = NULL;
    size_t length = 0;
    if (!reader.PopArrayOfBytes(&bytes, &length)) {
      LOG(ERROR) << "Error reading response from kwalletd (readEntry): "
                 << response->ToString();
      continue;
    }
    if (!bytes || !CheckSerializedValue(bytes, length, signon_realm))
      continue;

    // Can't we all just agree on whether bytes are signed or not? Please?
    Pickle pickle(reinterpret_cast<const char*>(bytes), length);
    PasswordFormList all_forms;
    DeserializeValue(signon_realm, pickle, forms);
  }
  return true;
}

bool NativeBackendKWallet::SetLoginsList(const PasswordFormList& forms,
                                         const std::string& signon_realm,
                                         int wallet_handle) {
  if (forms.empty()) {
    // No items left? Remove the entry from the wallet.
    dbus::MethodCall method_call(kKWalletInterface, "removeEntry");
    dbus::MessageWriter builder(&method_call);
    builder.AppendInt32(wallet_handle);  // handle
    builder.AppendString(folder_name_);  // folder
    builder.AppendString(signon_realm);  // key
    builder.AppendString(app_name_);     // appid
    scoped_ptr<dbus::Response> response(
        kwallet_proxy_->CallMethodAndBlock(
            &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
    if (!response.get()) {
      LOG(ERROR) << "Error contacting kwalletd (removeEntry)";
      return kInvalidKWalletHandle;
    }
    dbus::MessageReader reader(response.get());
    int ret = 0;
    if (!reader.PopInt32(&ret)) {
      LOG(ERROR) << "Error reading response from kwalletd (removeEntry): "
                 << response->ToString();
      return false;
    }
    if (ret != 0)
      LOG(ERROR) << "Bad return code " << ret << " from KWallet removeEntry";
    return ret == 0;
  }

  Pickle value;
  SerializeValue(forms, &value);

  dbus::MethodCall method_call(kKWalletInterface, "writeEntry");
  dbus::MessageWriter builder(&method_call);
  builder.AppendInt32(wallet_handle);  // handle
  builder.AppendString(folder_name_);  // folder
  builder.AppendString(signon_realm);  // key
  builder.AppendArrayOfBytes(static_cast<const uint8_t*>(value.data()),
                             value.size());  // value
  builder.AppendString(app_name_);     // appid
  scoped_ptr<dbus::Response> response(
      kwallet_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response.get()) {
    LOG(ERROR) << "Error contacting kwalletd (writeEntry)";
    return kInvalidKWalletHandle;
  }
  dbus::MessageReader reader(response.get());
  int ret = 0;
  if (!reader.PopInt32(&ret)) {
    LOG(ERROR) << "Error reading response from kwalletd (writeEntry): "
               << response->ToString();
    return false;
  }
  if (ret != 0)
    LOG(ERROR) << "Bad return code " << ret << " from KWallet writeEntry";
  return ret == 0;
}

bool NativeBackendKWallet::RemoveLoginsBetween(
    base::Time delete_begin,
    base::Time delete_end,
    TimestampToCompare date_to_compare,
    password_manager::PasswordStoreChangeList* changes) {
  DCHECK(changes);
  changes->clear();
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;

  // We could probably also use readEntryList here.
  std::vector<std::string> realm_list;
  {
    dbus::MethodCall method_call(kKWalletInterface, "entryList");
    dbus::MessageWriter builder(&method_call);
    builder.AppendInt32(wallet_handle);  // handle
    builder.AppendString(folder_name_);  // folder
    builder.AppendString(app_name_);     // appid
    scoped_ptr<dbus::Response> response(kwallet_proxy_->CallMethodAndBlock(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
    if (!response.get()) {
      LOG(ERROR) << "Error contacting kwalletd (entryList)";
      return false;
    }
    dbus::MessageReader reader(response.get());
    dbus::MessageReader array(response.get());
    if (!reader.PopArray(&array)) {
      LOG(ERROR) << "Error reading response from kwalletd (entryList): "
                 << response->ToString();
      return false;
    }
    while (array.HasMoreData()) {
      std::string realm;
      if (!array.PopString(&realm)) {
        LOG(ERROR) << "Error reading response from kwalletd (entryList): "
                   << response->ToString();
        return false;
      }
      realm_list.push_back(realm);
    }
  }

  bool ok = true;
  for (size_t i = 0; i < realm_list.size(); ++i) {
    const std::string& signon_realm = realm_list[i];
    dbus::MethodCall method_call(kKWalletInterface, "readEntry");
    dbus::MessageWriter builder(&method_call);
    builder.AppendInt32(wallet_handle);  // handle
    builder.AppendString(folder_name_);  // folder
    builder.AppendString(signon_realm);  // key
    builder.AppendString(app_name_);     // appid
    scoped_ptr<dbus::Response> response(kwallet_proxy_->CallMethodAndBlock(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
    if (!response.get()) {
      LOG(ERROR) << "Error contacting kwalletd (readEntry)";
      continue;
    }
    dbus::MessageReader reader(response.get());
    const uint8_t* bytes = NULL;
    size_t length = 0;
    if (!reader.PopArrayOfBytes(&bytes, &length)) {
      LOG(ERROR) << "Error reading response from kwalletd (readEntry): "
                 << response->ToString();
      continue;
    }
    if (!bytes || !CheckSerializedValue(bytes, length, signon_realm))
      continue;

    // Can't we all just agree on whether bytes are signed or not? Please?
    Pickle pickle(reinterpret_cast<const char*>(bytes), length);
    PasswordFormList all_forms;
    DeserializeValue(signon_realm, pickle, &all_forms);

    PasswordFormList kept_forms;
    kept_forms.reserve(all_forms.size());
    base::Time autofill::PasswordForm::*date_member =
        date_to_compare == CREATION_TIMESTAMP
            ? &autofill::PasswordForm::date_created
            : &autofill::PasswordForm::date_synced;
    for (size_t i = 0; i < all_forms.size(); ++i) {
      if (delete_begin <= all_forms[i]->*date_member &&
          (delete_end.is_null() || all_forms[i]->*date_member < delete_end)) {
        changes->push_back(password_manager::PasswordStoreChange(
            password_manager::PasswordStoreChange::REMOVE, *all_forms[i]));
        delete all_forms[i];
      } else {
        kept_forms.push_back(all_forms[i]);
      }
    }

    if (!SetLoginsList(kept_forms, signon_realm, wallet_handle)) {
      ok = false;
      changes->clear();
    }
    STLDeleteElements(&kept_forms);
  }
  return ok;
}

// static
void NativeBackendKWallet::SerializeValue(const PasswordFormList& forms,
                                          Pickle* pickle) {
  pickle->WriteInt(kPickleVersion);
  pickle->WriteUInt64(forms.size());
  for (PasswordFormList::const_iterator it = forms.begin();
       it != forms.end(); ++it) {
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
    pickle->WriteInt(form->type);
    pickle->WriteInt(form->times_used);
    autofill::SerializeFormData(form->form_data, pickle);
    pickle->WriteInt64(form->date_synced.ToInternalValue());
    pickle->WriteString16(form->display_name);
    pickle->WriteString(form->avatar_url.spec());
    pickle->WriteString(form->federation_url.spec());
    pickle->WriteBool(form->is_zero_click);
  }
}

// static
bool NativeBackendKWallet::DeserializeValueSize(const std::string& signon_realm,
                                                const PickleIterator& init_iter,
                                                int version,
                                                bool size_32,
                                                bool warn_only,
                                                PasswordFormList* forms) {
  PickleIterator iter = init_iter;

  uint64_t count = 0;
  if (size_32) {
    uint32_t count_32 = 0;
    if (!iter.ReadUInt32(&count_32)) {
      LOG(ERROR) << "Failed to deserialize KWallet entry "
                 << "(realm: " << signon_realm << ")";
      return false;
    }
    count = count_32;
  } else {
    if (!iter.ReadUInt64(&count)) {
      LOG(ERROR) << "Failed to deserialize KWallet entry "
                 << "(realm: " << signon_realm << ")";
      return false;
    }
  }

  if (count > 0xFFFF) {
    // Trying to pin down the cause of http://crbug.com/80728 (or fix it).
    // This is a very large number of passwords to be saved for a single realm.
    // It is almost certainly a corrupt pickle and not real data. Ignore it.
    // This very well might actually be http://crbug.com/107701, so if we're
    // reading an old pickle, we don't even log this the first time we try to
    // read it. (That is, when we're reading the native architecture size.)
    if (!warn_only) {
      LOG(ERROR) << "Suspiciously large number of entries in KWallet entry "
                 << "(" << count << "; realm: " << signon_realm << ")";
    }
    return false;
  }

  forms->reserve(forms->size() + count);
  for (uint64_t i = 0; i < count; ++i) {
    scoped_ptr<PasswordForm> form(new PasswordForm());
    form->signon_realm.assign(signon_realm);

    int scheme = 0;
    int64 date_created = 0;
    int type = 0;
    // Note that these will be read back in the order listed due to
    // short-circuit evaluation. This is important.
    if (!iter.ReadInt(&scheme) ||
        !ReadGURL(&iter, warn_only, &form->origin) ||
        !ReadGURL(&iter, warn_only, &form->action) ||
        !iter.ReadString16(&form->username_element) ||
        !iter.ReadString16(&form->username_value) ||
        !iter.ReadString16(&form->password_element) ||
        !iter.ReadString16(&form->password_value) ||
        !iter.ReadString16(&form->submit_element) ||
        !iter.ReadBool(&form->ssl_valid) ||
        !iter.ReadBool(&form->preferred) ||
        !iter.ReadBool(&form->blacklisted_by_user) ||
        !iter.ReadInt64(&date_created)) {
      LogDeserializationWarning(version, signon_realm, warn_only);
      return false;
    }
    form->scheme = static_cast<PasswordForm::Scheme>(scheme);
    form->date_created = base::Time::FromTimeT(date_created);

    if (version > 1) {
      if (!iter.ReadInt(&type) ||
          !iter.ReadInt(&form->times_used) ||
          !autofill::DeserializeFormData(&iter, &form->form_data)) {
        LogDeserializationWarning(version, signon_realm, false);
        return false;
      }
      form->type = static_cast<PasswordForm::Type>(type);
    }

    if (version > 2) {
      int64 date_synced = 0;
      if (!iter.ReadInt64(&date_synced)) {
        LogDeserializationWarning(version, signon_realm, false);
        return false;
      }
      form->date_synced = base::Time::FromInternalValue(date_synced);
    }

    if (version > 3) {
      if (!iter.ReadString16(&form->display_name) ||
          !ReadGURL(&iter, warn_only, &form->avatar_url) ||
          !ReadGURL(&iter, warn_only, &form->federation_url) ||
          !iter.ReadBool(&form->is_zero_click)) {
        LogDeserializationWarning(version, signon_realm, false);
        return false;
      }
    }

    forms->push_back(form.release());
  }

  return true;
}

// static
void NativeBackendKWallet::DeserializeValue(const std::string& signon_realm,
                                            const Pickle& pickle,
                                            PasswordFormList* forms) {
  PickleIterator iter(pickle);

  int version = -1;
  if (!iter.ReadInt(&version) ||
      version < 0 || version > kPickleVersion) {
    LOG(ERROR) << "Failed to deserialize KWallet entry "
               << "(realm: " << signon_realm << ")";
    return;
  }

  if (version > 0) {
    // In current pickles, we expect 64-bit sizes. Failure is an error.
    DeserializeValueSize(signon_realm, iter, version, false, false, forms);
    return;
  }

  const size_t saved_forms_size = forms->size();
  const bool size_32 = sizeof(size_t) == sizeof(uint32_t);
  if (!DeserializeValueSize(
          signon_realm, iter, version, size_32, true, forms)) {
    // We failed to read the pickle using the native architecture of the system.
    // Try again with the opposite architecture. Note that we do this even on
    // 32-bit machines, in case we're reading a 64-bit pickle. (Probably rare,
    // since mostly we expect upgrades, not downgrades, but both are possible.)
    forms->resize(saved_forms_size);
    DeserializeValueSize(signon_realm, iter, version, !size_32, false, forms);
  }
}

int NativeBackendKWallet::WalletHandle() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  // Open the wallet.
  // TODO(mdm): Are we leaking these handles? Find out.
  int32_t handle = kInvalidKWalletHandle;
  {
    dbus::MethodCall method_call(kKWalletInterface, "open");
    dbus::MessageWriter builder(&method_call);
    builder.AppendString(wallet_name_);  // wallet
    builder.AppendInt64(0);              // wid
    builder.AppendString(app_name_);     // appid
    scoped_ptr<dbus::Response> response(
        kwallet_proxy_->CallMethodAndBlock(
            &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
    if (!response.get()) {
      LOG(ERROR) << "Error contacting kwalletd (open)";
      return kInvalidKWalletHandle;
    }
    dbus::MessageReader reader(response.get());
    if (!reader.PopInt32(&handle)) {
      LOG(ERROR) << "Error reading response from kwalletd (open): "
                 << response->ToString();
      return kInvalidKWalletHandle;
    }
    if (handle == kInvalidKWalletHandle) {
      LOG(ERROR) << "Error obtaining KWallet handle";
      return kInvalidKWalletHandle;
    }
  }

  // Check if our folder exists.
  bool has_folder = false;
  {
    dbus::MethodCall method_call(kKWalletInterface, "hasFolder");
    dbus::MessageWriter builder(&method_call);
    builder.AppendInt32(handle);         // handle
    builder.AppendString(folder_name_);  // folder
    builder.AppendString(app_name_);     // appid
    scoped_ptr<dbus::Response> response(
        kwallet_proxy_->CallMethodAndBlock(
            &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
    if (!response.get()) {
      LOG(ERROR) << "Error contacting kwalletd (hasFolder)";
      return kInvalidKWalletHandle;
    }
    dbus::MessageReader reader(response.get());
    if (!reader.PopBool(&has_folder)) {
      LOG(ERROR) << "Error reading response from kwalletd (hasFolder): "
                 << response->ToString();
      return kInvalidKWalletHandle;
    }
  }

  // Create it if it didn't.
  if (!has_folder) {
    dbus::MethodCall method_call(kKWalletInterface, "createFolder");
    dbus::MessageWriter builder(&method_call);
    builder.AppendInt32(handle);         // handle
    builder.AppendString(folder_name_);  // folder
    builder.AppendString(app_name_);     // appid
    scoped_ptr<dbus::Response> response(
        kwallet_proxy_->CallMethodAndBlock(
            &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
    if (!response.get()) {
      LOG(ERROR) << "Error contacting kwalletd (createFolder)";
      return kInvalidKWalletHandle;
    }
    dbus::MessageReader reader(response.get());
    bool success = false;
    if (!reader.PopBool(&success)) {
      LOG(ERROR) << "Error reading response from kwalletd (createFolder): "
                 << response->ToString();
      return kInvalidKWalletHandle;
    }
    if (!success) {
      LOG(ERROR) << "Error creating KWallet folder";
      return kInvalidKWalletHandle;
    }
  }

  return handle;
}

std::string NativeBackendKWallet::GetProfileSpecificFolderName() const {
  // Originally, the folder name was always just "Chrome Form Data".
  // Now we use it to distinguish passwords for different profiles.
  return base::StringPrintf("%s (%d)", kKWalletFolder, profile_id_);
}
