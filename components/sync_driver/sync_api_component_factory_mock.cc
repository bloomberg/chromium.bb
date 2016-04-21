// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/sync_api_component_factory_mock.h"

#include <utility>

#include "components/sync_driver/change_processor.h"
#include "components/sync_driver/local_device_info_provider_mock.h"
#include "components/sync_driver/model_associator.h"
#include "sync/api/attachments/attachment_store.h"
#include "sync/internal_api/public/attachments/attachment_service_impl.h"

using sync_driver::AssociatorInterface;
using sync_driver::ChangeProcessor;
using testing::_;
using testing::InvokeWithoutArgs;
using testing::Return;

SyncApiComponentFactoryMock::SyncApiComponentFactoryMock()
    : local_device_(new sync_driver::LocalDeviceInfoProviderMock()) {
}

SyncApiComponentFactoryMock::SyncApiComponentFactoryMock(
    AssociatorInterface* model_associator, ChangeProcessor* change_processor)
    : model_associator_(model_associator),
      change_processor_(change_processor),
      local_device_(new sync_driver::LocalDeviceInfoProviderMock()) {
  ON_CALL(*this, CreateBookmarkSyncComponents(_, _)).
      WillByDefault(
          InvokeWithoutArgs(
              this,
              &SyncApiComponentFactoryMock::MakeSyncComponents));
}

SyncApiComponentFactoryMock::~SyncApiComponentFactoryMock() {}

std::unique_ptr<syncer::AttachmentService>
SyncApiComponentFactoryMock::CreateAttachmentService(
    std::unique_ptr<syncer::AttachmentStoreForSync> attachment_store,
    const syncer::UserShare& user_share,
    const std::string& store_birthday,
    syncer::ModelType model_type,
    syncer::AttachmentService::Delegate* delegate) {
  return syncer::AttachmentServiceImpl::CreateForTest();
}

sync_driver::SyncApiComponentFactory::SyncComponents
SyncApiComponentFactoryMock::MakeSyncComponents() {
  return sync_driver::SyncApiComponentFactory::SyncComponents(
      model_associator_.release(), change_processor_.release());
}

std::unique_ptr<sync_driver::LocalDeviceInfoProvider>
SyncApiComponentFactoryMock::CreateLocalDeviceInfoProvider() {
  return std::move(local_device_);
}

void SyncApiComponentFactoryMock::SetLocalDeviceInfoProvider(
    std::unique_ptr<sync_driver::LocalDeviceInfoProvider> local_device) {
  local_device_ = std::move(local_device);
}
