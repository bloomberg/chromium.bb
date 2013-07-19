// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/plugin/plugin_execute_stream_javascript.h"

#include "base/basictypes.h"
#include "content/test/plugin/plugin_client.h"

namespace NPAPIClient {

static const int kMaxLength = 4096;

ExecuteStreamJavaScript::ExecuteStreamJavaScript(
    NPP id, NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions) {
}

NPError ExecuteStreamJavaScript::NewStream(NPMIMEType type, NPStream* stream,
                                           NPBool seekable, uint16* stype) {
  return NPERR_NO_ERROR;
}

int32 ExecuteStreamJavaScript::WriteReady(NPStream *stream) {
  return kMaxLength;
}

int32 ExecuteStreamJavaScript::Write(NPStream *stream, int32 offset, int32 len,
                                     void *buffer) {
  if (stream == NULL) {
    SetError("Write got null stream");
    return -1;
  }
  if (len < 0 || len > kMaxLength) {
    SetError("Write got bogus stream chunk size");
    return -1;
  }

  std::string javascript("javascript:");
  javascript.append(static_cast<char*>(buffer), len);
  size_t js_length = javascript.length();
  if (js_length != static_cast<uint32_t>(js_length)) {
    SetError("Javascript too long.");
    return -1;
  }

  NPString script_string = { javascript.c_str(),
                             static_cast<uint32_t>(js_length) };
  NPObject *window_obj = NULL;
  NPAPIClient::PluginClient::HostFunctions()->getvalue(
      id(), NPNVWindowNPObject, &window_obj);

  NPVariant unused_result;
  NPAPIClient::PluginClient::HostFunctions()->evaluate(
      id(), window_obj, &script_string, &unused_result);

  return len;
}

NPError ExecuteStreamJavaScript::DestroyStream(NPStream *stream,
                                               NPError reason) {
  return NPERR_NO_ERROR;
}

} // namespace NPAPIClient
