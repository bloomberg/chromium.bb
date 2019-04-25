// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "base/pickle.h"
#include "components/bookmarks/browser/bookmark_node_data.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::Pickle pickle(reinterpret_cast<const char*>(data), size);
  bookmarks::BookmarkNodeData bookmark_node_data;
  bookmark_node_data.ReadFromPickle(&pickle);
  return 0;
}
