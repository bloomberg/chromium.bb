// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_snapshot/window_snapshot.h"

#include "base/logging.h"
#include "ui/gfx/rect.h"

namespace browser {

gfx::Rect GrabWindowSnapshot(gfx::NativeWindow window_handle,
                             std::vector<unsigned char>* png_representation) {
  // TODO(saintlou): Stub for Aura.
  NOTIMPLEMENTED();
  return gfx::Rect();
}

}  // namespace browser
