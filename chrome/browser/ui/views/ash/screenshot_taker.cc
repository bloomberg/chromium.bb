// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/screenshot_taker.h"

#include <string>

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
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
#include "ui/aura/window.h"

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

// How opaque should the layer that we flash onscreen to provide visual
// feedback after the screenshot is taken be?
const float kVisualFeedbackLayerOpacity = 0.25f;

// How long should the visual feedback layer be displayed?
const int64 kVisualFeedbackLayerDisplayTimeMs = 100;

}  // namespace

ScreenshotTaker::ScreenshotTaker() {
}

ScreenshotTaker::~ScreenshotTaker() {
}

void ScreenshotTaker::HandleTakePartialScreenshot(
    aura::Window* window, const gfx::Rect& rect) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  scoped_refptr<RefCountedBytes> png_data(new RefCountedBytes);

  bool is_logged_in = true;
#if defined(OS_CHROMEOS)
  is_logged_in = chromeos::UserManager::Get()->IsUserLoggedIn();
#endif

  if (browser::GrabWindowSnapshot(window, &png_data->data(), rect)) {
    DisplayVisualFeedback(rect);
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE, FROM_HERE,
        base::Bind(&SaveScreenshot, is_logged_in, png_data));
  } else {
    LOG(ERROR) << "Failed to grab the window screenshot";
  }
}

void ScreenshotTaker::HandleTakeScreenshot(aura::Window* window) {
  HandleTakePartialScreenshot(window, window->bounds());
}

void ScreenshotTaker::CloseVisualFeedbackLayer() {
  visual_feedback_layer_.reset();
}

void ScreenshotTaker::DisplayVisualFeedback(const gfx::Rect& rect) {
  visual_feedback_layer_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
  visual_feedback_layer_->SetColor(SK_ColorWHITE);
  visual_feedback_layer_->SetOpacity(kVisualFeedbackLayerOpacity);
  visual_feedback_layer_->SetBounds(rect);

  ui::Layer* parent = ash::Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_OverlayContainer)->layer();
  parent->Add(visual_feedback_layer_.get());
  visual_feedback_layer_->SetVisible(true);

  MessageLoopForUI::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ScreenshotTaker::CloseVisualFeedbackLayer,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(kVisualFeedbackLayerDisplayTimeMs));
}
