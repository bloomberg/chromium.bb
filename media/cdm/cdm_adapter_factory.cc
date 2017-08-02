// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_adapter_factory.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/cdm_factory.h"
#include "media/cdm/cdm_adapter.h"

namespace media {

CdmAdapterFactory::CdmAdapterFactory(
    CdmAllocator::CreationCB allocator_creation_cb)
    : allocator_creation_cb_(std::move(allocator_creation_cb)) {
  DCHECK(allocator_creation_cb_);
}

CdmAdapterFactory::~CdmAdapterFactory() {}

void CdmAdapterFactory::Create(
    const std::string& key_system,
    const GURL& security_origin,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    const CdmCreatedCB& cdm_created_cb) {
  DVLOG(1) << __FUNCTION__ << ": key_system=" << key_system;

  if (!security_origin.is_valid()) {
    LOG(ERROR) << "Invalid Origin: " << security_origin;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(cdm_created_cb, nullptr, "Invalid origin."));
    return;
  }

  std::unique_ptr<CdmAllocator> cdm_allocator = allocator_creation_cb_.Run();
  if (!cdm_allocator) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(cdm_created_cb, nullptr, "CDM allocator creation failed."));
    return;
  }

  // TODO(xhwang): Hook up auxiliary services, e.g. File IO, output protection,
  // and platform verification.
  CdmAdapter::Create(key_system, cdm_config, std::move(cdm_allocator),
                     CreateCdmFileIOCB(), session_message_cb, session_closed_cb,
                     session_keys_change_cb, session_expiration_update_cb,
                     cdm_created_cb);
}

}  // namespace media
