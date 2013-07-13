// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_REENCODE_FAVICON_H_
#define CHROME_BROWSER_IMPORTER_REENCODE_FAVICON_H_

#include <vector>

#include "base/basictypes.h"

// Given raw image data, decodes the icon, re-sampling to the correct size as
// necessary, and re-encodes as PNG data in the given output vector. Returns
// true on success.
bool ReencodeFavicon(const uint8* src_data, size_t src_len,
                     std::vector<uint8>* png_data);

#endif  // CHROME_BROWSER_IMPORTER_REENCODE_FAVICON_H_
