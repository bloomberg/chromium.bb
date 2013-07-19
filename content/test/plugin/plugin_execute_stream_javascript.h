// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PLUGIN_PLUGIN_EXECUTE_STREAM_JAVASCRIPT_H_
#define CONTENT_TEST_PLUGIN_PLUGIN_EXECUTE_STREAM_JAVASCRIPT_H_

#include "base/compiler_specific.h"
#include "content/test/plugin/plugin_test.h"

namespace NPAPIClient {

// This class tests executes the JavaScript given in the stream URL.
class ExecuteStreamJavaScript : public PluginTest {
 public:
  // Constructor.
  ExecuteStreamJavaScript(NPP id, NPNetscapeFuncs *host_functions);

  //
  // NPAPI functions
  //
  virtual NPError NewStream(NPMIMEType type, NPStream* stream,
                            NPBool seekable, uint16* stype) OVERRIDE;
  virtual int32   WriteReady(NPStream *stream) OVERRIDE;
  virtual int32   Write(NPStream *stream, int32 offset, int32 len,
                        void *buffer) OVERRIDE;
  virtual NPError DestroyStream(NPStream *stream, NPError reason) OVERRIDE;
};

}  // namespace NPAPIClient

#endif  // CONTENT_TEST_PLUGIN_PLUGIN_EXECUTE_STREAM_JAVASCRIPT_H_
