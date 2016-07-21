// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SEARCH_PROVIDER_H_
#define CHROME_COMMON_SEARCH_PROVIDER_H_

namespace search_provider {

// The type of OSDD that the renderer is giving to the browser.
enum OSDDType {
  // The Open Search Description URL was detected automatically.
  AUTODETECTED_PROVIDER,

  // The Open Search Description URL was given by Javascript.
  EXPLICIT_PROVIDER,
  OSDD_TYPE_LAST = EXPLICIT_PROVIDER
};

}  // namespace search_provider

#endif  // CHROME_COMMON_SEARCH_PROVIDER_H_
