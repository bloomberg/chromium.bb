// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/common_custom_types_struct_traits.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {

// static
common::mojom::ThreadPriority
EnumTraits<common::mojom::ThreadPriority, base::ThreadPriority>::ToMojom(
    base::ThreadPriority thread_priority) {
  switch (thread_priority) {
    case base::ThreadPriority::BACKGROUND:
      return common::mojom::ThreadPriority::BACKGROUND;
    case base::ThreadPriority::NORMAL:
      return common::mojom::ThreadPriority::NORMAL;
    case base::ThreadPriority::DISPLAY:
      return common::mojom::ThreadPriority::DISPLAY;
    case base::ThreadPriority::REALTIME_AUDIO:
      return common::mojom::ThreadPriority::REALTIME_AUDIO;
  }
  NOTREACHED();
  return common::mojom::ThreadPriority::BACKGROUND;
}

// static
bool EnumTraits<common::mojom::ThreadPriority, base::ThreadPriority>::FromMojom(
    common::mojom::ThreadPriority input,
    base::ThreadPriority* out) {
  switch (input) {
    case common::mojom::ThreadPriority::BACKGROUND:
      *out = base::ThreadPriority::BACKGROUND;
      return true;
    case common::mojom::ThreadPriority::NORMAL:
      *out = base::ThreadPriority::NORMAL;
      return true;
    case common::mojom::ThreadPriority::DISPLAY:
      *out = base::ThreadPriority::DISPLAY;
      return true;
    case common::mojom::ThreadPriority::REALTIME_AUDIO:
      *out = base::ThreadPriority::REALTIME_AUDIO;
      return true;
  }
  return false;
}

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
