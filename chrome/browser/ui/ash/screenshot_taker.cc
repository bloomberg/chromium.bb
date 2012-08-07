// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/screenshot_taker.h"

#include <climits>
#include <string>

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/window_snapshot/window_snapshot.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

namespace {
const int kScreenshotMinimumIntervalInMS = 500;

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

std::string GetScreenShotBaseFilename(bool use_24hour_clock) {
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

  return file_name;
}

FilePath GetScreenshotPath(const FilePath& base_directory,
                           const std::string& base_name) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  for (int retry = 0; retry < INT_MAX; retry++) {
    std::string retry_suffix;
    if (retry > 0)
      retry_suffix = base::StringPrintf(" (%d)", retry + 1);

    FilePath file_path = base_directory.AppendASCII(
        base_name + retry_suffix + ".png");
    if (!file_util::PathExists(file_path))
      return file_path;
  }
  return FilePath();
}

void SaveScreenshotToLocalFile(scoped_refptr<base::RefCountedBytes> png_data,
                               const FilePath& screenshot_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  if (static_cast<size_t>(file_util::WriteFile(
          screenshot_path,
          reinterpret_cast<char*>(&(png_data->data()[0])),
          png_data->size())) != png_data->size()) {
    LOG(ERROR) << "Failed to save to " << screenshot_path.value();
  }
}

void SaveScreenshot(const FilePath& screenshot_directory,
                    const std::string& base_name,
                    scoped_refptr<base::RefCountedBytes> png_data) {
  FilePath screenshot_path = GetScreenshotPath(screenshot_directory, base_name);
  if (screenshot_path.empty()) {
    LOG(ERROR) << "Failed to find a screenshot file name.";
    return;
  }
  SaveScreenshotToLocalFile(png_data, screenshot_path);
}

// TODO(kinaba): crbug.com/140425, remove this ungly #ifdef dispatch.
#ifdef OS_CHROMEOS
void SaveScreenshotToGData(scoped_refptr<base::RefCountedBytes> png_data,
                           gdata::GDataFileError error,
                           const FilePath& local_path) {
  if (error != gdata::GDATA_FILE_OK) {
    LOG(ERROR) << "Failed to write screenshot image to Google Drive: " << error;
    return;
  }
  SaveScreenshotToLocalFile(png_data, local_path);
}

void PostSaveScreenshotTask(const FilePath& screenshot_directory,
                            const std::string& base_name,
                            scoped_refptr<base::RefCountedBytes> png_data) {
  if (gdata::util::IsUnderGDataMountPoint(screenshot_directory)) {
    Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
    if (profile) {
      // TODO(kinaba,mukai): crbug.com/140749. Take care of the case
      // "base_name.png" already exists.
      gdata::util::PrepareWritableFileAndRun(
          profile,
          screenshot_directory.Append(base_name + ".png"),
          base::Bind(&SaveScreenshotToGData, png_data));
    }
  } else {
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE, FROM_HERE,
        base::Bind(&SaveScreenshot, screenshot_directory, base_name, png_data));
  }
}
#else
void PostSaveScreenshotTask(const FilePath& screenshot_directory,
                            const std::string& base_name,
                            scoped_refptr<base::RefCountedBytes> png_data) {
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveScreenshot, screenshot_directory, base_name, png_data));
}
#endif

bool AreScreenshotsDisabled() {
  return g_browser_process->local_state()->GetBoolean(
      prefs::kDisableScreenshots);
}

bool GrabWindowSnapshot(aura::Window* window,
                        const gfx::Rect& snapshot_bounds,
                        std::vector<unsigned char>* png_data) {
#if defined(OS_LINUX)
  // chrome::GrabWindowSnapshotForUser checks this too, but
  // RootWindow::GrabSnapshot does not.
  if (AreScreenshotsDisabled())
    return false;

  // We use XGetImage() for Linux/ChromeOS for performance reasons.
  // See crbug.com/119492
  // TODO(mukai): remove this when the performance issue has been fixed.
  if (window->GetRootWindow()->GrabSnapshot(snapshot_bounds, png_data))
    return true;
#endif  // OS_LINUX

  return chrome::GrabWindowSnapshotForUser(window, png_data, snapshot_bounds);
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

void ScreenshotTaker::HandleTakeScreenshot(aura::Window* window) {
  HandleTakePartialScreenshot(window, window->bounds());
}

void ScreenshotTaker::HandleTakePartialScreenshot(
    aura::Window* window, const gfx::Rect& rect) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (AreScreenshotsDisabled())
    return;

  scoped_refptr<base::RefCountedBytes> png_data(new base::RefCountedBytes);

  bool is_logged_in = true;
#if defined(OS_CHROMEOS)
  is_logged_in = chromeos::UserManager::Get()->IsUserLoggedIn();
#endif

  FilePath screenshot_directory;
  if (is_logged_in) {
    DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(
        ash::Shell::GetInstance()->delegate()->GetCurrentBrowserContext());
    screenshot_directory = download_prefs->DownloadPath();
  } else {
    if (!file_util::GetTempDir(&screenshot_directory)) {
      LOG(ERROR) << "Failed to find temporary directory.";
      return;
    }
  }

  if (GrabWindowSnapshot(window, rect, &png_data->data())) {
    last_screenshot_timestamp_ = base::Time::Now();
    DisplayVisualFeedback(rect);
    PostSaveScreenshotTask(screenshot_directory,
                           GetScreenShotBaseFilename(ShouldUse24HourClock()),
                           png_data);
  } else {
    LOG(ERROR) << "Failed to grab the window screenshot";
  }
}

bool ScreenshotTaker::CanTakeScreenshot() {
  return last_screenshot_timestamp_.is_null() ||
      base::Time::Now() - last_screenshot_timestamp_ >
      base::TimeDelta::FromMilliseconds(
          kScreenshotMinimumIntervalInMS);
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
