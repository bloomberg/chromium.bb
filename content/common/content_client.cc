// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_client.h"

namespace content {

static ContentClient* g_client;

void SetContentClient(ContentClient* client) {
  g_client = client;
}

ContentClient* GetContentClient() {
  return g_client;
}

}  // namespace content
