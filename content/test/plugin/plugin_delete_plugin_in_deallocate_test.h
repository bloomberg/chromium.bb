// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PLUGIN_PLUGIN_DELETE_PLUGIN_IN_DEALLOCATE_TEST_H_
#define CONTENT_TEST_PLUGIN_PLUGIN_DELETE_PLUGIN_IN_DEALLOCATE_TEST_H_

#include "base/compiler_specific.h"
#include "content/test/plugin/plugin_test.h"

namespace NPAPIClient {

// This test verifies that if a plugin scripts the page from within an
// NPObject's deallocation, and the page tears down the plugin, then that
// object doesn't end up being double-freed.

class DeletePluginInDeallocateTest : public PluginTest {
 public:
  // Constructor.
  DeletePluginInDeallocateTest(NPP id, NPNetscapeFuncs* host_functions);

  // NPAPI handlers.
  virtual NPError SetWindow(NPWindow* pNPWindow) OVERRIDE;
  virtual NPError Destroy() OVERRIDE;

  NPObject* npobject_;
  bool test_started_;
};

}  // namespace NPAPIClient

#endif  // CONTENT_TEST_PLUGIN_PLUGIN_DELETE_PLUGIN_IN_DEALLOCATE_TEST_H_
