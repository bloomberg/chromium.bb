// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_PROTO_UTILS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_PROTO_UTILS_H_

#include <vector>
#include "components/offline_pages/core/prefetch/prefetch_types.h"

namespace offline_pages {

// The fully qualified type name for PageBundle defined in proto.
extern const char kPageBundleTypeURL[];

// Used to parse the Operation serialized in binary proto |data|.
bool ParseOperationResponse(const std::string& data,
                            std::vector<RenderPageInfo>* pages);

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_PROTO_UTILS_H_
