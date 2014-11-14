// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/font_cache_handler_win.h"

#include "chrome/common/chrome_utility_messages.h"
#include "content/public/common/dwrite_font_platform_win.h"
#include "content/public/utility/utility_thread.h"

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
  content::BuildFontCache(full_path);
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}
