// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_ADAPTER_FACTORY_H_
#define MEDIA_CDM_CDM_ADAPTER_FACTORY_H_

#include "base/macros.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_export.h"
#include "media/cdm/cdm_allocator.h"

namespace media {

class MEDIA_EXPORT CdmAdapterFactory final : public CdmFactory {
 public:
  explicit CdmAdapterFactory(CdmAllocator::CreationCB allocator_creation_cb);
  ~CdmAdapterFactory() override;

  // CdmFactory implementation.
  void Create(const std::string& key_system,
              const GURL& security_origin,
              const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              const CdmCreatedCB& cdm_created_cb) override;

 private:
  // Callback to create CdmAllocator for the created CDM.
  CdmAllocator::CreationCB allocator_creation_cb_;

  DISALLOW_COPY_AND_ASSIGN(CdmAdapterFactory);
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_ADAPTER_FACTORY_H_
