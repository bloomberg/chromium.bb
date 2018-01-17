// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/cast_extension_web_contents_observer.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(extensions::CastExtensionWebContentsObserver);

namespace extensions {

CastExtensionWebContentsObserver::CastExtensionWebContentsObserver(
    content::WebContents* web_contents)
    : ExtensionWebContentsObserver(web_contents) {}

CastExtensionWebContentsObserver::~CastExtensionWebContentsObserver() {}

}  // namespace extensions
