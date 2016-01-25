// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/native_backend_kwallet_x.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/pickle.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/grit/chromium_strings.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "ui/base/l10n/l10n_util.h"

using autofill::PasswordForm;
using content::BrowserThread;

namespace {

// In case the fields in the pickle ever change, version them so we can try to
// read old pickles. (Note: do not eat old pickles past the expiration date.)
const int kPickleVersion = 7;

// We could localize this string, but then changing your locale would cause
// you to lose access to all your stored passwords. Maybe best not to do that.
// Name of the folder to store passwords in.
const char kKWalletFolder[] = "Chrome Form Data";

// DBus service, path, and interface names for klauncher and kwalletd.
const char kKWalletDName[] = "kwalletd";
const char kKWalletD5Name[] = "kwalletd5";
const char kKWalletServiceName[] = "org.kde.kwalletd";
const char kKWallet5ServiceName[] = "org.kde.kwalletd5";
const char kKWalletPath[] = "/modules/kwalletd";
const char kKWallet5Path[] = "/modules/kwalletd5";
const char kKWalletInterface[] = "org.kde.KWallet";
const char kKLauncherServiceName[] = "org.kde.klauncher";
const char kKLauncherPath[] = "/KLauncher";
const char kKLauncherInterface[] = "org.kde.KLauncher";

// Checks a serialized list of PasswordForms for sanity. Returns true if OK.
// Note that |realm| is only used for generating a useful warning message.
bool CheckSerializedValue(const uint8_t* byte_array,
                          size_t length,
                          const std::string& realm) {
  const base::Pickle::Header* header =
      reinterpret_cast<const base::Pickle::Header*>(byte_array);
  if (length < sizeof(*header) ||
      header->payload_size > length - sizeof(*header)) {
    LOG(WARNING) << "Invalid KWallet entry detected (realm: " << realm << ")";
    return false;
  }
  return true;
}

// Convenience function to read a GURL from a Pickle. Assumes the URL has
// been written as a UTF-8 string. Returns true on success.
bool ReadGURL(base::PickleIterator* iter, bool warn_only, GURL* url) {
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

// Deserializes a list of credentials from the wallet to |forms| (replacing
// the contents of |forms|). |size_32| controls reading the size field within
// the pickle as 32 bits. We used to use Pickle::WriteSize() to write the number
// of password forms, but that has a different size on 32- and 64-bit systems.
// So, now we always write a 64-bit quantity, but we support trying to read it
// as either size when reading old pickles that fail to deserialize using the
// native size. Returns true on success.
bool DeserializeValueSize(const std::string& signon_realm,
                          const base::PickleIterator& init_iter,
                          int version,
                          bool size_32,
                          bool warn_only,
                          ScopedVector<autofill::PasswordForm>* forms) {
  base::PickleIterator iter = init_iter;

  size_t count = 0;
  if (size_32) {
    uint32_t count_32 = 0;
    if (!iter.ReadUInt32(&count_32)) {
      LOG(ERROR) << "Failed to deserialize KWallet entry "
                 << "(realm: " << signon_realm << ")";
      return false;
    }
    count = count_32;
  } else {
    if (!iter.ReadSizeT(&count)) {
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

  // We'll swap |converted_forms| with |*forms| on success, to make sure we
  // don't return partial results on failure.
  ScopedVector<autofill::PasswordForm> converted_forms;
  converted_forms.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    scoped_ptr<PasswordForm> form(new PasswordForm());
    form->signon_realm.assign(signon_realm);

    int scheme = 0;
    int64_t date_created = 0;
    int type = 0;
    int generation_upload_status = 0;
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
      int64_t date_synced = 0;
      if (!iter.ReadInt64(&date_synced)) {
        LogDeserializationWarning(version, signon_realm, false);
        return false;
      }
      form->date_synced = base::Time::FromInternalValue(date_synced);
    }

    if (version > 3) {
      if (!iter.ReadString16(&form->display_name) ||
          !ReadGURL(&iter, warn_only, &form->icon_url) ||
          !ReadGURL(&iter, warn_only, &form->federation_url) ||
          !iter.ReadBool(&form->skip_zero_click)) {
        LogDeserializationWarning(version, signon_realm, false);
        return false;
      }
    }

    if (version > 4) {
      form->date_created = base::Time::FromInternalValue(date_created);
    } else {
      form->date_created = base::Time::FromTimeT(date_created);
    }

    if (version > 5) {
      bool read_success = iter.ReadInt(&generation_upload_status);
      if (!read_success && version > 6) {
        // Valid version 6 pickles might still lack the
        // generation_upload_status, see http://crbug.com/494229#c11.
        LogDeserializationWarning(version, signon_realm, false);
        return false;
      }
      if (read_success) {
        form->generation_upload_status =
            static_cast<PasswordForm::GenerationUploadStatus>(
                generation_upload_status);
      }
    }

    converted_forms.push_back(std::move(form));
  }

  forms->swap(converted_forms);
  return true;
}

// Serializes a list of PasswordForms to be stored in the wallet.
void SerializeValue(const std::vector<autofill::PasswordForm*>& forms,
                    base::Pickle* pickle) {
  pickle->WriteInt(kPickleVersion);
  pickle->WriteSizeT(forms.size());
  for (autofill::PasswordForm* form : forms) {
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
    pickle->WriteInt64(form->date_created.ToInternalValue());
    pickle->WriteInt(form->type);
    pickle->WriteInt(form->times_used);
    autofill::SerializeFormData(form->form_data, pickle);
    pickle->WriteInt64(form->date_synced.ToInternalValue());
    pickle->WriteString16(form->display_name);
    pickle->WriteString(form->icon_url.spec());
    pickle->WriteString(form->federation_url.spec());
    pickle->WriteBool(form->skip_zero_click);
    pickle->WriteInt(form->generation_upload_status);
  }
}

// Moves the content of |second| to the end of |first|.
void AppendSecondToFirst(ScopedVector<autofill::PasswordForm>* first,
                         ScopedVector<autofill::PasswordForm> second) {
  first->reserve(first->size() + second.size());
  first->insert(first->end(), second.begin(), second.end());
  second.weak_clear();
}

void UMALogDeserializationStatus(bool success) {
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.KWalletDeserializationStatus",
                        success);
}

}  // namespace

NativeBackendKWallet::NativeBackendKWallet(
    LocalProfileId id, base::nix::DesktopEnvironment desktop_env)
    : profile_id_(id),
      kwallet_proxy_(nullptr),
      app_name_(l10n_util::GetStringUTF8(IDS_PRODUCT_NAME)) {
  folder_name_ = GetProfileSpecificFolderName();

  if (desktop_env == base::nix::DESKTOP_ENVIRONMENT_KDE5) {
    dbus_service_name_ = kKWallet5ServiceName;
    dbus_path_ = kKWallet5Path;
    kwalletd_name_ = kKWalletD5Name;
  } else {
    dbus_service_name_ = kKWalletServiceName;
    dbus_path_ = kKWalletPath;
    kwalletd_name_ = kKWalletDName;
  }
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
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
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
      session_bus_->GetObjectProxy(dbus_service_name_,
                                   dbus::ObjectPath(dbus_path_));
  // kwalletd may not be running. If we get a temporary failure initializing it,
  // try to start it and then try again. (Note the short-circuit evaluation.)
  const InitResult result = InitWallet();
  *success = (result == INIT_SUCCESS ||
              (result == TEMPORARY_FAIL &&
               StartKWalletd() && InitWallet() == INIT_SUCCESS));
  event->Signal();
}

bool NativeBackendKWallet::StartKWalletd() {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  // Sadly kwalletd doesn't use DBus activation, so we have to make a call to
  // klauncher to start it.
  dbus::ObjectProxy* klauncher =
      session_bus_->GetObjectProxy(kKLauncherServiceName,
                                   dbus::ObjectPath(kKLauncherPath));

  dbus::MethodCall method_call(kKLauncherInterface,
                               "start_service_by_desktop_name");
  dbus::MessageWriter builder(&method_call);
  std::vector<std::string> empty;
  builder.AppendString(kwalletd_name_);  // serviceName
  builder.AppendArrayOfStrings(empty);   // urls
  builder.AppendArrayOfStrings(empty);   // envs
  builder.AppendString(std::string());   // startup_id
  builder.AppendBool(false);             // blind
  scoped_ptr<dbus::Response> response(
      klauncher->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response.get()) {
    LOG(ERROR) << "Error contacting klauncher to start " << kwalletd_name_;
    return false;
  }
  dbus::MessageReader reader(response.get());
  int32_t ret = -1;
  std::string dbus_name;
  std::string error;
  int32_t pid = -1;
  if (!reader.PopInt32(&ret) || !reader.PopString(&dbus_name) ||
      !reader.PopString(&error) || !reader.PopInt32(&pid)) {
    LOG(ERROR) << "Error reading response from klauncher to start "
               << kwalletd_name_ << ": " << response->ToString();
    return false;
  }
  if (!error.empty() || ret) {
    LOG(ERROR) << "Error launching " << kwalletd_name_ << ": error '" << error
               << "' (code " << ret << ")";
    return false;
  }

  return true;
}

NativeBackendKWallet::InitResult NativeBackendKWallet::InitWallet() {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  {
    // Check that KWallet is enabled.
    dbus::MethodCall method_call(kKWalletInterface, "isEnabled");
    scoped_ptr<dbus::Response> response(
        kwallet_proxy_->CallMethodAndBlock(
            &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
    if (!response.get()) {
      LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (isEnabled)";
      return TEMPORARY_FAIL;
    }
    dbus::MessageReader reader(response.get());
    bool enabled = false;
    if (!reader.PopBool(&enabled)) {
      LOG(ERROR) << "Error reading response from " << kwalletd_name_
                 << " (isEnabled): " << response->ToString();
      return PERMANENT_FAIL;
    }
    // Not enabled? Don't use KWallet. But also don't warn here.
    if (!enabled) {
      VLOG(1) << kwalletd_name_ << " reports that KWallet is not enabled.";
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
      LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (networkWallet)";
      return TEMPORARY_FAIL;
    }
    dbus::MessageReader reader(response.get());
    if (!reader.PopString(&wallet_name_)) {
      LOG(ERROR) << "Error reading response from " << kwalletd_name_
                 << " (networkWallet): " << response->ToString();
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
  if (!GetLoginsList(form.signon_realm, wallet_handle, &forms))
    return password_manager::PasswordStoreChangeList();

  auto it = std::partition(forms.begin(), forms.end(),
                           [&form](const PasswordForm* current_form) {
    return !ArePasswordFormUniqueKeyEqual(form, *current_form);
  });
  password_manager::PasswordStoreChangeList changes;
  if (it != forms.end()) {
    // It's an update.
    changes.push_back(password_manager::PasswordStoreChange(
        password_manager::PasswordStoreChange::REMOVE, **it));
    forms.erase(it, forms.end());
  }

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
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;

  ScopedVector<autofill::PasswordForm> forms;
  if (!GetLoginsList(form.signon_realm, wallet_handle, &forms))
    return false;

  auto it = std::partition(forms.begin(), forms.end(),
                           [&form](const PasswordForm* current_form) {
    return !ArePasswordFormUniqueKeyEqual(form, *current_form);
  });

  if (it == forms.end())
    return true;

  forms.erase(it, forms.end());
  forms.push_back(new PasswordForm(form));
  if (SetLoginsList(forms.get(), form.signon_realm, wallet_handle)) {
    changes->push_back(password_manager::PasswordStoreChange(
        password_manager::PasswordStoreChange::UPDATE, form));
    return true;
  }

  return false;
}

bool NativeBackendKWallet::RemoveLogin(
    const PasswordForm& form,
    password_manager::PasswordStoreChangeList* changes) {
  DCHECK(changes);
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;

  ScopedVector<autofill::PasswordForm> all_forms;
  if (!GetLoginsList(form.signon_realm, wallet_handle, &all_forms))
    return false;

  ScopedVector<autofill::PasswordForm> kept_forms;
  kept_forms.reserve(all_forms.size());
  for (auto& saved_form : all_forms) {
    if (!ArePasswordFormUniqueKeyEqual(form, *saved_form)) {
      kept_forms.push_back(saved_form);
      saved_form = nullptr;
    }
  }

  if (kept_forms.size() != all_forms.size()) {
    changes->push_back(password_manager::PasswordStoreChange(
        password_manager::PasswordStoreChange::REMOVE, form));
    return SetLoginsList(kept_forms.get(), form.signon_realm, wallet_handle);
  }

  return true;
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

bool NativeBackendKWallet::GetLogins(
    const PasswordForm& form,
    ScopedVector<autofill::PasswordForm>* forms) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;
  return GetLoginsList(form.signon_realm, wallet_handle, forms);
}

bool NativeBackendKWallet::GetAutofillableLogins(
    ScopedVector<autofill::PasswordForm>* forms) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;
  return GetLoginsList(BlacklistOptions::AUTOFILLABLE, wallet_handle, forms);
}

bool NativeBackendKWallet::GetBlacklistLogins(
    ScopedVector<autofill::PasswordForm>* forms) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;
  return GetLoginsList(BlacklistOptions::BLACKLISTED, wallet_handle, forms);
}

bool NativeBackendKWallet::GetAllLogins(
    ScopedVector<autofill::PasswordForm>* forms) {
  int wallet_handle = WalletHandle();
  if (wallet_handle == kInvalidKWalletHandle)
    return false;
  return GetAllLoginsInternal(wallet_handle, forms);
}

bool NativeBackendKWallet::GetLoginsList(
    const std::string& signon_realm,
    int wallet_handle,
    ScopedVector<autofill::PasswordForm>* forms) {
  forms->clear();
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
      LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (hasEntry)";
      return false;
    }
    dbus::MessageReader reader(response.get());
    bool has_entry = false;
    if (!reader.PopBool(&has_entry)) {
      LOG(ERROR) << "Error reading response from " << kwalletd_name_
                 << " (hasEntry): " << response->ToString();
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
      LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (readEntry)";
      return false;
    }
    dbus::MessageReader reader(response.get());
    const uint8_t* bytes = nullptr;
    size_t length = 0;
    if (!reader.PopArrayOfBytes(&bytes, &length)) {
      LOG(ERROR) << "Error reading response from " << kwalletd_name_
                 << " (readEntry): " << response->ToString();
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
    base::Pickle pickle(reinterpret_cast<const char*>(bytes), length);
    *forms = DeserializeValue(signon_realm, pickle);
  }

  return true;
}

bool NativeBackendKWallet::GetLoginsList(
    BlacklistOptions options,
    int wallet_handle,
    ScopedVector<autofill::PasswordForm>* forms) {
  forms->clear();
  ScopedVector<autofill::PasswordForm> all_forms;
  if (!GetAllLoginsInternal(wallet_handle, &all_forms))
    return false;

  // Remove the duplicate sync tags.
  ScopedVector<autofill::PasswordForm> duplicates;
  password_manager_util::FindDuplicates(&all_forms, &duplicates, nullptr);
  if (!duplicates.empty()) {
    // Fill the signon realms to be updated.
    std::map<std::string, std::vector<autofill::PasswordForm*>> update_forms;
    for (autofill::PasswordForm* form : duplicates) {
      update_forms.insert(std::make_pair(
          form->signon_realm, std::vector<autofill::PasswordForm*>()));
    }

    // Fill the actual forms to be saved.
    for (autofill::PasswordForm* form : all_forms) {
      auto it = update_forms.find(form->signon_realm);
      if (it != update_forms.end())
        it->second.push_back(form);
    }

    // Update the backend.
    for (const auto& forms : update_forms) {
      if (!SetLoginsList(forms.second, forms.first, wallet_handle))
        return false;
    }
  }
  // We have to read all the entries, and then filter them here.
  forms->reserve(all_forms.size());
  for (auto& saved_form : all_forms) {
    if (saved_form->blacklisted_by_user ==
        (options == BlacklistOptions::BLACKLISTED)) {
      forms->push_back(saved_form);
      saved_form = nullptr;
    }
  }

  return true;
}

bool NativeBackendKWallet::GetAllLoginsInternal(
    int wallet_handle,
    ScopedVector<autofill::PasswordForm>* forms) {
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
      LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (entryList)";
      return false;
    }
    dbus::MessageReader reader(response.get());
    if (!reader.PopArrayOfStrings(&realm_list)) {
      LOG(ERROR) << "Error reading response from " << kwalletd_name_
                 << "(entryList): " << response->ToString();
      return false;
    }
  }

  forms->clear();
  for (const std::string& signon_realm : realm_list) {
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
      LOG(ERROR) << "Error contacting " << kwalletd_name_ << "(readEntry)";
      return false;
    }
    dbus::MessageReader reader(response.get());
    const uint8_t* bytes = nullptr;
    size_t length = 0;
    if (!reader.PopArrayOfBytes(&bytes, &length)) {
      LOG(ERROR) << "Error reading response from " << kwalletd_name_
                 << " (readEntry): " << response->ToString();
      return false;
    }
    if (!bytes || !CheckSerializedValue(bytes, length, signon_realm))
      continue;

    // Can't we all just agree on whether bytes are signed or not? Please?
    base::Pickle pickle(reinterpret_cast<const char*>(bytes), length);
    AppendSecondToFirst(forms, DeserializeValue(signon_realm, pickle));
  }
  return true;
}

bool NativeBackendKWallet::SetLoginsList(
    const std::vector<autofill::PasswordForm*>& forms,
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
      LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (removeEntry)";
      return kInvalidKWalletHandle;
    }
    dbus::MessageReader reader(response.get());
    int ret = 0;
    if (!reader.PopInt32(&ret)) {
      LOG(ERROR) << "Error reading response from " << kwalletd_name_
                 << " (removeEntry): " << response->ToString();
      return false;
    }
    if (ret != 0)
      LOG(ERROR) << "Bad return code " << ret << " from KWallet removeEntry";
    return ret == 0;
  }

  base::Pickle value;
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
    LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (writeEntry)";
    return kInvalidKWalletHandle;
  }
  dbus::MessageReader reader(response.get());
  int ret = 0;
  if (!reader.PopInt32(&ret)) {
    LOG(ERROR) << "Error reading response from " << kwalletd_name_
               << " (writeEntry): " << response->ToString();
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
      LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (entryList)";
      return false;
    }
    dbus::MessageReader reader(response.get());
    dbus::MessageReader array(response.get());
    if (!reader.PopArray(&array)) {
      LOG(ERROR) << "Error reading response from " << kwalletd_name_
                 << " (entryList): " << response->ToString();
      return false;
    }
    while (array.HasMoreData()) {
      std::string realm;
      if (!array.PopString(&realm)) {
        LOG(ERROR) << "Error reading response from " << kwalletd_name_
                   << " (entryList): " << response->ToString();
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
      LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (readEntry)";
      continue;
    }
    dbus::MessageReader reader(response.get());
    const uint8_t* bytes = nullptr;
    size_t length = 0;
    if (!reader.PopArrayOfBytes(&bytes, &length)) {
      LOG(ERROR) << "Error reading response from " << kwalletd_name_
                 << " (readEntry): " << response->ToString();
      continue;
    }
    if (!bytes || !CheckSerializedValue(bytes, length, signon_realm))
      continue;

    // Can't we all just agree on whether bytes are signed or not? Please?
    base::Pickle pickle(reinterpret_cast<const char*>(bytes), length);
    ScopedVector<autofill::PasswordForm> all_forms =
        DeserializeValue(signon_realm, pickle);

    ScopedVector<autofill::PasswordForm> kept_forms;
    kept_forms.reserve(all_forms.size());
    base::Time autofill::PasswordForm::*date_member =
        date_to_compare == CREATION_TIMESTAMP
            ? &autofill::PasswordForm::date_created
            : &autofill::PasswordForm::date_synced;
    for (auto& saved_form : all_forms) {
      if (delete_begin <= saved_form->*date_member &&
          (delete_end.is_null() || saved_form->*date_member < delete_end)) {
        changes->push_back(password_manager::PasswordStoreChange(
            password_manager::PasswordStoreChange::REMOVE, *saved_form));
      } else {
        kept_forms.push_back(saved_form);
        saved_form = nullptr;
      }
    }

    if (!SetLoginsList(kept_forms.get(), signon_realm, wallet_handle)) {
      ok = false;
      changes->clear();
    }
  }
  return ok;
}

// static
ScopedVector<autofill::PasswordForm> NativeBackendKWallet::DeserializeValue(
    const std::string& signon_realm,
    const base::Pickle& pickle) {
  base::PickleIterator iter(pickle);

  int version = -1;
  if (!iter.ReadInt(&version) ||
      version < 0 || version > kPickleVersion) {
    LOG(ERROR) << "Failed to deserialize KWallet entry "
               << "(realm: " << signon_realm << ")";
    return ScopedVector<autofill::PasswordForm>();
  }

  ScopedVector<autofill::PasswordForm> forms;
  bool success = true;
  if (version > 0) {
    // In current pickles, we expect 64-bit sizes. Failure is an error.
    success = DeserializeValueSize(
        signon_realm, iter, version, false, false, &forms);
    UMALogDeserializationStatus(success);
    return forms;
  }

  const bool size_32 = sizeof(size_t) == sizeof(uint32_t);
  if (!DeserializeValueSize(
          signon_realm, iter, version, size_32, true, &forms)) {
    // We failed to read the pickle using the native architecture of the system.
    // Try again with the opposite architecture. Note that we do this even on
    // 32-bit machines, in case we're reading a 64-bit pickle. (Probably rare,
    // since mostly we expect upgrades, not downgrades, but both are possible.)
    success = DeserializeValueSize(
        signon_realm, iter, version, !size_32, false, &forms);
  }
  UMALogDeserializationStatus(success);
  return forms;
}

int NativeBackendKWallet::WalletHandle() {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);

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
      LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (open)";
      return kInvalidKWalletHandle;
    }
    dbus::MessageReader reader(response.get());
    if (!reader.PopInt32(&handle)) {
      LOG(ERROR) << "Error reading response from " << kwalletd_name_
                 << " (open): " << response->ToString();
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
      LOG(ERROR) << "Error contacting " << kwalletd_name_ << " (hasFolder)";
      return kInvalidKWalletHandle;
    }
    dbus::MessageReader reader(response.get());
    if (!reader.PopBool(&has_folder)) {
      LOG(ERROR) << "Error reading response from " << kwalletd_name_
                 << " (hasFolder): " << response->ToString();
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
      LOG(ERROR) << "Error contacting << " << kwalletd_name_
                 << " (createFolder)";
      return kInvalidKWalletHandle;
    }
    dbus::MessageReader reader(response.get());
    bool success = false;
    if (!reader.PopBool(&success)) {
      LOG(ERROR) << "Error reading response from " << kwalletd_name_
                 << " (createFolder): " << response->ToString();
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
