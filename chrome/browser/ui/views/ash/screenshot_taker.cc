// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/screenshot_taker.h"

#include <climits>
#include <string>

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/window_snapshot/window_snapshot.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/window.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

namespace {
bool ShouldUse24HourClock() {
#if defined(OS_CHROMEOS)
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  if (profile) {
    PrefService* pref_service = profile->GetPrefs();
    if (pref_service) {
      return pref_service->GetBoolean(prefs::kUse24HourClock);
    }
  }
#endif
  return base::GetHourClockType() == base::k24HourClock;
}

FilePath GetScreenshotPath(const FilePath& base_directory,
                           bool use_24hour_clock) {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);

  // We don't use base/i18n/time_formatting.h here because it doesn't
  // support our format.  Don't use ICU either to avoid i18n file names
  // for non-English locales.
  // TODO(mukai): integrate this logic somewhere time_formatting.h
  std::string file_name = base::StringPrintf(
      "Screenshot %d-%02d-%02d ", now.year, now.month, now.day_of_month);

  if (use_24hour_clock) {
    file_name.append(base::StringPrintf(
        "%02d:%02d:%02d", now.hour, now.minute, now.second));
  } else {
    int hour = now.hour;
    if (hour > 12) {
      hour -= 12;
    } else if (hour == 0) {
      hour = 12;
    }
    file_name.append(base::StringPrintf(
        "%d:%02d:%02d ", hour, now.minute, now.second));
    file_name.append((now.hour >= 12) ? "PM" : "AM");
  }

  for (int retry = 0; retry < INT_MAX; retry++) {
    std::string retry_suffix;
    if (retry > 0)
      retry_suffix = base::StringPrintf(" (%d)", retry + 1);

    FilePath file_path = base_directory.AppendASCII(
        file_name + retry_suffix + ".png");
    if (!file_util::PathExists(file_path))
      return file_path;
  }

  return FilePath();
}

// |is_logged_in| is used only for ChromeOS.  Otherwise it is always true.
void SaveScreenshot(bool is_logged_in,
                    bool use_24hour_clock,
                    scoped_refptr<base::RefCountedBytes> png_data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  FilePath screenshot_directory;
  if (is_logged_in) {
    screenshot_directory = download_util::GetDefaultDownloadDirectory();
  } else {
    if (!file_util::GetTempDir(&screenshot_directory)) {
      LOG(ERROR) << "Failed to find temporary directory.";
      return;
    }
  }

  FilePath screenshot_path = GetScreenshotPath(
      screenshot_directory, use_24hour_clock);

  if (screenshot_path.empty()) {
    LOG(ERROR) << "Failed to find a screenshot file name.";
    return;
  }

  if (static_cast<size_t>(file_util::WriteFile(
          screenshot_path,
          reinterpret_cast<char*>(&(png_data->data()[0])),
          png_data->size())) != png_data->size()) {
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

  scoped_refptr<base::RefCountedBytes> png_data(new base::RefCountedBytes);

  bool is_logged_in = true;
#if defined(OS_CHROMEOS)
  is_logged_in = chromeos::UserManager::Get()->IsUserLoggedIn();
#endif

  bool use_24hour_clock = ShouldUse24HourClock();

  if (browser::GrabWindowSnapshot(window, &png_data->data(), rect)) {
    DisplayVisualFeedback(rect);
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE, FROM_HERE,
        base::Bind(&SaveScreenshot, is_logged_in, use_24hour_clock, png_data));
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

  ui::Layer* parent = ash::Shell::GetContainer(
      ash::Shell::GetActiveRootWindow(),
      ash::internal::kShellWindowId_OverlayContainer)->layer();
  parent->Add(visual_feedback_layer_.get());
  visual_feedback_layer_->SetVisible(true);

  MessageLoopForUI::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ScreenshotTaker::CloseVisualFeedbackLayer,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(kVisualFeedbackLayerDisplayTimeMs));
}
