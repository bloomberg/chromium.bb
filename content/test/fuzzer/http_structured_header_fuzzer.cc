// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/http_structured_header.h"  // nogncheck

namespace content {
namespace http_structured_header {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::StringPiece input(reinterpret_cast<const char*>(data), size);
  ParseItem(input);
  ParseListOfLists(input);
  ParseParameterisedList(input);
  return 0;
}

}  // namespace http_structured_header
}  // namespace content
