// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_browser_client.h"

#include "base/memory/scoped_ptr.h"

namespace chromecast {
namespace shell {

// static
scoped_ptr<CastContentBrowserClient> CastContentBrowserClient::Create() {
  return make_scoped_ptr(new CastContentBrowserClient());
}

}  // namespace shell
}  // namespace chromecast
