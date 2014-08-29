// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/homedir_methods.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"

#if defined(USE_SYSTEM_PROTOBUF)
#include <google/protobuf/repeated_field.h>
#else
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
#endif

using chromeos::DBusThreadManager;
using google::protobuf::RepeatedPtrField;

namespace cryptohome {

namespace {

HomedirMethods* g_homedir_methods = NULL;

void FillKeyProtobuf(const KeyDefinition& key_def, Key* key) {
  key->set_secret(key_def.key);
  KeyData* data = key->mutable_data();
  data->set_label(key_def.label);

  if (key_def.revision > 0)
    data->set_revision(key_def.revision);

  if (key_def.privileges != 0) {
    KeyPrivileges* privileges = data->mutable_privileges();
    privileges->set_mount(key_def.privileges & PRIV_MOUNT);
    privileges->set_add(key_def.privileges & PRIV_ADD);
    privileges->set_remove(key_def.privileges & PRIV_REMOVE);
    privileges->set_update(key_def.privileges & PRIV_MIGRATE);
    privileges->set_authorized_update(key_def.privileges &
                                      PRIV_AUTHORIZED_UPDATE);
  }

  if (key_def.encryption_key.empty() && key_def.signature_key.empty())
    return;

  KeyAuthorizationData* auth_data = data->add_authorization_data();
  auth_data->set_type(KeyAuthorizationData::KEY_AUTHORIZATION_TYPE_HMACSHA256);
  if (!key_def.encryption_key.empty()) {
    KeyAuthorizationSecret* secret = auth_data->add_secrets();
    secret->mutable_usage()->set_encrypt(true);
    secret->set_symmetric_key(key_def.encryption_key);
  }
  if (!key_def.signature_key.empty()) {
    KeyAuthorizationSecret* secret = auth_data->add_secrets();
    secret->mutable_usage()->set_sign(true);
    secret->set_symmetric_key(key_def.signature_key);
  }
}

// Fill identification protobuffer.
void FillIdentificationProtobuf(const Identification& id,
                                cryptohome::AccountIdentifier* id_proto) {
  id_proto->set_email(id.user_id);
}

// Fill authorization protobuffer.
void FillAuthorizationProtobuf(const Authorization& auth,
                               cryptohome::AuthorizationRequest* auth_proto) {
  Key* key = auth_proto->mutable_key();
  if (!auth.label.empty()) {
    key->mutable_data()->set_label(auth.label);
  }
  key->set_secret(auth.key);
}

MountError MapError(CryptohomeErrorCode code) {
  switch (code) {
    case CRYPTOHOME_ERROR_NOT_SET:
      return MOUNT_ERROR_NONE;
    case CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND:
      return MOUNT_ERROR_USER_DOES_NOT_EXIST;
    case CRYPTOHOME_ERROR_NOT_IMPLEMENTED:
    case CRYPTOHOME_ERROR_MOUNT_FATAL:
    case CRYPTOHOME_ERROR_KEY_QUOTA_EXCEEDED:
    case CRYPTOHOME_ERROR_BACKING_STORE_FAILURE:
      return MOUNT_ERROR_FATAL;
    case CRYPTOHOME_ERROR_AUTHORIZATION_KEY_NOT_FOUND:
    case CRYPTOHOME_ERROR_KEY_NOT_FOUND:
    case CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED:
      return MOUNT_ERROR_KEY_FAILURE;
    case CRYPTOHOME_ERROR_TPM_COMM_ERROR:
      return MOUNT_ERROR_TPM_COMM_ERROR;
    case CRYPTOHOME_ERROR_TPM_DEFEND_LOCK:
      return MOUNT_ERROR_TPM_DEFEND_LOCK;
    case CRYPTOHOME_ERROR_MOUNT_MOUNT_POINT_BUSY:
      return MOUNT_ERROR_MOUNT_POINT_BUSY;
    case CRYPTOHOME_ERROR_TPM_NEEDS_REBOOT:
      return MOUNT_ERROR_TPM_NEEDS_REBOOT;
    case CRYPTOHOME_ERROR_AUTHORIZATION_KEY_DENIED:
    case CRYPTOHOME_ERROR_KEY_LABEL_EXISTS:
    case CRYPTOHOME_ERROR_UPDATE_SIGNATURE_INVALID:
      return MOUNT_ERROR_KEY_FAILURE;
    default:
      NOTREACHED();
      return MOUNT_ERROR_FATAL;
  }
}

// The implementation of HomedirMethods
class HomedirMethodsImpl : public HomedirMethods {
 public:
  HomedirMethodsImpl() : weak_ptr_factory_(this) {}

  virtual ~HomedirMethodsImpl() {}

  virtual void GetKeyDataEx(const Identification& id,
                            const std::string& label,
                            const GetKeyDataCallback& callback) OVERRIDE {
    cryptohome::AccountIdentifier id_proto;
    cryptohome::AuthorizationRequest kEmptyAuthProto;
    cryptohome::GetKeyDataRequest request;

    FillIdentificationProtobuf(id, &id_proto);
    request.mutable_key()->mutable_data()->set_label(label);

    DBusThreadManager::Get()->GetCryptohomeClient()->GetKeyDataEx(
        id_proto,
        kEmptyAuthProto,
        request,
        base::Bind(&HomedirMethodsImpl::OnGetKeyDataExCallback,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  virtual void CheckKeyEx(const Identification& id,
                          const Authorization& auth,
                          const Callback& callback) OVERRIDE {
    cryptohome::AccountIdentifier id_proto;
    cryptohome::AuthorizationRequest auth_proto;
    cryptohome::CheckKeyRequest request;

    FillIdentificationProtobuf(id, &id_proto);
    FillAuthorizationProtobuf(auth, &auth_proto);

    DBusThreadManager::Get()->GetCryptohomeClient()->CheckKeyEx(
        id_proto,
        auth_proto,
        request,
        base::Bind(&HomedirMethodsImpl::OnBaseReplyCallback,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  virtual void MountEx(const Identification& id,
                       const Authorization& auth,
                       const MountParameters& request,
                       const MountCallback& callback) OVERRIDE {
    cryptohome::AccountIdentifier id_proto;
    cryptohome::AuthorizationRequest auth_proto;
    cryptohome::MountRequest request_proto;

    FillIdentificationProtobuf(id, &id_proto);
    FillAuthorizationProtobuf(auth, &auth_proto);

    if (request.ephemeral)
      request_proto.set_require_ephemeral(true);

    if (!request.create_keys.empty()) {
      CreateRequest* create = request_proto.mutable_create();
      for (size_t i = 0; i < request.create_keys.size(); ++i)
        FillKeyProtobuf(request.create_keys[i], create->add_keys());
    }

    DBusThreadManager::Get()->GetCryptohomeClient()->MountEx(
        id_proto,
        auth_proto,
        request_proto,
        base::Bind(&HomedirMethodsImpl::OnMountExCallback,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  virtual void AddKeyEx(const Identification& id,
                        const Authorization& auth,
                        const KeyDefinition& new_key,
                        bool clobber_if_exists,
                        const Callback& callback) OVERRIDE {
    cryptohome::AccountIdentifier id_proto;
    cryptohome::AuthorizationRequest auth_proto;
    cryptohome::AddKeyRequest request;

    FillIdentificationProtobuf(id, &id_proto);
    FillAuthorizationProtobuf(auth, &auth_proto);
    FillKeyProtobuf(new_key, request.mutable_key());
    request.set_clobber_if_exists(clobber_if_exists);

    DBusThreadManager::Get()->GetCryptohomeClient()->AddKeyEx(
        id_proto,
        auth_proto,
        request,
        base::Bind(&HomedirMethodsImpl::OnBaseReplyCallback,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  virtual void RemoveKeyEx(const Identification& id,
                           const Authorization& auth,
                           const std::string& label,
                           const Callback& callback) OVERRIDE {
    cryptohome::AccountIdentifier id_proto;
    cryptohome::AuthorizationRequest auth_proto;
    cryptohome::RemoveKeyRequest request;

    FillIdentificationProtobuf(id, &id_proto);
    FillAuthorizationProtobuf(auth, &auth_proto);
    request.mutable_key()->mutable_data()->set_label(label);

    DBusThreadManager::Get()->GetCryptohomeClient()->RemoveKeyEx(
        id_proto,
        auth_proto,
        request,
        base::Bind(&HomedirMethodsImpl::OnBaseReplyCallback,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  virtual void UpdateKeyEx(const Identification& id,
                           const Authorization& auth,
                           const KeyDefinition& new_key,
                           const std::string& signature,
                           const Callback& callback) OVERRIDE {
    cryptohome::AccountIdentifier id_proto;
    cryptohome::AuthorizationRequest auth_proto;
    cryptohome::UpdateKeyRequest pb_update_key;

    FillIdentificationProtobuf(id, &id_proto);
    FillAuthorizationProtobuf(auth, &auth_proto);
    FillKeyProtobuf(new_key, pb_update_key.mutable_changes());
    pb_update_key.set_authorization_signature(signature);

    DBusThreadManager::Get()->GetCryptohomeClient()->UpdateKeyEx(
        id_proto,
        auth_proto,
        pb_update_key,
        base::Bind(&HomedirMethodsImpl::OnBaseReplyCallback,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

 private:
  void OnGetKeyDataExCallback(const GetKeyDataCallback& callback,
                              chromeos::DBusMethodCallStatus call_status,
                              bool result,
                              const BaseReply& reply) {
    if (call_status != chromeos::DBUS_METHOD_CALL_SUCCESS) {
      callback.Run(false, MOUNT_ERROR_FATAL, ScopedVector<RetrievedKeyData>());
      return;
    }
    if (reply.has_error()) {
      if (reply.error() != CRYPTOHOME_ERROR_NOT_SET) {
        callback.Run(false,
                     MapError(reply.error()),
                     ScopedVector<RetrievedKeyData>());
        return;
      }
    }

    if (!reply.HasExtension(GetKeyDataReply::reply)) {
      callback.Run(false, MOUNT_ERROR_FATAL, ScopedVector<RetrievedKeyData>());
      return;
    }

    // Extract the contents of the |KeyData| protos returned.
    const RepeatedPtrField<KeyData>& key_data_proto =
        reply.GetExtension(GetKeyDataReply::reply).key_data();
    ScopedVector<RetrievedKeyData> key_data_list;
    for (RepeatedPtrField<KeyData>::const_iterator it = key_data_proto.begin();
         it != key_data_proto.end(); ++it) {

      // Extract |type|, |label| and |revision|.
      DCHECK_EQ(KeyData::KEY_TYPE_PASSWORD, it->type());
      key_data_list.push_back(new RetrievedKeyData(
          RetrievedKeyData::TYPE_PASSWORD,
          it->label(),
          it->revision()));
      RetrievedKeyData* key_data = key_data_list.back();

      // Extract |privileges|.
      const KeyPrivileges& privileges = it->privileges();
      if (privileges.mount())
        key_data->privileges |= PRIV_MOUNT;
      if (privileges.add())
        key_data->privileges |= PRIV_ADD;
      if (privileges.remove())
        key_data->privileges |= PRIV_REMOVE;
      if (privileges.update())
        key_data->privileges |= PRIV_MIGRATE;
      if (privileges.authorized_update())
        key_data->privileges |= PRIV_AUTHORIZED_UPDATE;

      // Extract |authorization_data|.
      for (RepeatedPtrField<KeyAuthorizationData>::const_iterator auth_it =
               it->authorization_data().begin();
           auth_it != it->authorization_data().end(); ++auth_it) {
        switch (auth_it->type()) {
          case KeyAuthorizationData::KEY_AUTHORIZATION_TYPE_HMACSHA256:
            key_data->authorization_types.push_back(
                RetrievedKeyData::AUTHORIZATION_TYPE_HMACSHA256);
            break;
          case KeyAuthorizationData::
                   KEY_AUTHORIZATION_TYPE_AES256CBC_HMACSHA256:
            key_data->authorization_types.push_back(
                RetrievedKeyData::AUTHORIZATION_TYPE_AES256CBC_HMACSHA256);
            break;
          default:
            NOTREACHED();
            break;
        }
      }

      // Extract |provider_data|.
      for (RepeatedPtrField<KeyProviderData::Entry>::const_iterator
              provider_data_it = it->provider_data().entry().begin();
           provider_data_it != it->provider_data().entry().end();
           ++provider_data_it) {
        // Extract |name|.
        key_data->provider_data.push_back(
            new RetrievedKeyData::ProviderData(provider_data_it->name()));
        RetrievedKeyData::ProviderData* provider_data =
            key_data->provider_data.back();

        int data_items = 0;

        // Extract |number|.
        if (provider_data_it->has_number()) {
          provider_data->number.reset(new int64(provider_data_it->number()));
          ++data_items;
        }

        // Extract |bytes|.
        if (provider_data_it->has_bytes()) {
          provider_data->bytes.reset(
              new std::string(provider_data_it->bytes()));
          ++data_items;
        }

        DCHECK_EQ(1, data_items);
      }
    }

    callback.Run(true, MOUNT_ERROR_NONE, key_data_list.Pass());
  }

  void OnMountExCallback(const MountCallback& callback,
                         chromeos::DBusMethodCallStatus call_status,
                         bool result,
                         const BaseReply& reply) {
    if (call_status != chromeos::DBUS_METHOD_CALL_SUCCESS) {
      callback.Run(false, MOUNT_ERROR_FATAL, std::string());
      return;
    }
    if (reply.has_error()) {
      if (reply.error() != CRYPTOHOME_ERROR_NOT_SET) {
        callback.Run(false, MapError(reply.error()), std::string());
        return;
      }
    }
    if (!reply.HasExtension(MountReply::reply)) {
      callback.Run(false, MOUNT_ERROR_FATAL, std::string());
      return;
    }

    std::string mount_hash;
    mount_hash = reply.GetExtension(MountReply::reply).sanitized_username();
    callback.Run(true, MOUNT_ERROR_NONE, mount_hash);
  }

  void OnBaseReplyCallback(const Callback& callback,
                           chromeos::DBusMethodCallStatus call_status,
                           bool result,
                           const BaseReply& reply) {
    if (call_status != chromeos::DBUS_METHOD_CALL_SUCCESS) {
      callback.Run(false, MOUNT_ERROR_FATAL);
      return;
    }
    if (reply.has_error()) {
      if (reply.error() != CRYPTOHOME_ERROR_NOT_SET) {
        callback.Run(false, MapError(reply.error()));
        return;
      }
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
