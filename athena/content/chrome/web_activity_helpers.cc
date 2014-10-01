// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/web_activity_helpers.h"

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/tab_helper.h"

namespace athena {

void AttachWebActivityHelpers(content::WebContents* contents) {
  // TODO(pkotwicz): Call TabHelpers::AttachTabHelpers()
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      contents);
  extensions::TabHelper::CreateForWebContents(contents);
}

}  // namespace athena
