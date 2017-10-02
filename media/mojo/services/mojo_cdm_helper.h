// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_HELPER_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_HELPER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "media/cdm/cdm_auxiliary_helper.h"
#include "media/mojo/interfaces/output_protection.mojom.h"
#include "media/mojo/interfaces/platform_verification.mojom.h"
#include "media/mojo/services/media_mojo_export.h"

namespace service_manager {
namespace mojom {
class InterfaceProvider;
}
}  // namespace service_manager

namespace media {

// Helper class that connects the CDM to various auxiliary services. All
// additional services (FileIO, memory allocation, output protection, and
// platform verification) are lazily created.
class MEDIA_MOJO_EXPORT MojoCdmHelper final : public CdmAuxiliaryHelper {
 public:
  explicit MojoCdmHelper(
      service_manager::mojom::InterfaceProvider* interface_provider);
  ~MojoCdmHelper() final;

  // CdmAuxiliaryHelper implementation.
  std::unique_ptr<media::CdmFileIO> CreateCdmFileIO(
      cdm::FileIOClient* client) final;
  cdm::Buffer* CreateCdmBuffer(size_t capacity) final;
  std::unique_ptr<VideoFrameImpl> CreateCdmVideoFrame() final;
  void QueryStatus(QueryStatusCB callback) final;
  void EnableProtection(uint32_t desired_protection_mask,
                        EnableProtectionCB callback) final;
  void ChallengePlatform(const std::string& service_id,
                         const std::string& challenge,
                         ChallengePlatformCB callback) final;
  void GetStorageId(uint32_t version, StorageIdCB callback) final;

 private:
  // All services are created lazily.
  CdmAllocator* GetAllocator();
  bool ConnectToOutputProtection();
  bool ConnectToPlatformVerification();

  // Provides interfaces when needed.
  service_manager::mojom::InterfaceProvider* interface_provider_;

  // Keep track if connection to the Mojo service has been attempted once.
  // The service may not exist, or may fail later.
  bool output_protection_attempted_ = false;
  bool platform_verification_attempted_ = false;

  std::unique_ptr<media::CdmAllocator> allocator_;
  media::mojom::OutputProtectionPtr output_protection_;
  media::mojom::PlatformVerificationPtr platform_verification_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_HELPER_H_
