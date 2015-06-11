// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_content_renderer_client.h"

#include "base/memory/scoped_ptr.h"
#include "ipc/message_filter.h"

namespace chromecast {
namespace shell {

// static
scoped_ptr<CastContentRendererClient> CastContentRendererClient::Create() {
  return make_scoped_ptr(new CastContentRendererClient());
}

}  // namespace shell
}  // namespace chromecast
