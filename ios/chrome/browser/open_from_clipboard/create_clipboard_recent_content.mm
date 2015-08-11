// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/open_from_clipboard/create_clipboard_recent_content.h"

#import "components/open_from_clipboard/clipboard_recent_content_ios.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

scoped_ptr<ClipboardRecentContent> CreateClipboardRecentContentIOS() {
  return make_scoped_ptr(new ClipboardRecentContentIOS(
      ios::GetChromeBrowserProvider()->GetChromeUIScheme()));
}
