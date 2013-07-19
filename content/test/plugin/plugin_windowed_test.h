// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PLUGIN_PLUGIN_WINDOWED_TEST_H
#define CONTENT_TEST_PLUGIN_PLUGIN_WINDOWED_TEST_H

#include "content/test/plugin/plugin_test.h"

namespace NPAPIClient {

// This class contains a list of windowed plugin tests. Please add additional
// tests to this class.
class WindowedPluginTest : public PluginTest {
 public:
  WindowedPluginTest(NPP id, NPNetscapeFuncs *host_functions);
  ~WindowedPluginTest();

 private:
  static LRESULT CALLBACK WindowProc(
      HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static void CallJSFunction(WindowedPluginTest*, const char*);

  virtual NPError SetWindow(NPWindow* pNPWindow);
  virtual NPError Destroy();

  HWND window_;
  bool done_;
};

}  // namespace NPAPIClient

#endif  // CONTENT_TEST_PLUGIN_PLUGIN_WINDOWED_TEST_H
