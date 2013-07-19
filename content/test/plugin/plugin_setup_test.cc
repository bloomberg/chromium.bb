// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/strings/string_util.h"

#include "content/test/plugin/plugin_setup_test.h"

namespace NPAPIClient {

PluginSetupTest::PluginSetupTest(NPP id, NPNetscapeFuncs *host_functions)
  : PluginTest(id, host_functions) {
}

NPError PluginSetupTest::SetWindow(NPWindow* pNPWindow) {
  this->SignalTestCompleted();

  return NPERR_NO_ERROR;
}

}  // namespace NPAPIClient
