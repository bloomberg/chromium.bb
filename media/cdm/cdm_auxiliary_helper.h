// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_AUXILIARY_HELPER_H_
#define MEDIA_CDM_CDM_AUXILIARY_HELPER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/media_export.h"
#include "media/cdm/cdm_allocator.h"
#include "media/cdm/cdm_file_io.h"
#include "media/cdm/output_protection.h"
#include "media/cdm/platform_verification.h"

namespace media {

// Provides a wrapper on the auxiliary functions (CdmAllocator, CdmFileIO,
// OutputProtection, PlatformVerification) needed by the CDM. The default
// implementation does nothing -- it simply returns NULL, false, 0, etc.
// as required to meet the interface.
class MEDIA_EXPORT CdmAuxiliaryHelper : public CdmAllocator,
                                        public OutputProtection,
                                        public PlatformVerification {
 public:
  // Callback to create CdmAllocator for the created CDM.
  using CreationCB =
      base::RepeatingCallback<std::unique_ptr<CdmAuxiliaryHelper>()>;

  CdmAuxiliaryHelper();
  ~CdmAuxiliaryHelper() override;

  // Given |client|, create a CdmFileIO object and return it. Caller owns the
  // returned object, and should only destroy it after Close() has been called.
  virtual std::unique_ptr<CdmFileIO> CreateCdmFileIO(cdm::FileIOClient* client);

  // CdmAllocator implementation.
  cdm::Buffer* CreateCdmBuffer(size_t capacity) override;
  std::unique_ptr<VideoFrameImpl> CreateCdmVideoFrame() override;

  // OutputProtection implementation.
  void QueryStatus(QueryStatusCB callback) override;
  void EnableProtection(uint32_t desired_protection_mask,
                        EnableProtectionCB callback) override;

  // PlatformVerification implementation.
  void ChallengePlatform(const std::string& service_id,
                         const std::string& challenge,
                         ChallengePlatformCB callback) override;
  void GetStorageId(StorageIdCB callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CdmAuxiliaryHelper);
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_AUXILIARY_HELPER_H_
