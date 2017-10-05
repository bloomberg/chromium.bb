// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/common/credential_manager_struct_traits.h"

#include "url/mojo/origin_struct_traits.h"
#include "url/mojo/url_gurl_struct_traits.h"

namespace mojo {

// static
password_manager::mojom::CredentialType EnumTraits<
    password_manager::mojom::CredentialType,
    password_manager::CredentialType>::ToMojom(password_manager::CredentialType
                                                   input) {
  switch (input) {
    case password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY:
      return password_manager::mojom::CredentialType::EMPTY;
    case password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD:
      return password_manager::mojom::CredentialType::PASSWORD;
    case password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED:
      return password_manager::mojom::CredentialType::FEDERATED;
  }

  NOTREACHED();
  return password_manager::mojom::CredentialType::EMPTY;
}

// static
bool EnumTraits<password_manager::mojom::CredentialType,
                password_manager::CredentialType>::
    FromMojom(password_manager::mojom::CredentialType input,
              password_manager::CredentialType* output) {
  switch (input) {
    case password_manager::mojom::CredentialType::EMPTY:
      *output = password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY;
      return true;
    case password_manager::mojom::CredentialType::PASSWORD:
      *output = password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD;
      return true;
    case password_manager::mojom::CredentialType::FEDERATED:
      *output = password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
password_manager::mojom::CredentialManagerError
EnumTraits<password_manager::mojom::CredentialManagerError,
           password_manager::CredentialManagerError>::
    ToMojom(password_manager::CredentialManagerError input) {
  switch (input) {
    case password_manager::CredentialManagerError::SUCCESS:
      return password_manager::mojom::CredentialManagerError::SUCCESS;
    case password_manager::CredentialManagerError::DISABLED:
      return password_manager::mojom::CredentialManagerError::DISABLED;
    case password_manager::CredentialManagerError::PENDINGREQUEST:
      return password_manager::mojom::CredentialManagerError::PENDINGREQUEST;
    case password_manager::CredentialManagerError::PASSWORDSTOREUNAVAILABLE:
      return password_manager::mojom::CredentialManagerError::
          PASSWORDSTOREUNAVAILABLE;
    case password_manager::CredentialManagerError::UNKNOWN:
      return password_manager::mojom::CredentialManagerError::UNKNOWN;
  }

  NOTREACHED();
  return password_manager::mojom::CredentialManagerError::UNKNOWN;
}

// static
bool EnumTraits<password_manager::mojom::CredentialManagerError,
                password_manager::CredentialManagerError>::
    FromMojom(password_manager::mojom::CredentialManagerError input,
              password_manager::CredentialManagerError* output) {
  switch (input) {
    case password_manager::mojom::CredentialManagerError::SUCCESS:
      *output = password_manager::CredentialManagerError::SUCCESS;
      return true;
    case password_manager::mojom::CredentialManagerError::DISABLED:
      *output = password_manager::CredentialManagerError::DISABLED;
      return true;
    case password_manager::mojom::CredentialManagerError::PENDINGREQUEST:
      *output = password_manager::CredentialManagerError::PENDINGREQUEST;
      return true;
    case password_manager::mojom::CredentialManagerError::
        PASSWORDSTOREUNAVAILABLE:
      *output =
          password_manager::CredentialManagerError::PASSWORDSTOREUNAVAILABLE;
      return true;
    case password_manager::mojom::CredentialManagerError::UNKNOWN:
      *output = password_manager::CredentialManagerError::UNKNOWN;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
password_manager::mojom::CredentialMediationRequirement
EnumTraits<password_manager::mojom::CredentialMediationRequirement,
           password_manager::CredentialMediationRequirement>::
    ToMojom(password_manager::CredentialMediationRequirement input) {
  switch (input) {
    case password_manager::CredentialMediationRequirement::kSilent:
      return password_manager::mojom::CredentialMediationRequirement::kSilent;
    case password_manager::CredentialMediationRequirement::kOptional:
      return password_manager::mojom::CredentialMediationRequirement::kOptional;
    case password_manager::CredentialMediationRequirement::kRequired:
      return password_manager::mojom::CredentialMediationRequirement::kRequired;
  }

  NOTREACHED();
  return password_manager::mojom::CredentialMediationRequirement::kOptional;
}

// static
bool EnumTraits<password_manager::mojom::CredentialMediationRequirement,
                password_manager::CredentialMediationRequirement>::
    FromMojom(password_manager::mojom::CredentialMediationRequirement input,
              password_manager::CredentialMediationRequirement* output) {
  switch (input) {
    case password_manager::mojom::CredentialMediationRequirement::kSilent:
      *output = password_manager::CredentialMediationRequirement::kSilent;
      return true;
    case password_manager::mojom::CredentialMediationRequirement::kOptional:
      *output = password_manager::CredentialMediationRequirement::kOptional;
      return true;
    case password_manager::mojom::CredentialMediationRequirement::kRequired:
      *output = password_manager::CredentialMediationRequirement::kRequired;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
bool StructTraits<password_manager::mojom::CredentialInfoDataView,
                  password_manager::CredentialInfo>::
    Read(password_manager::mojom::CredentialInfoDataView data,
         password_manager::CredentialInfo* out) {
  if (data.ReadType(&out->type) && data.ReadId(&out->id) &&
      data.ReadName(&out->name) && data.ReadIcon(&out->icon) &&
      data.ReadPassword(&out->password) &&
      data.ReadFederation(&out->federation))
    return true;

  return false;
}

}  // namespace mojo
