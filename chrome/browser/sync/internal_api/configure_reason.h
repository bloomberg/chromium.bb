// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_INTERNAL_API_CONFIGURE_REASON_H_
#define CHROME_BROWSER_SYNC_INTERNAL_API_CONFIGURE_REASON_H_
#pragma once

namespace sync_api {

// Note: This should confirm with the enums in sync.proto for
// GetUpdatesCallerInfo. They will have 1:1 mapping but this will only map
// to a subset of the GetUpdatesCallerInfo enum values.
enum ConfigureReason {
  // We should never be here during actual configure. This is for setting
  // default values.
  CONFIGURE_REASON_UNKNOWN,

  // The client is configuring because the user opted to sync a different set
  // of datatypes.
  CONFIGURE_REASON_RECONFIGURATION,

  // The client is configuring because the client is being asked to migrate.
  CONFIGURE_REASON_MIGRATION,

  // Setting up sync performs an initial config to download NIGORI data, and
  // also a config to download initial data once the user selects types.
  CONFIGURE_REASON_NEW_CLIENT,

  // A new datatype is enabled for syncing due to a client upgrade.
  CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE,
};

} // namespace sync_api

#endif  // CHROME_BROWSER_SYNC_INTERNAL_API_CONFIGURE_REASON_H_
