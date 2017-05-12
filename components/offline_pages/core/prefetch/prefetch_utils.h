// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_UTILS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_UTILS_H_

#include <vector>
#include "components/offline_pages/core/prefetch/prefetch_types.h"

namespace offline_pages {

namespace proto {
class Any;
}  // namespace proto

// The fully qualified type name for PageBundle defined in proto.
extern const char kPageBundleTypeURL[];

// Parse PageBundle data stored as Any proto data. True is returned when the
// parsing succeeds and the result pages are stored in |pages|.
bool ParsePageBundleInAnyData(const proto::Any& any_data,
                              std::vector<RenderPageInfo>* pages);

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_UTILS_H_
