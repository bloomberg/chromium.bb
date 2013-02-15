// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

IconLoader::IconLoader(const IconGroupID& group, IconSize size,
                       Delegate* delegate)
    : target_message_loop_(NULL),
      group_(group),
      icon_size_(size),
      image_(NULL),
      delegate_(delegate) {
}

IconLoader::~IconLoader() {
}

void IconLoader::Start() {
  target_message_loop_ = base::MessageLoopProxy::current();

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&IconLoader::ReadIcon, this));
}

void IconLoader::NotifyDelegate() {
  // If the delegate takes ownership of the Image, release it from the scoped
  // pointer.
  if (delegate_->OnImageLoaded(this, image_.get()))
    ignore_result(image_.release());  // Can't ignore return value.
}
