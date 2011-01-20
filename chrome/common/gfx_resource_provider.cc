// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gfx_resource_provider.h"

#include "app/resource_bundle.h"
#include "base/string_piece.h"

namespace chrome {

base::StringPiece GfxResourceProvider(int key) {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(key);
}

}  // namespace chrome
