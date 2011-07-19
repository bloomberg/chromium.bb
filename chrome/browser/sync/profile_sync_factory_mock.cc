// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/profile_sync_factory_mock.h"

using browser_sync::AssociatorInterface;
using browser_sync::ChangeProcessor;
using testing::_;
using testing::InvokeWithoutArgs;

ProfileSyncFactoryMock::ProfileSyncFactoryMock() {}

ProfileSyncFactoryMock::ProfileSyncFactoryMock(
    AssociatorInterface* bookmark_model_associator,
    ChangeProcessor* bookmark_change_processor)
    : bookmark_model_associator_(bookmark_model_associator),
      bookmark_change_processor_(bookmark_change_processor) {
  ON_CALL(*this, CreateBookmarkSyncComponents(_, _)).
      WillByDefault(
          InvokeWithoutArgs(
              this,
              &ProfileSyncFactoryMock::MakeBookmarkSyncComponents));
}

ProfileSyncFactoryMock::~ProfileSyncFactoryMock() {}

ProfileSyncFactory::SyncComponents
ProfileSyncFactoryMock::MakeBookmarkSyncComponents() {
  return SyncComponents(bookmark_model_associator_.release(),
                        bookmark_change_processor_.release());
}
