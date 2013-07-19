// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_TEST_PLUGIN_PLUGIN_NPOBJECT_IDENTITY_TEST_H_
#define CONTENT_TEST_PLUGIN_PLUGIN_NPOBJECT_IDENTITY_TEST_H_

#include "base/compiler_specific.h"
#include "content/test/plugin/plugin_test.h"

namespace NPAPIClient {

// The NPObjectProxyTest tests that when we proxy an NPObject that is itself
// a proxy, we don't create a new proxy but instead just use the original
// pointer.

class NPObjectIdentityTest : public PluginTest {
 public:
  // Constructor.
  NPObjectIdentityTest(NPP id, NPNetscapeFuncs *host_functions);

  // NPAPI SetWindow handler.
  virtual NPError SetWindow(NPWindow* pNPWindow) OVERRIDE;
};

}  // namespace NPAPIClient

#endif  // CONTENT_TEST_PLUGIN_PLUGIN_NPOBJECT_IDENTITY_TEST_H_
