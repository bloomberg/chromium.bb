// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_location_param_traits.h"

// Auto-generate IPC::ParamTraits<> code specializations for sending and
// receiving enum extensions::Manifest::Location over IPC.

#include "ipc/param_traits_size_macros.h"
namespace IPC {
#undef EXTENSIONS_COMMON_MANIFEST_LOCATION_PARAM_TRAITS_H_
#include "extensions/common/manifest_location_param_traits.h"
}  // namespace IPC

#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef EXTENSIONS_COMMON_MANIFEST_LOCATION_PARAM_TRAITS_H_
#include "extensions/common/manifest_location_param_traits.h"
}  // namespace IPC

#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef EXTENSIONS_COMMON_MANIFEST_LOCATION_PARAM_TRAITS_H_
#include "extensions/common/manifest_location_param_traits.h"
}  // namespace IPC

#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef EXTENSIONS_COMMON_MANIFEST_LOCATION_PARAM_TRAITS_H_
#include "extensions/common/manifest_location_param_traits.h"
}  // namespace IPC
