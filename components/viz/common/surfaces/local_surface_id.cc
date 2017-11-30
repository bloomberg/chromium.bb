// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/local_surface_id.h"

#include "base/strings/stringprintf.h"

namespace viz {

std::string LocalSurfaceId::ToString() const {
  return base::StringPrintf("LocalSurfaceId(%d, %s" PRIu64 ")", parent_id_,
                            nonce_.ToString().c_str());
}

std::ostream& operator<<(std::ostream& out,
                         const LocalSurfaceId& local_surface_id) {
  return out << local_surface_id.ToString();
}

}  // namespace viz
