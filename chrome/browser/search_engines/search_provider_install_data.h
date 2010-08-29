// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_DATA_H_
#define CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_DATA_H_

#include "base/ref_counted.h"

class GURL;

// Provides the search provider install state for the I/O thread.
class SearchProviderInstallData
    : public base::RefCountedThreadSafe<SearchProviderInstallData> {
 public:
  enum State {
    // The install state cannot be determined at the moment.
    NOT_READY = -2,

    // Equates to an access denied error.
    DENIED = -1,

    // The search provider is not installed.
    NOT_INSTALLED = 0,

    // The search provider is in the user's set but is not
    INSTALLED_BUT_NOT_DEFAULT = 1,

    // The search provider is set as the user's default.
    INSTALLED_AS_DEFAULT = 2
  };

  // Returns the search provider install state for the given origin.
  virtual State GetInstallState(const GURL& requested_origin) = 0;

 protected:
  friend class base::RefCountedThreadSafe<SearchProviderInstallData>;
  virtual ~SearchProviderInstallData() {}
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_DATA_H_
