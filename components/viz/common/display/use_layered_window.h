// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_DISPLAY_USE_LAYERED_WINDOW_H_
#define COMPONENTS_VIZ_COMMON_DISPLAY_USE_LAYERED_WINDOW_H_

#include <windows.h>

#include "components/viz/common/viz_common_export.h"

namespace viz {

// Layered windows are a legacy way of supporting transparency for HWNDs. With
// Desktop Window Manager (DWM) HWNDs support transparency natively. DWM is
// always enabled on Windows 8 and later. However, for Windows 7 (and earlier)
// DWM might be disabled and layered windows are necessary to guarantee the HWND
// will support transparency.
VIZ_COMMON_EXPORT bool NeedsToUseLayerWindow(HWND hwnd);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_DISPLAY_USE_LAYERED_WINDOW_H_
