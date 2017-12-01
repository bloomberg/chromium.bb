// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_helper.h"

#include "base/stl_util.h"
#include "media/base/scoped_callback_runner.h"
#include "media/cdm/cdm_helpers.h"
#include "media/mojo/services/mojo_cdm_allocator.h"
#include "media/mojo/services/mojo_cdm_file_io.h"
#include "services/service_manager/public/cpp/connect.h"

namespace media {

MojoCdmHelper::MojoCdmHelper(
    service_manager::mojom::InterfaceProvider* interface_provider)
    : interface_provider_(interface_provider), weak_factory_(this) {}

MojoCdmHelper::~MojoCdmHelper() = default;

void MojoCdmHelper::SetFileReadCB(FileReadCB file_read_cb) {
  file_read_cb_ = std::move(file_read_cb);
}

cdm::FileIO* MojoCdmHelper::CreateCdmFileIO(cdm::FileIOClient* client) {
  ConnectToCdmStorage();

  // Pass a reference to CdmStorage so that MojoCdmFileIO can open a file.
  auto mojo_cdm_file_io =
      std::make_unique<MojoCdmFileIO>(this, client, cdm_storage_ptr_.get());

  cdm::FileIO* cdm_file_io = mojo_cdm_file_io.get();
  DVLOG(3) << __func__ << ": cdm_file_io = " << cdm_file_io;

  cdm_file_io_set_.push_back(std::move(mojo_cdm_file_io));
  return cdm_file_io;
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
  ConnectToOutputProtection();
  output_protection_ptr_->QueryStatus(std::move(scoped_callback));
}

void MojoCdmHelper::EnableProtection(uint32_t desired_protection_mask,
                                     EnableProtectionCB callback) {
  EnableProtectionCB scoped_callback =
      ScopedCallbackRunner(std::move(callback), false);
  ConnectToOutputProtection();
  output_protection_ptr_->EnableProtection(desired_protection_mask,
                                           std::move(scoped_callback));
}

void MojoCdmHelper::ChallengePlatform(const std::string& service_id,
                                      const std::string& challenge,
                                      ChallengePlatformCB callback) {
  ChallengePlatformCB scoped_callback =
      ScopedCallbackRunner(std::move(callback), false, "", "", "");
  ConnectToPlatformVerification();
  platform_verification_ptr_->ChallengePlatform(service_id, challenge,
                                                std::move(scoped_callback));
}

void MojoCdmHelper::GetStorageId(uint32_t version, StorageIdCB callback) {
  StorageIdCB scoped_callback = ScopedCallbackRunner(
      std::move(callback), version, std::vector<uint8_t>());
  ConnectToPlatformVerification();
  platform_verification_ptr_->GetStorageId(version, std::move(scoped_callback));
}

void MojoCdmHelper::CloseCdmFileIO(MojoCdmFileIO* cdm_file_io) {
  DVLOG(3) << __func__ << ": cdm_file_io = " << cdm_file_io;
  base::EraseIf(cdm_file_io_set_,
                [cdm_file_io](const std::unique_ptr<MojoCdmFileIO>& ptr) {
                  return ptr.get() == cdm_file_io;
                });
}

void MojoCdmHelper::ReportFileReadSize(int file_size_bytes) {
  DVLOG(3) << __func__ << ": file_size_bytes = " << file_size_bytes;
  if (file_read_cb_)
    file_read_cb_.Run(file_size_bytes);
}

void MojoCdmHelper::ConnectToCdmStorage() {
  if (!cdm_storage_ptr_) {
    service_manager::GetInterface<mojom::CdmStorage>(interface_provider_,
                                                     &cdm_storage_ptr_);
  }
}

CdmAllocator* MojoCdmHelper::GetAllocator() {
  if (!allocator_)
    allocator_ = base::MakeUnique<MojoCdmAllocator>();
  return allocator_.get();
}

void MojoCdmHelper::ConnectToOutputProtection() {
  if (!output_protection_ptr_) {
    service_manager::GetInterface<mojom::OutputProtection>(
        interface_provider_, &output_protection_ptr_);
  }
}

void MojoCdmHelper::ConnectToPlatformVerification() {
  if (!platform_verification_ptr_) {
    service_manager::GetInterface<mojom::PlatformVerification>(
        interface_provider_, &platform_verification_ptr_);
  }
}

}  // namespace media
