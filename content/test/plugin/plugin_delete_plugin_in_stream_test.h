// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PLUGIN_PLUGIN_DELETE_PLUGIN_IN_STREAM_TEST_H_
#define CONTENT_TEST_PLUGIN_PLUGIN_DELETE_PLUGIN_IN_STREAM_TEST_H_

#include "base/compiler_specific.h"
#include "content/test/plugin/plugin_test.h"

namespace NPAPIClient {

// This class tests
class DeletePluginInStreamTest : public PluginTest {
 public:
  // Constructor.
  DeletePluginInStreamTest(NPP id, NPNetscapeFuncs *host_functions);
  //
  // NPAPI functions
  //
  virtual NPError SetWindow(NPWindow* pNPWindow) OVERRIDE;
  virtual NPError NewStream(NPMIMEType type, NPStream* stream,
                            NPBool seekable, uint16* stype) OVERRIDE;
 private:
  bool test_started_;
  std::string self_url_;
};

}  // namespace NPAPIClient

#endif  // CONTENT_TEST_PLUGIN_PLUGIN_DELETE_PLUGIN_IN_STREAM_TEST_H_
