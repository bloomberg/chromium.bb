// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_chromeos.h"

#include <cert.h>

#include "base/bind.h"
#include "crypto/nss_crypto_module_delegate.h"
#include "crypto/nss_util_internal.h"

namespace net {

namespace {

typedef base::Callback<void(crypto::ScopedPK11Slot system_slot,
                            crypto::ScopedPK11Slot private_slot)>
    GetSystemAndPrivateSlotCallback;

// Gets the private slot for the user with the username hash |username_hash| and
// calls |callback| with both |system_slot| and the obtained private slot.
void GetPrivateSlotAndCallBack(const std::string& username_hash,
                               const GetSystemAndPrivateSlotCallback& callback,
                               crypto::ScopedPK11Slot system_slot) {
  base::Callback<void(crypto::ScopedPK11Slot)> wrapped_callback =
      base::Bind(callback, base::Passed(&system_slot));

  crypto::ScopedPK11Slot slot(
      crypto::GetPrivateSlotForChromeOSUser(username_hash, wrapped_callback));
  if (slot)
    wrapped_callback.Run(slot.Pass());
}

// Gets the system slot, then the private slot for the user with the username
// hash |username_hash|, and finally calls |callback| with both slots.
void GetSystemAndPrivateSlot(const std::string& username_hash,
                             const GetSystemAndPrivateSlotCallback& callback) {
  crypto::ScopedPK11Slot system_slot(crypto::GetSystemNSSKeySlot(
      base::Bind(&GetPrivateSlotAndCallBack, username_hash, callback)));
  if (system_slot)
    GetPrivateSlotAndCallBack(username_hash, callback, system_slot.Pass());
}

}  // namespace

ClientCertStoreChromeOS::ClientCertStoreChromeOS(
    bool use_system_slot,
    const std::string& username_hash,
    const PasswordDelegateFactory& password_delegate_factory)
    : ClientCertStoreNSS(password_delegate_factory),
      use_system_slot_(use_system_slot),
      username_hash_(username_hash) {
}

ClientCertStoreChromeOS::~ClientCertStoreChromeOS() {}

void ClientCertStoreChromeOS::GetClientCerts(
    const SSLCertRequestInfo& cert_request_info,
    CertificateList* selected_certs,
    const base::Closure& callback) {
  GetSystemAndPrivateSlotCallback bound_callback =
      base::Bind(&ClientCertStoreChromeOS::DidGetSystemAndPrivateSlot,
                 // Caller is responsible for keeping the ClientCertStore alive
                 // until the callback is run.
                 base::Unretained(this),
                 &cert_request_info,
                 selected_certs,
                 callback);

  if (use_system_slot_) {
    GetSystemAndPrivateSlot(username_hash_, bound_callback);
  } else {
    // Skip getting the system slot.
    GetPrivateSlotAndCallBack(
        username_hash_, bound_callback, crypto::ScopedPK11Slot());
  }
}

void ClientCertStoreChromeOS::GetClientCertsImpl(
    CERTCertList* cert_list,
    const SSLCertRequestInfo& request,
    bool query_nssdb,
    CertificateList* selected_certs) {
  ClientCertStoreNSS::GetClientCertsImpl(
      cert_list, request, query_nssdb, selected_certs);

  size_t pre_size = selected_certs->size();
  selected_certs->erase(
      std::remove_if(
          selected_certs->begin(),
          selected_certs->end(),
          NSSProfileFilterChromeOS::CertNotAllowedForProfilePredicate(
              profile_filter_)),
      selected_certs->end());
  DVLOG(1) << "filtered " << pre_size - selected_certs->size() << " of "
           << pre_size << " certs";
}

void ClientCertStoreChromeOS::DidGetSystemAndPrivateSlot(
    const SSLCertRequestInfo* request,
    CertificateList* selected_certs,
    const base::Closure& callback,
    crypto::ScopedPK11Slot system_slot,
    crypto::ScopedPK11Slot private_slot) {
  profile_filter_.Init(crypto::GetPublicSlotForChromeOSUser(username_hash_),
                       private_slot.Pass(),
                       system_slot.Pass());
  ClientCertStoreNSS::GetClientCerts(*request, selected_certs, callback);
}

}  // namespace net
