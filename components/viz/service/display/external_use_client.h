// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EXTERNAL_USE_CLIENT_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EXTERNAL_USE_CLIENT_H_

#include <vector>

#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// Interface used by DisplayResourceProvider::LockSetForExternalUse to notify
// the external client about resource removal.
class VIZ_SERVICE_EXPORT ExternalUseClient {
 public:
  // Release cached data associated with the given |resource_ids|.
  virtual void ReleaseCachedResources(
      const std::vector<ResourceId>& resource_ids) = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EXTERNAL_USE_CLIENT_H_
