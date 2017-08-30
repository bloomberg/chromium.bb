// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_auxiliary_helper.h"

#include "media/cdm/cdm_helpers.h"

namespace media {

CdmAuxiliaryHelper::CdmAuxiliaryHelper() {}
CdmAuxiliaryHelper::~CdmAuxiliaryHelper() {}

std::unique_ptr<CdmFileIO> CdmAuxiliaryHelper::CreateCdmFileIO(
    cdm::FileIOClient* client) {
  return nullptr;
}

cdm::Buffer* CdmAuxiliaryHelper::CreateCdmBuffer(size_t capacity) {
  return nullptr;
}

std::unique_ptr<VideoFrameImpl> CdmAuxiliaryHelper::CreateCdmVideoFrame() {
  return nullptr;
}

void CdmAuxiliaryHelper::QueryStatus(QueryStatusCB callback) {
  std::move(callback).Run(false, 0, 0);
}

void CdmAuxiliaryHelper::EnableProtection(uint32_t desired_protection_mask,
                                          EnableProtectionCB callback) {
  std::move(callback).Run(false);
}

void CdmAuxiliaryHelper::ChallengePlatform(const std::string& service_id,
                                           const std::string& challenge,
                                           ChallengePlatformCB callback) {
  std::move(callback).Run(false, "", "", "");
}

void CdmAuxiliaryHelper::GetStorageId(StorageIdCB callback) {
  std::move(callback).Run(std::vector<uint8_t>());
}

}  // namespace media
