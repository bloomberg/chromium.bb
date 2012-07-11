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
};

// The install state of the search provider (not installed, installed, default).
enum InstallState {
  // Equates to an access denied error.
  DENIED = -1,

  // DON'T CHANGE THE VALUES BELOW.
  // All of the following values are manidated by the
  // spec for window.external.IsSearchProviderInstalled.

  // The search provider is not installed.
  NOT_INSTALLED = 0,

  // The search provider is in the user's set but is not
  INSTALLED_BUT_NOT_DEFAULT = 1,

  // The search provider is set as the user's default.
  INSTALLED_AS_DEFAULT = 2
};

}  // namespace search_provider

#endif  // CHROME_COMMON_SEARCH_PROVIDER_H_
