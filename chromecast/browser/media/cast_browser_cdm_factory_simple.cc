// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/cast_browser_cdm_factory.h"

#include "chromecast/media/cdm/browser_cdm_cast.h"

namespace chromecast {
namespace media {

scoped_ptr<BrowserCdmCast> CreatePlatformBrowserCdm(
    const CastKeySystem& key_system) {
  return scoped_ptr<BrowserCdmCast>();
}

}  // namespace media
}  // namespace chromecast
