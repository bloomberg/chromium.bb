// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include "content/browser/browser_thread.h"
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
}

void IconLoader::Start() {
  target_message_loop_ = base::MessageLoopProxy::CreateForCurrentThread();

#if defined(TOOLKIT_GTK)
  // This call must happen on the UI thread before we can start loading icons.
  mime_util::DetectGtkTheme();
#endif

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &IconLoader::ReadIcon));
}

void IconLoader::NotifyDelegate() {
  if (delegate_->OnBitmapLoaded(this, bitmap_.Get()))
    bitmap_.Release();
}
