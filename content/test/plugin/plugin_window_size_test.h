// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PLUGIN_PLUGIN_WINDOW_SIZE_TEST_H_
#define CONTENT_TEST_PLUGIN_PLUGIN_WINDOW_SIZE_TEST_H_

#include "base/compiler_specific.h"
#include "content/test/plugin/plugin_test.h"

namespace NPAPIClient {

// This class tests whether the plugin window has a non zero rect
// on the second SetWindow call.
class PluginWindowSizeTest : public PluginTest {
 public:
  // Constructor.
  PluginWindowSizeTest(NPP id, NPNetscapeFuncs *host_functions);
  // NPAPI SetWindow handler
  virtual NPError SetWindow(NPWindow* pNPWindow) OVERRIDE;
};

}  // namespace NPAPIClient

#endif  // CONTENT_TEST_PLUGIN_PLUGIN_WINDOW_SIZE_TEST_H_
