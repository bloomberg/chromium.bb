// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/plugin/plugin_request_read_test.h"

#include "base/basictypes.h"

namespace NPAPIClient {

PluginRequestReadTest::PluginRequestReadTest(NPP id,
                                             NPNetscapeFuncs* host_functions)
    : PluginTest(id, host_functions),
      tests_started_(false),
      read_requested_(false) {
}

PluginRequestReadTest::~PluginRequestReadTest() {
}

NPError PluginRequestReadTest::New(uint16 mode, int16 argc, const char* argn[],
                             const char* argv[], NPSavedData* saved) {
  url_to_request_ = GetArgValue("url_to_request", argc, argn, argv);
  return PluginTest::New(mode, argc, argn, argv, saved);
}

NPError PluginRequestReadTest::SetWindow(NPWindow* window) {
  if (!tests_started_) {
    tests_started_ = true;
    NPError result = HostFunctions()->geturl(id(),
                                             url_to_request_.c_str(),
                                             NULL);
    if (result != NPERR_NO_ERROR)
      SetError("Failed request anURL.");
  }
  return PluginTest::SetWindow(window);
}

NPError PluginRequestReadTest::NewStream(NPMIMEType type, NPStream* stream,
                                         NPBool seekable, uint16* stream_type) {
  *stream_type = NP_SEEK;
  if (!read_requested_) {
    requested_ranges_.resize(1);
    requested_ranges_[0].offset = 4;
    requested_ranges_[0].length = 6;
    requested_ranges_[0].next = NULL;
    NPError result = HostFunctions()->requestread(stream,
                                                  &requested_ranges_[0]);
    if (result != NPERR_NO_ERROR)
      SetError("Failed request read from stream.");
    read_requested_ = true;
  }
  return PluginTest::NewStream(type, stream, seekable, stream_type);
}

NPError PluginRequestReadTest::DestroyStream(NPStream *stream, NPError reason) {
  if (!requested_ranges_.empty())
    SetError("Some requested ranges are not received!");
  SignalTestCompleted();
  return PluginTest::DestroyStream(stream, reason);
}

int32 PluginRequestReadTest::WriteReady(NPStream* stream) {
  int32 result = 0;
  for (size_t i = 0; i < requested_ranges_.size(); ++i)
    result += requested_ranges_[i].length;
  return result;
}

int32 PluginRequestReadTest::Write(NPStream* stream, int32 offset, int32 len,
                                   void* buffer) {
  std::vector<NPByteRange>::iterator it;
  // Remove received range (or sub-range) from requested_ranges_, and
  // verify that we have received proper data.

  for (it = requested_ranges_.begin(); it != requested_ranges_.end(); ++it) {
    if (it->offset == offset)
      break;
  }
  if (it == requested_ranges_.end()) {
    // It is Ok for browser to write some data from start of the stream before
    // we've issued any read requests.
    return len;
  }
  // Shrink range to mark area we have just received.
  it->offset += len;
  if (static_cast<int32>(it->length) < len)
    it->length = 0;
  else
    it->length -= len;
  if (it->length == 0)
    requested_ranges_.erase(it);

  // Verify that data, which we got, is right.
  const char* data = static_cast<const char*>(buffer);
  for (int32 i = 0; i < len; ++i) {
    int cur_offset = offset + i;
    char expected = '0' + cur_offset;
    if (data[i] != expected) {
      SetError("Content mismatch between data and source!");
      break;
    }
  }
  if (requested_ranges_.empty())
    SignalTestCompleted();

  return len;
}

}  // namespace NPAPIClient

