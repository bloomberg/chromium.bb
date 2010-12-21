// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_sync_traits.h"

#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/extension_util.h"
#include "chrome/browser/sync/protocol/app_specifics.pb.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "chrome/common/extensions/extension.h"

namespace browser_sync {

ExtensionSyncTraits::ExtensionSyncTraits(
    syncable::ModelType model_type,
    IsValidAndSyncablePredicate is_valid_and_syncable,
    ShouldHandleExtensionUninstallPredicate
        should_handle_extension_uninstall,
    const char* root_node_tag,
    ExtensionSpecificsGetter extension_specifics_getter,
    ExtensionSpecificsSetter extension_specifics_setter,
    ExtensionSpecificsEntityGetter extension_specifics_entity_getter)
    : model_type(model_type),
      is_valid_and_syncable(is_valid_and_syncable),
      should_handle_extension_uninstall(should_handle_extension_uninstall),
      root_node_tag(root_node_tag),
      extension_specifics_getter(extension_specifics_getter),
      extension_specifics_setter(extension_specifics_setter),
      extension_specifics_entity_getter(extension_specifics_entity_getter) {}

ExtensionSyncTraits::~ExtensionSyncTraits() {}

namespace {

const sync_pb::ExtensionSpecifics& GetExtensionSpecifics(
        const sync_api::BaseNode& node) {
  return node.GetExtensionSpecifics();
}

void SetExtensionSpecifics(
    const sync_pb::ExtensionSpecifics& extension_specifics,
    sync_api::WriteNode* node) {
  node->SetTitle(UTF8ToWide(extension_specifics.name()));
  node->SetExtensionSpecifics(extension_specifics);
}

bool GetExtensionSpecificsFromEntity(
    const sync_pb::EntitySpecifics& entity_specifics,
    sync_pb::ExtensionSpecifics* extension_specifics) {
  if (!entity_specifics.HasExtension(sync_pb::extension)) {
    return false;
  }
  *extension_specifics = entity_specifics.GetExtension(sync_pb::extension);
  return true;
}

bool IsSyncableExtension(Extension::Type type, const GURL& update_url) {
  switch (type) {
    case Extension::TYPE_EXTENSION:
      return true;
    case Extension::TYPE_USER_SCRIPT:
      // We only want to sync user scripts with update URLs.
      return !update_url.is_empty();
    default:
      return false;
  }
}

bool IsValidAndSyncableExtension(const Extension& extension) {
  return
      IsExtensionValid(extension) &&
      IsSyncableExtension(extension.GetType(), extension.update_url());
}

bool IsExtensionUninstall(
    const UninstalledExtensionInfo& uninstalled_extension_info) {
  return IsSyncableExtension(uninstalled_extension_info.extension_type,
                             uninstalled_extension_info.update_url);
}

}  // namespace

ExtensionSyncTraits GetExtensionSyncTraits() {
  return ExtensionSyncTraits(syncable::EXTENSIONS,
                             &IsValidAndSyncableExtension,
                             &IsExtensionUninstall,
                             "google_chrome_extensions",
                             &GetExtensionSpecifics,
                             &SetExtensionSpecifics,
                             &GetExtensionSpecificsFromEntity);
}

namespace {

const sync_pb::ExtensionSpecifics& GetExtensionSpecificsOfApp(
        const sync_api::BaseNode& node) {
  return node.GetAppSpecifics().extension();
}

void SetExtensionSpecificsOfApp(
    const sync_pb::ExtensionSpecifics& extension_specifics,
    sync_api::WriteNode* node) {
  node->SetTitle(UTF8ToWide(extension_specifics.name()));
  sync_pb::AppSpecifics app_specifics;
  *app_specifics.mutable_extension() = extension_specifics;
  node->SetAppSpecifics(app_specifics);
}

bool GetExtensionSpecificsFromEntityOfApp(
    const sync_pb::EntitySpecifics& entity_specifics,
    sync_pb::ExtensionSpecifics* extension_specifics) {
  if (!entity_specifics.HasExtension(sync_pb::app)) {
    return false;
  }
  *extension_specifics =
      entity_specifics.GetExtension(sync_pb::app).extension();
  return true;
}

bool IsSyncableApp(Extension::Type type) {
  return
      (type == Extension::TYPE_HOSTED_APP) ||
      (type == Extension::TYPE_PACKAGED_APP);
}

bool IsValidAndSyncableApp(
    const Extension& extension) {
  return IsExtensionValid(extension) && IsSyncableApp(extension.GetType());
}

bool IsAppUninstall(
    const UninstalledExtensionInfo& uninstalled_extension_info) {
  return IsSyncableApp(uninstalled_extension_info.extension_type);
}

}  // namespace

ExtensionSyncTraits GetAppSyncTraits() {
  return ExtensionSyncTraits(syncable::APPS,
                             &IsValidAndSyncableApp,
                             &IsAppUninstall,
                             "google_chrome_apps",
                             &GetExtensionSpecificsOfApp,
                             &SetExtensionSpecificsOfApp,
                             &GetExtensionSpecificsFromEntityOfApp);
}

}  // namespace browser_sync
