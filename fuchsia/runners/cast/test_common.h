// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_TEST_COMMON_H_
#define FUCHSIA_RUNNERS_CAST_TEST_COMMON_H_

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/strings/string_piece.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"

namespace fuchsia {
namespace io {
class Directory;
}
}  // namespace fuchsia

// Starts a Cast component from the runner |sys_runner| with the URL |cast_url|
// and returns the outgoing service directory client channel.
// The Cast component will connect to the CastChannel FIDL service bound at
// |cast_channel_binding|.
// Blocks until |cast_channel_binding| is bound.
fidl::InterfaceHandle<fuchsia::io::Directory> StartCastComponent(
    const base::StringPiece& cast_url,
    fuchsia::sys::RunnerPtr* sys_runner,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request,
    fidl::Binding<chromium::cast::CastChannel>* cast_channel_binding);

#endif  // FUCHSIA_RUNNERS_CAST_TEST_COMMON_H_
