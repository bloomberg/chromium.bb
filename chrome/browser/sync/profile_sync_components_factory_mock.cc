// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"

using browser_sync::AssociatorInterface;
using browser_sync::ChangeProcessor;
using testing::_;
using testing::InvokeWithoutArgs;

ProfileSyncComponentsFactoryMock::ProfileSyncComponentsFactoryMock() {}

ProfileSyncComponentsFactoryMock::ProfileSyncComponentsFactoryMock(
    AssociatorInterface* model_associator, ChangeProcessor* change_processor)
    : model_associator_(model_associator),
      change_processor_(change_processor) {
  ON_CALL(*this, CreateBookmarkSyncComponents(_, _)).
      WillByDefault(
          InvokeWithoutArgs(
              this,
              &ProfileSyncComponentsFactoryMock::MakeSyncComponents));
  ON_CALL(*this, CreateSearchEngineSyncComponents(_, _)).
      WillByDefault(
          InvokeWithoutArgs(
              this,
              &ProfileSyncComponentsFactoryMock::MakeSyncComponents));
  ON_CALL(*this, CreateAppNotificationSyncComponents(_, _)).
      WillByDefault(
          InvokeWithoutArgs(
              this,
              &ProfileSyncComponentsFactoryMock::MakeSyncComponents));
}

ProfileSyncComponentsFactoryMock::~ProfileSyncComponentsFactoryMock() {}

ProfileSyncComponentsFactory::SyncComponents
    ProfileSyncComponentsFactoryMock::MakeSyncComponents() {
  return SyncComponents(model_associator_.release(),
                        change_processor_.release());
}
