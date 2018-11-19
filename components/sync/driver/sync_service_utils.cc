// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service_utils.h"

#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace syncer {

UploadState GetUploadToGoogleState(const SyncService* sync_service,
                                   ModelType type) {
  // Note: Before configuration is done, GetPreferredDataTypes returns
  // "everything" (i.e. the default setting). If a data type is missing there,
  // it must be because the user explicitly disabled it.
  if (!sync_service || sync_service->IsLocalSyncEnabled() ||
      !sync_service->CanSyncFeatureStart() ||
      !sync_service->GetPreferredDataTypes().Has(type)) {
    return UploadState::NOT_ACTIVE;
  }

  // If the given ModelType is encrypted with a custom passphrase, we also
  // consider uploading inactive, since Google can't read the data.
  // Note that encryption is tricky: Some data types (e.g. PASSWORDS) are always
  // encrypted, but not necessarily with a custom passphrase. On the other hand,
  // some data types are never encrypted (e.g. DEVICE_INFO), even if the
  // "encrypt everything" setting is enabled.
  if (sync_service->GetEncryptedDataTypes().Has(type) &&
      sync_service->IsUsingSecondaryPassphrase()) {
    return UploadState::NOT_ACTIVE;
  }

  // Persistent auth errors always map to NOT_ACTIVE. For transient errors, we
  // give the benefit of the doubt and may still say we're INITIALIZING.
  if (sync_service->GetAuthError().IsPersistentError()) {
    return UploadState::NOT_ACTIVE;
  }

  switch (sync_service->GetTransportState()) {
    case SyncService::TransportState::DISABLED:
      return UploadState::NOT_ACTIVE;

    case SyncService::TransportState::WAITING_FOR_START_REQUEST:
    case SyncService::TransportState::START_DEFERRED:
    case SyncService::TransportState::INITIALIZING:
    case SyncService::TransportState::PENDING_DESIRED_CONFIGURATION:
    case SyncService::TransportState::CONFIGURING:
      return UploadState::INITIALIZING;

    case SyncService::TransportState::ACTIVE:
      // If sync is active, but the data type in question still isn't, then
      // something must have gone wrong with that data type.
      if (!sync_service->GetActiveDataTypes().Has(type)) {
        return UploadState::NOT_ACTIVE;
      }
      if (sync_service->GetAuthError().IsTransientError()) {
        return UploadState::INITIALIZING;
      }
      // TODO(crbug.com/831579): We currently need to wait for
      // GetLastCycleSnapshot to return an initialized snapshot because we don't
      // actually know if the token is valid until sync has tried it. This is
      // bad because sync can take arbitrarily long to try the token (especially
      // if the user doesn't have history sync enabled). Instead, if the
      // identity code would persist persistent auth errors, we could read those
      // from startup.
      if (!sync_service->GetLastCycleSnapshot().is_initialized()) {
        return UploadState::INITIALIZING;
      }
      return UploadState::ACTIVE;
  }
  NOTREACHED();
  return UploadState::NOT_ACTIVE;
}

}  // namespace syncer
