// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PLUGIN_PLUGIN_CREATE_INSTANCE_IN_PAINT_H_
#define CONTENT_TEST_PLUGIN_PLUGIN_CREATE_INSTANCE_IN_PAINT_H_

#include "content/test/plugin/plugin_test.h"

namespace NPAPIClient {

// This class tests that creating a new plugin via script while handling a
// Windows message doesn't cause a deadlock.
class CreateInstanceInPaintTest : public PluginTest {
 public:
  // Constructor.
  CreateInstanceInPaintTest(NPP id, NPNetscapeFuncs *host_functions);
  //
  // NPAPI functions
  //
  virtual NPError SetWindow(NPWindow* pNPWindow);

 private:
  static LRESULT CALLBACK WindowProc(
      HWND window, UINT message, WPARAM wparam, LPARAM lparam);

  HWND window_;
  bool created_;
};

}  // namespace NPAPIClient

#endif  // CONTENT_TEST_PLUGIN_PLUGIN_CREATE_INSTANCE_IN_PAINT_H_
