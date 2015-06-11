// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "media/base/cdm_context.h"
#include "media/base/media_export.h"
#include "media/mojo/services/mojo_cdm_service.h"

namespace media {

// A class that creates, owns and manages all MojoCdmService instances.
class MEDIA_EXPORT MojoCdmServiceContext : public CdmContextProvider {
 public:
  MojoCdmServiceContext();
  ~MojoCdmServiceContext() override;

  // Creates a MojoCdmService for |key_system| and weakly binds it to |request|.
  // The created MojoCdmService is owned by |this|. The request will be dropped
  // if no MojoCdmService can be created, resulting in a connection error.
  void CreateCdmService(
      const mojo::String& key_system,
      const mojo::String& security_origin,
      int32_t cdm_id,
      mojo::InterfaceRequest<mojo::ContentDecryptionModule> request);

  // CdmContextProvider implementation.
  // The returned CdmContext can be destroyed at any time if the pipe is
  // disconnected.
  // TODO(xhwang): When implementing SetCdm(), make sure we never dereference
  // garbage. For example, use media::PlayerTracker.
  CdmContext* GetCdmContext(int32_t cdm_id) override;

  // Called when there is a connection error with |service|.
  void ServiceHadConnectionError(MojoCdmService* service);

 private:
  // A map between CDM ID and MojoCdmService. Owns all MojoCdmService created.
  base::ScopedPtrHashMap<int32_t, scoped_ptr<MojoCdmService>> services_;

  DISALLOW_COPY_AND_ASSIGN(MojoCdmServiceContext);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_
