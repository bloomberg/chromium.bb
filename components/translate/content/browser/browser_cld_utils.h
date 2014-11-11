// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_BROWSER_CLD_UTILS_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_BROWSER_CLD_UTILS_H_

#include "base/macros.h"

namespace translate {

// Browser-side utilities for dealing with CLD data source configuration.
// This class exists primarily to avoid duplicating code in high-level targets
// such as the Chrome browser, Chrome shell, and so on.
class BrowserCldUtils {
 public:
  // Perform conditional configuration of the CLD data provider.
  // If no specific BrowserCldDataProviderFactory has yet been set, an instance
  // is created and assigned as appropriate for each platform. If a specific
  // BrowserCldDataProviderFactory *has* been set, this method does nothing
  // (as a side effect of setting the default factory, subsequent invocations
  // do nothing - making this method reentrant).
  //
  // This method will not overwrite a data provider configured by an embedder.
  // This method must be invoked prior to registering components with the
  // component updater if the component updater implementation is to be used;
  // it is best to ensure that this method is called as early as possible.
  static void ConfigureDefaultDataProvider();

 private:
  BrowserCldUtils() {}
  ~BrowserCldUtils() {}
  DISALLOW_COPY_AND_ASSIGN(BrowserCldUtils);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_BROWSER_CLD_UTILS_H_
