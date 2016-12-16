// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

IconLoader::IconLoader(const base::FilePath& file_path,
                       IconSize size,
                       Delegate* delegate)
    : file_path_(file_path),
      icon_size_(size),
      delegate_(delegate) {}

IconLoader::~IconLoader() {
}

void IconLoader::Start() {
  target_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
      base::Bind(&IconLoader::ReadGroup, this),
      base::Bind(&IconLoader::OnReadGroup, this));
}

void IconLoader::ReadGroup() {
  group_ = GroupForFilepath(file_path_);
}

void IconLoader::OnReadGroup() {
  BrowserThread::PostTask(ReadIconThreadID(), FROM_HERE,
                          base::Bind(&IconLoader::ReadIcon, this));
}

void IconLoader::NotifyDelegate() {
  delegate_->OnImageLoaded(this, std::move(image_), group_);
}
