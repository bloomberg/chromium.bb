// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/theme_model_associator.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/theme_util.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/theme_specifics.pb.h"

namespace browser_sync {

namespace {

static const char kThemesTag[] = "google_chrome_themes";
static const char kCurrentThemeNodeTitle[] = "Current Theme";

static const char kNoThemesFolderError[] =
    "Server did not create the top-level themes node. We "
    "might be running against an out-of-date server.";

}  // namespace

ThemeModelAssociator::ThemeModelAssociator(
    ProfileSyncService* sync_service)
    : sync_service_(sync_service) {
  DCHECK(sync_service_);
}

ThemeModelAssociator::~ThemeModelAssociator() {}

bool ThemeModelAssociator::AssociateModels() {
  sync_api::WriteTransaction trans(sync_service_->GetUserShare());
  sync_api::ReadNode root(&trans);
  if (!root.InitByTagLookup(kThemesTag)) {
    LOG(ERROR) << kNoThemesFolderError;
    return false;
  }

  Profile* profile = sync_service_->profile();
  sync_api::WriteNode node(&trans);
  // TODO(akalin): When we have timestamps, we may want to do
  // something more intelligent than preferring the sync data over our
  // local data.
  if (node.InitByClientTagLookup(syncable::THEMES, kCurrentThemeClientTag)) {
    // Update the current theme from the sync data.
    // TODO(akalin): If the sync data does not have
    // use_system_theme_by_default and we do, update that flag on the
    // sync data.
    sync_pb::ThemeSpecifics theme_specifics = node.GetThemeSpecifics();
    if (UpdateThemeSpecificsOrSetCurrentThemeIfNecessary(profile,
                                                         &theme_specifics))
      node.SetThemeSpecifics(theme_specifics);
  } else {
    // Set the sync data from the current theme.
    sync_api::WriteNode node(&trans);
    if (!node.InitUniqueByCreation(syncable::THEMES, root,
                                   kCurrentThemeClientTag)) {
      LOG(ERROR) << "Could not create current theme node.";
      return false;
    }
    node.SetIsFolder(false);
    node.SetTitle(UTF8ToWide(kCurrentThemeNodeTitle));
    sync_pb::ThemeSpecifics theme_specifics;
    GetThemeSpecificsFromCurrentTheme(profile, &theme_specifics);
    node.SetThemeSpecifics(theme_specifics);
  }
  return true;
}

bool ThemeModelAssociator::DisassociateModels() {
  // We don't maintain any association state, so nothing to do.
  return true;
}

bool ThemeModelAssociator::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(has_nodes);
  *has_nodes = false;
  sync_api::ReadTransaction trans(sync_service_->GetUserShare());
  sync_api::ReadNode root(&trans);
  if (!root.InitByTagLookup(kThemesTag)) {
    LOG(ERROR) << kNoThemesFolderError;
    return false;
  }
  // The sync model has user created nodes iff the themes folder has
  // any children.
  *has_nodes = root.GetFirstChildId() != sync_api::kInvalidId;
  return true;
}

bool ThemeModelAssociator::CryptoReadyIfNecessary() {
  // We only access the cryptographer while holding a transaction.
  sync_api::ReadTransaction trans(sync_service_->GetUserShare());
  syncable::ModelTypeSet encrypted_types;
  sync_service_->GetEncryptedDataTypes(&encrypted_types);
  return encrypted_types.count(syncable::THEMES) == 0 ||
         sync_service_->IsCryptographerReady(&trans);
}

}  // namespace browser_sync
