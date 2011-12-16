// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/screenshot_taker.h"

#include <string>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/ui/window_snapshot/window_snapshot.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/root_window.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

namespace {
std::string GetScreenshotFileName() {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);

  return base::StringPrintf("screenshot-%d%02d%02d-%02d%02d%02d.png",
                            now.year, now.month, now.day_of_month,
                            now.hour, now.minute, now.second);
}

// |is_logged_in| is used only for ChromeOS.  Otherwise it is always true.
void SaveScreenshot(bool is_logged_in,
                    scoped_refptr<RefCountedBytes> png_data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  std::string screenshot_filename = GetScreenshotFileName();
  FilePath screenshot_path;
  if (is_logged_in) {
    screenshot_path = download_util::GetDefaultDownloadDirectory().AppendASCII(
        screenshot_filename);
  } else {
    file_util::CreateTemporaryFile(&screenshot_path);
  }

  if (static_cast<size_t>(file_util::WriteFile(
          screenshot_path,
          reinterpret_cast<char*>(&(png_data->data()[0])),
          png_data->size())) == png_data->size()) {
    if (!is_logged_in) {
      // We created a temporary file without .png suffix.  Rename it
      // here.
      FilePath real_path = screenshot_path.DirName().AppendASCII(
          screenshot_filename);
      if (!file_util::ReplaceFile(screenshot_path, real_path)) {
        LOG(ERROR) << "Failed to rename the file to " << real_path.value();
      }
    }
  } else {
    LOG(ERROR) << "Failed to save to " << screenshot_path.value();
  }
}

}  // namespace

ScreenshotTaker::ScreenshotTaker() {
}

void ScreenshotTaker::HandleTakeScreenshot() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  aura::RootWindow* root_window = aura::RootWindow::GetInstance();
  scoped_refptr<RefCountedBytes> png_data(new RefCountedBytes);

  bool is_logged_in = true;
#if defined(OS_CHROMEOS)
  is_logged_in = chromeos::UserManager::Get()->user_is_logged_in();
#endif

  if (browser::GrabWindowSnapshot(
          root_window, &png_data->data(), root_window->bounds())) {
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE, FROM_HERE,
        base::Bind(&SaveScreenshot, is_logged_in, png_data));
  } else {
    LOG(ERROR) << "Failed to grab the window screenshot";
  }
}
