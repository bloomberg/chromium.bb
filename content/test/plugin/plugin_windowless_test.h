// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PLUGIN_PLUGIN_EXECUTE_SCRIPT_DELETE_TEST_H_
#define CONTENT_TEST_PLUGIN_PLUGIN_EXECUTE_SCRIPT_DELETE_TEST_H_

#include "base/compiler_specific.h"
#include "content/test/plugin/plugin_test.h"

namespace NPAPIClient {

// This class contains a list of windowless plugin tests. Please add additional
// tests to this class.
class WindowlessPluginTest : public PluginTest {
 public:
  // Constructor.
  WindowlessPluginTest(NPP id, NPNetscapeFuncs *host_functions);

  // These tests run in windowless plugin mode.
  virtual bool IsWindowless() const OVERRIDE;

  // NPAPI HandleEvent handler
  virtual NPError New(uint16 mode, int16 argc, const char* argn[],
                      const char* argv[], NPSavedData* saved) OVERRIDE;
  virtual int16 HandleEvent(void* event) OVERRIDE;

 protected:
  NPError ExecuteScript(NPNetscapeFuncs* browser, NPP id,
      const std::string& script, NPVariant* result);
  void ExecuteScriptDeleteInPaint(NPNetscapeFuncs* browser);
  void ExecuteScriptResizeInPaint(NPNetscapeFuncs* browser);
  void MultipleInstanceSyncCalls(NPNetscapeFuncs* browser);
  void ConvertPoint(NPNetscapeFuncs* browser);

  int paint_counter_;
};

}  // namespace NPAPIClient

#endif  // CONTENT_TEST_PLUGIN_PLUGIN_EXECUTE_SCRIPT_DELETE_TEST_H_
