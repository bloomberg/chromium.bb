// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/mock_helpers.h"

namespace media {

MockCdmAuxiliaryHelper::MockCdmAuxiliaryHelper(
    std::unique_ptr<CdmAllocator> allocator)
    : allocator_(std::move(allocator)) {}

MockCdmAuxiliaryHelper::~MockCdmAuxiliaryHelper() {}

cdm::Buffer* MockCdmAuxiliaryHelper::CreateCdmBuffer(size_t capacity) {
  return allocator_->CreateCdmBuffer(capacity);
}

std::unique_ptr<VideoFrameImpl> MockCdmAuxiliaryHelper::CreateCdmVideoFrame() {
  return allocator_->CreateCdmVideoFrame();
}

void MockCdmAuxiliaryHelper::QueryStatus(QueryStatusCB callback) {
  std::move(callback).Run(QueryStatusCalled(), 0, 0);
}

void MockCdmAuxiliaryHelper::EnableProtection(uint32_t desired_protection_mask,
                                              EnableProtectionCB callback) {
  std::move(callback).Run(EnableProtectionCalled(desired_protection_mask));
}

void MockCdmAuxiliaryHelper::ChallengePlatform(const std::string& service_id,
                                               const std::string& challenge,
                                               ChallengePlatformCB callback) {
  std::move(callback).Run(ChallengePlatformCalled(service_id, challenge), "",
                          "", "");
}

void MockCdmAuxiliaryHelper::GetStorageId(StorageIdCB callback) {
  std::move(callback).Run(GetStorageIdCalled());
}

}  // namespace media
