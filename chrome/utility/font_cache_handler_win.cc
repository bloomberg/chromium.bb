// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/font_cache_handler_win.h"

#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_utility_messages.h"
#include "content/public/common/dwrite_font_platform_win.h"
#include "content/public/utility/utility_thread.h"

FontCacheHandler::FontCacheHandler() {
}

FontCacheHandler::~FontCacheHandler() {
}

bool FontCacheHandler::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(FontCacheHandler, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_BuildDirectWriteFontCache,
                        OnBuildFontCache)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void FontCacheHandler::OnBuildFontCache(const base::FilePath& full_path) {
  DCHECK(!cache_thread_);

  utility_task_runner_ = base::MessageLoop::current()->task_runner();

  // Create worker thread for building font cache.
  cache_thread_.reset(new base::Thread("font_cache_thread"));
  if (!cache_thread_->Start()) {
    NOTREACHED();
    return;
  }
  cache_thread_->message_loop()->PostTask(
      FROM_HERE, base::Bind(&FontCacheHandler::StartBuildingFontCache,
                            base::Unretained(this),
                            full_path));
}

void FontCacheHandler::StartBuildingFontCache(const base::FilePath& full_path) {
  content::BuildFontCache(full_path);
  utility_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&FontCacheHandler::Cleanup,
                                            base::Unretained(this)));
}

void FontCacheHandler::Cleanup() {
  cache_thread_.reset();
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}
