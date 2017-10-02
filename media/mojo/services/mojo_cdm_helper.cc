// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_helper.h"

#include "media/base/scoped_callback_runner.h"
#include "media/cdm/cdm_helpers.h"
#include "media/mojo/services/mojo_cdm_allocator.h"
#include "services/service_manager/public/cpp/connect.h"

namespace media {

MojoCdmHelper::MojoCdmHelper(
    service_manager::mojom::InterfaceProvider* interface_provider)
    : interface_provider_(interface_provider) {}

MojoCdmHelper::~MojoCdmHelper() {}

std::unique_ptr<CdmFileIO> MojoCdmHelper::CreateCdmFileIO(
    cdm::FileIOClient* client) {
  // TODO(jrummell): Hook up File IO. http://crbug.com/479923.
  return nullptr;
}

cdm::Buffer* MojoCdmHelper::CreateCdmBuffer(size_t capacity) {
  return GetAllocator()->CreateCdmBuffer(capacity);
}

std::unique_ptr<VideoFrameImpl> MojoCdmHelper::CreateCdmVideoFrame() {
  return GetAllocator()->CreateCdmVideoFrame();
}

void MojoCdmHelper::QueryStatus(QueryStatusCB callback) {
  QueryStatusCB scoped_callback =
      ScopedCallbackRunner(std::move(callback), false, 0, 0);
  if (!ConnectToOutputProtection())
    return;

  output_protection_->QueryStatus(std::move(scoped_callback));
}

void MojoCdmHelper::EnableProtection(uint32_t desired_protection_mask,
                                     EnableProtectionCB callback) {
  EnableProtectionCB scoped_callback =
      ScopedCallbackRunner(std::move(callback), false);
  if (!ConnectToOutputProtection())
    return;

  output_protection_->EnableProtection(desired_protection_mask,
                                       std::move(scoped_callback));
}

void MojoCdmHelper::ChallengePlatform(const std::string& service_id,
                                      const std::string& challenge,
                                      ChallengePlatformCB callback) {
  ChallengePlatformCB scoped_callback =
      ScopedCallbackRunner(std::move(callback), false, "", "", "");
  if (!ConnectToPlatformVerification())
    return;

  platform_verification_->ChallengePlatform(service_id, challenge,
                                            std::move(scoped_callback));
}

void MojoCdmHelper::GetStorageId(uint32_t version, StorageIdCB callback) {
  StorageIdCB scoped_callback = ScopedCallbackRunner(
      std::move(callback), version, std::vector<uint8_t>());
  // TODO(jrummell): Hook up GetStorageId() once added to the mojo interface.
  // http://crbug.com/478960.
}

CdmAllocator* MojoCdmHelper::GetAllocator() {
  if (!allocator_)
    allocator_ = base::MakeUnique<MojoCdmAllocator>();
  return allocator_.get();
}

bool MojoCdmHelper::ConnectToOutputProtection() {
  if (!output_protection_attempted_) {
    output_protection_attempted_ = true;
    service_manager::GetInterface<mojom::OutputProtection>(interface_provider_,
                                                           &output_protection_);
  }
  return output_protection_.is_bound();
}

bool MojoCdmHelper::ConnectToPlatformVerification() {
  if (!platform_verification_attempted_) {
    platform_verification_attempted_ = true;
    service_manager::GetInterface<mojom::PlatformVerification>(
        interface_provider_, &platform_verification_);
  }
  return platform_verification_.is_bound();
}

}  // namespace media
