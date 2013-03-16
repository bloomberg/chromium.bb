// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TRUETYPE_FONT_LIST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TRUETYPE_FONT_LIST_H_

#include <string>
#include <vector>

namespace content {

// Adds font family names on the host platform to the vector of strings.
//
// This function is potentially slow (the system may do a bunch of I/O) so be
// sure not to call this on a time-critical thread like the UI or I/O threads.
void GetFontFamilies_SlowBlocking(std::vector<std::string>* font_families);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TRUETYPE_FONT_LIST_H_
