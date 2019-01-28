// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/manage_sync_settings_mediator.h"

#include "base/logging.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/material_cell_catalog_view_controller.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SyncDataTypeSectionIdentifier = kSectionIdentifierEnumZero,
};

}  // namespace

@implementation ManageSyncSettingsMediator

#pragma mark - Loads sync data type section

// Loads the sync data type section.
- (void)loadSyncDataTypeSection {
  TableViewModel* model = self.consumer.tableViewModel;
  [model addSectionWithIdentifier:SyncDataTypeSectionIdentifier];
}

#pragma mark - ManageSyncSettingsTableViewControllerModelDelegate

- (void)manageSyncSettingsTableViewControllerLoadModel:
    (id<ManageSyncSettingsConsumer>)controller {
  DCHECK_EQ(self.consumer, controller);
  [self loadSyncDataTypeSection];
}

@end
