// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/homedir_methods.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/device_event_log/device_event_log.h"

using chromeos::DBusThreadManager;
using google::protobuf::RepeatedPtrField;

namespace cryptohome {

namespace {

HomedirMethods* g_homedir_methods = NULL;

// The implementation of HomedirMethods
class HomedirMethodsImpl : public HomedirMethods {
 public:
  HomedirMethodsImpl() : weak_ptr_factory_(this) {}

  ~HomedirMethodsImpl() override = default;

  void GetKeyDataEx(const Identification& id,
                    const cryptohome::AuthorizationRequest& auth,
                    const cryptohome::GetKeyDataRequest& request,
                    const GetKeyDataCallback& callback) override {
    DBusThreadManager::Get()->GetCryptohomeClient()->GetKeyDataEx(
        id, auth, request,
        base::BindOnce(&HomedirMethodsImpl::OnGetKeyDataExCallback,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void CheckKeyEx(const Identification& id,
                  const cryptohome::AuthorizationRequest& auth,
                  const cryptohome::CheckKeyRequest& request,
                  const Callback& callback) override {
    DBusThreadManager::Get()->GetCryptohomeClient()->CheckKeyEx(
        id, auth, request,
        base::BindOnce(&HomedirMethodsImpl::OnBaseReplyCallback,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void AddKeyEx(const Identification& id,
                const AuthorizationRequest& auth,
                const AddKeyRequest& request,
                const Callback& callback) override {
    DBusThreadManager::Get()->GetCryptohomeClient()->AddKeyEx(
        id, auth, request,
        base::BindOnce(&HomedirMethodsImpl::OnBaseReplyCallback,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void RemoveKeyEx(const Identification& id,
                   const AuthorizationRequest& auth,
                   const RemoveKeyRequest& request,
                   const Callback& callback) override {
    DBusThreadManager::Get()->GetCryptohomeClient()->RemoveKeyEx(
        id, auth, request,
        base::BindOnce(&HomedirMethodsImpl::OnBaseReplyCallback,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void UpdateKeyEx(const Identification& id,
                   const AuthorizationRequest& auth,
                   const UpdateKeyRequest& request,
                   const Callback& callback) override {
    DBusThreadManager::Get()->GetCryptohomeClient()->UpdateKeyEx(
        id, auth, request,
        base::BindOnce(&HomedirMethodsImpl::OnBaseReplyCallback,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void GetAccountDiskUsage(
      const Identification& id,
      const GetAccountDiskUsageCallback& callback) override {
    DBusThreadManager::Get()->GetCryptohomeClient()->GetAccountDiskUsage(
        id, base::BindOnce(&HomedirMethodsImpl::OnGetAccountDiskUsageCallback,
                           weak_ptr_factory_.GetWeakPtr(), callback));
  }

 private:
  void OnGetKeyDataExCallback(const GetKeyDataCallback& callback,
                              base::Optional<BaseReply> reply) {
    if (!reply.has_value()) {
      callback.Run(false, MOUNT_ERROR_FATAL, std::vector<KeyDefinition>());
      return;
    }
    if (reply->has_error() && reply->error() != CRYPTOHOME_ERROR_NOT_SET) {
      callback.Run(false, CryptohomeErrorToMountError(reply->error()),
                   std::vector<KeyDefinition>());
      return;
    }

    if (!reply->HasExtension(GetKeyDataReply::reply)) {
      callback.Run(false, MOUNT_ERROR_FATAL, std::vector<KeyDefinition>());
      return;
    }

    // Extract the contents of the |KeyData| protos returned.
    const RepeatedPtrField<KeyData>& key_data =
        reply->GetExtension(GetKeyDataReply::reply).key_data();
    std::vector<KeyDefinition> key_definitions;
    for (RepeatedPtrField<KeyData>::const_iterator it = key_data.begin();
         it != key_data.end(); ++it) {
      // Extract |type|, |label| and |revision|.
      DCHECK_EQ(KeyData::KEY_TYPE_PASSWORD, it->type());
      key_definitions.push_back(KeyDefinition(std::string() /* secret */,
                                              it->label(),
                                              0 /* privileges */));
      KeyDefinition& key_definition = key_definitions.back();
      key_definition.revision = it->revision();

      // Extract |privileges|.
      const KeyPrivileges& privileges = it->privileges();
      if (privileges.mount())
        key_definition.privileges |= PRIV_MOUNT;
      if (privileges.add())
        key_definition.privileges |= PRIV_ADD;
      if (privileges.remove())
        key_definition.privileges |= PRIV_REMOVE;
      if (privileges.update())
        key_definition.privileges |= PRIV_MIGRATE;
      if (privileges.authorized_update())
        key_definition.privileges |= PRIV_AUTHORIZED_UPDATE;

      // Extract |authorization_data|.
      for (RepeatedPtrField<KeyAuthorizationData>::const_iterator auth_it =
               it->authorization_data().begin();
           auth_it != it->authorization_data().end(); ++auth_it) {
        key_definition.authorization_data.push_back(
            KeyDefinition::AuthorizationData());
        KeyAuthorizationDataToAuthorizationData(
            *auth_it, &key_definition.authorization_data.back());
      }

      // Extract |provider_data|.
      for (RepeatedPtrField<KeyProviderData::Entry>::const_iterator
              provider_data_it = it->provider_data().entry().begin();
           provider_data_it != it->provider_data().entry().end();
           ++provider_data_it) {
        // Extract |name|.
        key_definition.provider_data.push_back(
            KeyDefinition::ProviderData(provider_data_it->name()));
        KeyDefinition::ProviderData& provider_data =
            key_definition.provider_data.back();

        int data_items = 0;

        // Extract |number|.
        if (provider_data_it->has_number()) {
          provider_data.number.reset(new int64_t(provider_data_it->number()));
          ++data_items;
        }

        // Extract |bytes|.
        if (provider_data_it->has_bytes()) {
          provider_data.bytes.reset(
              new std::string(provider_data_it->bytes()));
          ++data_items;
        }

        DCHECK_EQ(1, data_items);
      }
    }

    callback.Run(true, MOUNT_ERROR_NONE, key_definitions);
  }

  void OnGetAccountDiskUsageCallback(
      const GetAccountDiskUsageCallback& callback,
      base::Optional<BaseReply> reply) {
    if (!reply.has_value()) {
      callback.Run(false, -1);
      return;
    }
    if (reply->has_error() && reply->error() != CRYPTOHOME_ERROR_NOT_SET) {
      callback.Run(false, -1);
      return;
    }
    if (!reply->HasExtension(GetAccountDiskUsageReply::reply)) {
      callback.Run(false, -1);
      return;
    }

    int64_t size = reply->GetExtension(GetAccountDiskUsageReply::reply).size();
    callback.Run(true, size);
  }

  void OnBaseReplyCallback(const Callback& callback,
                           base::Optional<BaseReply> reply) {
    if (!reply.has_value()) {
      callback.Run(false, MOUNT_ERROR_FATAL);
      return;
    }
    if (reply->has_error() && reply->error() != CRYPTOHOME_ERROR_NOT_SET) {
      callback.Run(false, CryptohomeErrorToMountError(reply->error()));
      return;
    }
    callback.Run(true, MOUNT_ERROR_NONE);
  }

  base::WeakPtrFactory<HomedirMethodsImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HomedirMethodsImpl);
};

}  // namespace

// static
void HomedirMethods::Initialize() {
  if (g_homedir_methods) {
    LOG(WARNING) << "HomedirMethods was already initialized";
    return;
  }
  g_homedir_methods = new HomedirMethodsImpl();
  VLOG(1) << "HomedirMethods initialized";
}

// static
void HomedirMethods::InitializeForTesting(HomedirMethods* homedir_methods) {
  if (g_homedir_methods) {
    LOG(WARNING) << "HomedirMethods was already initialized";
    return;
  }
  g_homedir_methods = homedir_methods;
  VLOG(1) << "HomedirMethods initialized";
}

// static
void HomedirMethods::Shutdown() {
  if (!g_homedir_methods) {
    LOG(WARNING) << "AsyncMethodCaller::Shutdown() called with NULL manager";
    return;
  }
  delete g_homedir_methods;
  g_homedir_methods = NULL;
  VLOG(1) << "HomedirMethods Shutdown completed";
}

// static
HomedirMethods* HomedirMethods::GetInstance() { return g_homedir_methods; }

}  // namespace cryptohome
