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
#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

namespace {
// How opaque should the layer that we flash onscreen to provide visual
// feedback after the screenshot is taken be?
const float kVisualFeedbackLayerOpacity = 0.25f;

// How long should the visual feedback layer be displayed?
const int64 kVisualFeedbackLayerDisplayTimeMs = 100;

// The minimum interval between two screenshot commands.  It has to be
// more than 1000 to prevent the conflict of filenames.
const int kScreenshotMinimumIntervalInMS = 1000;

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

bool AreScreenshotsDisabled() {
  return g_browser_process->local_state()->GetBoolean(
      prefs::kDisableScreenshots);
}

std::string GetScreenshotBaseFilename() {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);

  // We don't use base/i18n/time_formatting.h here because it doesn't
  // support our format.  Don't use ICU either to avoid i18n file names
  // for non-English locales.
  // TODO(mukai): integrate this logic somewhere time_formatting.h
  std::string file_name = base::StringPrintf(
      "Screenshot %d-%02d-%02d at ", now.year, now.month, now.day_of_month);

  if (ShouldUse24HourClock()) {
    file_name.append(base::StringPrintf(
        "%02d.%02d.%02d", now.hour, now.minute, now.second));
  } else {
    int hour = now.hour;
    if (hour > 12) {
      hour -= 12;
    } else if (hour == 0) {
      hour = 12;
    }
    file_name.append(base::StringPrintf(
        "%d.%02d.%02d ", hour, now.minute, now.second));
    file_name.append((now.hour >= 12) ? "PM" : "AM");
  }

  return file_name;
}

bool GetScreenshotDirectory(FilePath* directory) {
  if (AreScreenshotsDisabled())
    return false;

  bool is_logged_in = true;
#if defined(OS_CHROMEOS)
  is_logged_in = chromeos::UserManager::Get()->IsUserLoggedIn();
#endif

  if (is_logged_in) {
    DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(
        ash::Shell::GetInstance()->delegate()->GetCurrentBrowserContext());
    *directory = download_prefs->DownloadPath();
  } else {
    if (!file_util::GetTempDir(directory)) {
      LOG(ERROR) << "Failed to find temporary directory.";
      return false;
    }
  }
  return true;
}

void SaveScreenshot(const FilePath& screenshot_path,
                    scoped_refptr<base::RefCountedBytes> png_data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  DCHECK(!screenshot_path.empty());
  if (static_cast<size_t>(file_util::WriteFile(
          screenshot_path,
          reinterpret_cast<char*>(&(png_data->data()[0])),
          png_data->size())) != png_data->size()) {
    LOG(ERROR) << "Failed to save to " << screenshot_path.value();
  }
}

// TODO(kinaba): crbug.com/140425, remove this ungly #ifdef dispatch.
#ifdef OS_CHROMEOS
void SaveScreenshotToDrive(scoped_refptr<base::RefCountedBytes> png_data,
                           gdata::DriveFileError error,
                           const FilePath& local_path) {
  if (error != gdata::DRIVE_FILE_OK) {
    LOG(ERROR) << "Failed to write screenshot image to Google Drive: " << error;
    return;
  }
  SaveScreenshot(local_path, png_data);
}

void PostSaveScreenshotTask(const FilePath& screenshot_path,
                            scoped_refptr<base::RefCountedBytes> png_data) {
  if (gdata::util::IsUnderDriveMountPoint(screenshot_path)) {
    Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
    if (profile) {
      gdata::util::PrepareWritableFileAndRun(
          profile,
          screenshot_path,
          base::Bind(&SaveScreenshotToDrive, png_data));
    }
  } else {
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE, FROM_HERE,
        base::Bind(&SaveScreenshot, screenshot_path, png_data));
  }
}
#else
void PostSaveScreenshotTask(const FilePath& screenshot_path,
                            scoped_refptr<base::RefCountedBytes> png_data) {
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveScreenshot, screenshot_path, png_data));
}
#endif

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

}  // namespace

ScreenshotTaker::ScreenshotTaker() {
}

ScreenshotTaker::~ScreenshotTaker() {
}

void ScreenshotTaker::HandleTakeScreenshotForAllRootWindows() {
  FilePath screenshot_directory;
  if (!GetScreenshotDirectory(&screenshot_directory))
    return;

  std::string screenshot_basename = GetScreenshotBaseFilename();
  ash::Shell::RootWindowList root_windows = ash::Shell::GetAllRootWindows();
  for (size_t i = 0; i < root_windows.size(); ++i) {
    aura::RootWindow* root_window = root_windows[i];
    scoped_refptr<base::RefCountedBytes> png_data(new base::RefCountedBytes);
    std::string basename = screenshot_basename;
    gfx::Rect rect = root_window->bounds();
    if (root_windows.size() > 1)
      basename += base::StringPrintf(" - Display %d", static_cast<int>(i + 1));
    if (GrabWindowSnapshot(root_window, rect, &png_data->data())) {
      DisplayVisualFeedback(rect);
      PostSaveScreenshotTask(
          screenshot_directory.AppendASCII(basename + ".png"), png_data);
    } else {
      LOG(ERROR) << "Failed to grab the window screenshot for " << i;
    }
  }
  last_screenshot_timestamp_ = base::Time::Now();
}

void ScreenshotTaker::HandleTakePartialScreenshot(
    aura::Window* window, const gfx::Rect& rect) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  FilePath screenshot_directory;
  if (!GetScreenshotDirectory(&screenshot_directory))
    return;

  scoped_refptr<base::RefCountedBytes> png_data(new base::RefCountedBytes);

  if (GrabWindowSnapshot(window, rect, &png_data->data())) {
    last_screenshot_timestamp_ = base::Time::Now();
    DisplayVisualFeedback(rect);
    PostSaveScreenshotTask(
        screenshot_directory.AppendASCII(GetScreenshotBaseFilename() + ".png"),
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
