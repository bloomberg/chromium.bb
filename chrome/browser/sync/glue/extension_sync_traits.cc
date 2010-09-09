// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_sync_traits.h"

#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/protocol/app_specifics.pb.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"

namespace browser_sync {

ExtensionSyncTraits::ExtensionSyncTraits(
    syncable::ModelType model_type,
    const char* root_node_tag,
    const ExtensionTypeSet& allowed_extension_types,
    ExtensionSpecificsGetter extension_specifics_getter,
    ExtensionSpecificsSetter extension_specifics_setter,
    ExtensionSpecificsEntityGetter extension_specifics_entity_getter)
    : model_type(model_type),
      root_node_tag(root_node_tag),
      allowed_extension_types(allowed_extension_types),
      extension_specifics_getter(extension_specifics_getter),
      extension_specifics_setter(extension_specifics_setter),
      extension_specifics_entity_getter(extension_specifics_entity_getter) {}

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

}  // namespace

ExtensionSyncTraits GetExtensionSyncTraits() {
  ExtensionTypeSet allowed_extension_types;
  allowed_extension_types.insert(EXTENSION);
  allowed_extension_types.insert(UPDATEABLE_USER_SCRIPT);
  return ExtensionSyncTraits(syncable::EXTENSIONS,
                             "google_chrome_extensions",
                             allowed_extension_types,
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

}  // namespace

ExtensionSyncTraits GetAppSyncTraits() {
  ExtensionTypeSet allowed_extension_types;
  allowed_extension_types.insert(APP);
  return ExtensionSyncTraits(syncable::APPS,
                             "google_chrome_apps",
                             allowed_extension_types,
                             &GetExtensionSpecificsOfApp,
                             &SetExtensionSpecificsOfApp,
                             &GetExtensionSpecificsFromEntityOfApp);
}

}  // namespace browser_sync
