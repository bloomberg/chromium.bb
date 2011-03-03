// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#import <AppKit/AppKit.h>

#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/sys_string_conversions.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"

void IconLoader::ReadIcon() {
  NSString* group = base::SysUTF8ToNSString(group_);
  NSWorkspace* workspace = [NSWorkspace sharedWorkspace];
  NSImage* icon = [workspace iconForFileType:group];

  // Mac will ignore the size because icons have multiple size representations
  // and NSImage choses the best at draw-time.
  image_.reset(new gfx::Image([icon retain]));

  target_message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &IconLoader::NotifyDelegate));
}
