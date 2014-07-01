// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "components/sync_driver/change_processor.h"
#include "components/sync_driver/model_associator.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/attachments/attachment_service_impl.h"
#include "sync/internal_api/public/attachments/fake_attachment_store.h"

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
}

ProfileSyncComponentsFactoryMock::~ProfileSyncComponentsFactoryMock() {}

scoped_ptr<syncer::AttachmentService>
ProfileSyncComponentsFactoryMock::CreateAttachmentService(
    const syncer::UserShare& user_share,
    syncer::AttachmentService::Delegate* delegate) {
  return syncer::AttachmentServiceImpl::CreateForTest();
}

ProfileSyncComponentsFactory::SyncComponents
ProfileSyncComponentsFactoryMock::MakeSyncComponents() {
  return SyncComponents(model_associator_.release(),
                        change_processor_.release());
}
