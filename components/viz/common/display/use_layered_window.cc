// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/display/use_layered_window.h"

#include "base/win/windows_version.h"
#include "ui/base/win/internal_constants.h"

namespace viz {

bool NeedsToUseLayerWindow(HWND hwnd) {
  return base::win::GetVersion() <= base::win::VERSION_WIN7 &&
         GetProp(hwnd, ui::kWindowTranslucent);
}

}  // namespace viz
