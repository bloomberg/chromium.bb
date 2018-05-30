// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/timer/arc_timer_struct_traits.h"

#include <utility>

#include "base/posix/unix_domain_socket.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {

// static
arc::mojom::ClockId EnumTraits<arc::mojom::ClockId, int32_t>::ToMojom(
    int32_t clock_id) {
  switch (clock_id) {
    case CLOCK_REALTIME_ALARM:
      return arc::mojom::ClockId::REALTIME_ALARM;
    case CLOCK_BOOTTIME_ALARM:
      return arc::mojom::ClockId::BOOTTIME_ALARM;
  }
  NOTREACHED();
  return arc::mojom::ClockId::BOOTTIME_ALARM;
}

// static
bool EnumTraits<arc::mojom::ClockId, int32_t>::FromMojom(
    arc::mojom::ClockId input,
    int32_t* output) {
  switch (input) {
    case arc::mojom::ClockId::REALTIME_ALARM:
      *output = CLOCK_REALTIME_ALARM;
      return true;
    case arc::mojom::ClockId::BOOTTIME_ALARM:
      *output = CLOCK_BOOTTIME_ALARM;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
arc::mojom::ClockId
StructTraits<arc::mojom::CreateTimerRequestDataView, arc::CreateTimerRequest>::
    clock_id(const arc::CreateTimerRequest& arc_timer_request) {
  return EnumTraits<arc::mojom::ClockId, int32_t>::ToMojom(
      arc_timer_request.clock_id);
}

// static
mojo::ScopedHandle
StructTraits<arc::mojom::CreateTimerRequestDataView, arc::CreateTimerRequest>::
    expiration_fd(arc::CreateTimerRequest& arc_timer_request) {
  return WrapPlatformHandle(
      PlatformHandle(std::move(arc_timer_request.expiration_fd)));
}

// static
bool StructTraits<
    arc::mojom::CreateTimerRequestDataView,
    arc::CreateTimerRequest>::Read(arc::mojom::CreateTimerRequestDataView input,
                                   arc::CreateTimerRequest* output) {
  if (!EnumTraits<arc::mojom::ClockId, int32_t>::FromMojom(input.clock_id(),
                                                           &output->clock_id)) {
    return false;
  }
  output->expiration_fd =
      UnwrapPlatformHandle(input.TakeExpirationFd()).TakeFD();
  return true;
}

}  // namespace mojo
