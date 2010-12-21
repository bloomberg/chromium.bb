// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_SYNC_TRAITS_H_
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_SYNC_TRAITS_H_
#pragma once

#include "chrome/browser/sync/syncable/model_type.h"

class Extension;
struct UninstalledExtensionInfo;

namespace sync_api {
class BaseNode;
class WriteNode;
}  // namespace sync_api

namespace sync_pb {
class EntitySpecifics;
class ExtensionSpecifics;
}  // namespace sync_pb

namespace browser_sync {

typedef bool (*IsValidAndSyncablePredicate)(const Extension&);

typedef bool (*ShouldHandleExtensionUninstallPredicate)(
    const UninstalledExtensionInfo&);

// ExtensionSpecificsGetter : BaseNode -> ExtensionSpecifics
typedef const sync_pb::ExtensionSpecifics& (*ExtensionSpecificsGetter)(
    const sync_api::BaseNode&);

// A function that sets the appropriate member of the given BaseNode
// to the given ExtensionSpecifics.
typedef void (*ExtensionSpecificsSetter)(
    const sync_pb::ExtensionSpecifics&, sync_api::WriteNode*);

// A function that tries to retrieve an ExtensionSpecifics from an
// EntitySpecifics.  Returns true and fills in the second argument iff
// successful.
typedef bool (*ExtensionSpecificsEntityGetter)(
    const sync_pb::EntitySpecifics&, sync_pb::ExtensionSpecifics*);

// Represents the properties needed for syncing data types that
// involve extensions (e.g., "real" extensions, apps).
struct ExtensionSyncTraits {
  ExtensionSyncTraits(
      syncable::ModelType model_type,
      IsValidAndSyncablePredicate is_valid_and_syncable,
      ShouldHandleExtensionUninstallPredicate
          should_handle_extension_uninstall,
      const char* root_node_tag,
      ExtensionSpecificsGetter extension_specifics_getter,
      ExtensionSpecificsSetter extension_specifics_setter,
      ExtensionSpecificsEntityGetter extension_specifics_entity_getter);
  ~ExtensionSyncTraits();

  // The sync type for the data type.
  const syncable::ModelType model_type;
  // A checker to make sure that the downloaded extension is valid and
  // syncable.
  const IsValidAndSyncablePredicate is_valid_and_syncable;
  // A checker to know which extension uninstall events to handle.
  const ShouldHandleExtensionUninstallPredicate
      should_handle_extension_uninstall;
  // The tag with which the top-level data type node is marked.
  const char* const root_node_tag;
  // The function that retrieves a ExtensionSpecifics reference (which
  // may be embedded in another specifics object) from a sync node.
  const ExtensionSpecificsGetter extension_specifics_getter;
  // The function that embeds an ExtensionSpecifics into a sync node.
  const ExtensionSpecificsSetter extension_specifics_setter;
  // The function that retrieves a ExtensionSpecifics (if possible)
  // from an EntitySpecifics.
  ExtensionSpecificsEntityGetter extension_specifics_entity_getter;
};

// Gets traits for extensions sync.
ExtensionSyncTraits GetExtensionSyncTraits();

// Gets traits for apps sync.
ExtensionSyncTraits GetAppSyncTraits();

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_SYNC_TRAITS_H_
