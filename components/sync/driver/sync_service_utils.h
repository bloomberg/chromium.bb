// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_UTILS_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_UTILS_H_

#include "components/sync/base/model_type.h"

namespace syncer {

class SyncService;

// Indicates whether uploading of data to Google is enabled, i.e. the user has
// given consent to upload this data. Since this enum is used for logging
// histograms, entries must not be removed or reordered.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.sync
enum class UploadState {
  // Syncing is enabled in principle, but the sync service is still
  // initializing, so e.g. we don't know about any auth errors yet.
  INITIALIZING,
  // We are not syncing to Google, and the caller should assume that we do not
  // have consent to do so. This can have a number of reasons, e.g.: sync as a
  // whole is disabled, or the given model type is disabled, or we're in
  // "local sync" mode, or this data type is encrypted with a custom passphrase
  // (in which case we're technically still uploading, but Google can't inspect
  // the data), or we're in a persistent auth error state. As one special case
  // of an auth error, sync may be "paused" because the user signed out of the
  // content area.
  NOT_ACTIVE,
  // We're actively syncing data to Google servers, in a form that is readable
  // by Google.
  ACTIVE,
  // Used when logging histograms. Must have this exact name.
  kMaxValue = ACTIVE
};

// Returns whether |type| is being uploaded to Google. This is useful for
// features that depend on user consent for uploading data (e.g. history) to
// Google.
UploadState GetUploadToGoogleState(const SyncService* sync_service,
                                   ModelType type);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_UTILS_H_
