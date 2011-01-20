// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GFX_RESOURCE_PROVIDER_H_
#define CHROME_COMMON_GFX_RESOURCE_PROVIDER_H_
#pragma once

namespace base {
class StringPiece;
}

namespace chrome {

// This is called indirectly by the gfx theme code to access resources.
base::StringPiece GfxResourceProvider(int key);

}  // namespace chrome

#endif // CHROME_COMMON_GFX_RESOURCE_PROVIDER_H_
