// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/cdm_context.h"

namespace media {

class MojoCdmService;

// A class that creates, owns and manages all MojoCdmService instances.
class MojoCdmServiceContext : public CdmContextProvider {
 public:
  MojoCdmServiceContext();
  ~MojoCdmServiceContext() override;

  base::WeakPtr<MojoCdmServiceContext> GetWeakPtr();

  // Registers The |cdm_service| with |cdm_id|.
  void RegisterCdm(int cdm_id, MojoCdmService* cdm_service);

  // Unregisters the CDM. Must be called before the CDM is destroyed.
  void UnregisterCdm(int cdm_id);

  // CdmContextProvider implementation.
  // The returned CdmContext can be destroyed at any time if the pipe is
  // disconnected.
  // TODO(xhwang): When implementing SetCdm(), make sure we never dereference
  // garbage. For example, use media::PlayerTracker.
  CdmContext* GetCdmContext(int32_t cdm_id) override;

 private:
  // A map between CDM ID and MojoCdmService.
  std::map<int, MojoCdmService*> cdm_services_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MojoCdmServiceContext> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoCdmServiceContext);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_
