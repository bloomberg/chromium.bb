// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include "base/message_loop.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(TOOLKIT_GTK)
#include "base/mime_util.h"
#endif

IconLoader::IconLoader(const IconGroupID& group, IconSize size,
                       Delegate* delegate)
    : target_message_loop_(NULL),
      group_(group),
      icon_size_(size),
      bitmap_(NULL),
      delegate_(delegate) {
}

IconLoader::~IconLoader() {
  delete bitmap_;
}

void IconLoader::Start() {
  target_message_loop_ = MessageLoop::current();

#if defined(TOOLKIT_GTK)
  // This call must happen on the UI thread before we can start loading icons.
  mime_util::DetectGtkTheme();
#endif

  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &IconLoader::ReadIcon));
}

void IconLoader::NotifyDelegate() {
  if (delegate_->OnBitmapLoaded(this, bitmap_))
    bitmap_ = NULL;
}
