// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PLUGIN_PLUGIN_TEST_FACTROY_H__
#define CONTENT_TEST_PLUGIN_PLUGIN_TEST_FACTROY_H__

#include <string>

#include "third_party/npapi/bindings/nphostapi.h"

namespace NPAPIClient {

class PluginTest;

extern PluginTest* CreatePluginTest(const std::string& test_name,
                                    NPP instance,
                                    NPNetscapeFuncs* host_functions);

}  // namespace NPAPIClient

#endif  // CONTENT_TEST_PLUGIN_PLUGIN_TEST_FACTROY_H__
