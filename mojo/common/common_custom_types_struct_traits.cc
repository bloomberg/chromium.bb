// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/common_custom_types_struct_traits.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {

// static
bool StructTraits<common::mojom::MemoryAllocatorDumpCrossProcessUidDataView,
                  base::trace_event::MemoryAllocatorDumpGuid>::
    Read(common::mojom::MemoryAllocatorDumpCrossProcessUidDataView data,
         base::trace_event::MemoryAllocatorDumpGuid* out) {
  // Receiving a zeroed MemoryAllocatorDumpCrossProcessUid is a bug.
  if (data.value() == 0)
    return false;

  *out = base::trace_event::MemoryAllocatorDumpGuid(data.value());
  return true;
}

}  // namespace mojo
