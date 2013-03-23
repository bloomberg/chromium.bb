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
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/screenshot_source.h"
#include "chrome/browser/ui/window_snapshot/window_snapshot.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
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


void SaveScreenshotInternal(const base::FilePath& screenshot_path,
                            scoped_refptr<base::RefCountedBytes> png_data) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  DCHECK(!screenshot_path.empty());
  if (static_cast<size_t>(file_util::WriteFile(
          screenshot_path,
          reinterpret_cast<char*>(&(png_data->data()[0])),
          png_data->size())) != png_data->size()) {
    LOG(ERROR) << "Failed to save to " << screenshot_path.value();
  }
}

void SaveScreenshot(const base::FilePath& screenshot_path,
                    scoped_refptr<base::RefCountedBytes> png_data) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  DCHECK(!screenshot_path.empty());

  if (!file_util::CreateDirectory(screenshot_path.DirName())) {
    LOG(ERROR) << "Failed to ensure the existence of "
               << screenshot_path.DirName().value();
    return;
  }
  SaveScreenshotInternal(screenshot_path, png_data);
}

// TODO(kinaba): crbug.com/140425, remove this ungly #ifdef dispatch.
#if defined(OS_CHROMEOS)
void SaveScreenshotToDrive(scoped_refptr<base::RefCountedBytes> png_data,
                           drive::DriveFileError error,
                           const base::FilePath& local_path) {
  if (error != drive::DRIVE_FILE_OK) {
    LOG(ERROR) << "Failed to write screenshot image to Google Drive: " << error;
    return;
  }
  SaveScreenshotInternal(local_path, png_data);
}

void EnsureDirectoryExistsCallback(
    Profile* profile,
    const base::FilePath& screenshot_path,
    scoped_refptr<base::RefCountedBytes> png_data,
    drive::DriveFileError error) {
  // It is okay to fail with DRIVE_FILE_ERROR_EXISTS since anyway the directory
  // of the target file exists.
  if (error == drive::DRIVE_FILE_OK ||
      error == drive::DRIVE_FILE_ERROR_EXISTS) {
    drive::util::PrepareWritableFileAndRun(
        profile,
        screenshot_path,
        base::Bind(&SaveScreenshotToDrive, png_data));
  } else {
    LOG(ERROR) << "Failed to ensure the existence of the specified directory "
               << "in Google Drive: " << error;
  }
}

void PostSaveScreenshotTask(const base::FilePath& screenshot_path,
                            scoped_refptr<base::RefCountedBytes> png_data) {
  if (drive::util::IsUnderDriveMountPoint(screenshot_path)) {
    Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
    if (profile) {
      drive::util::EnsureDirectoryExists(
          profile,
          screenshot_path.DirName(),
          base::Bind(&EnsureDirectoryExistsCallback,
                     profile,
                     screenshot_path,
                     png_data));
    }
  } else {
    content::BrowserThread::GetBlockingPool()->PostTask(
        FROM_HERE, base::Bind(&SaveScreenshot, screenshot_path, png_data));
  }
}
#else
void PostSaveScreenshotTask(const base::FilePath& screenshot_path,
                            scoped_refptr<base::RefCountedBytes> png_data) {
  content::BrowserThread::GetBlockingPool()->PostTask(
      FROM_HERE, base::Bind(&SaveScreenshot, screenshot_path, png_data));
}
#endif

bool GrabWindowSnapshot(aura::Window* window,
                        const gfx::Rect& snapshot_bounds,
                        std::vector<unsigned char>* png_data) {
#if defined(OS_LINUX)
  // chrome::GrabWindowSnapshotForUser checks this too, but
  // RootWindow::GrabSnapshot does not.
  if (ScreenshotSource::AreScreenshotsDisabled())
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
  base::FilePath screenshot_directory;
  if (!ScreenshotSource::GetScreenshotDirectory(&screenshot_directory))
    return;

  std::string screenshot_basename =
      ScreenshotSource::GetScreenshotBaseFilename();
  ash::Shell::RootWindowList root_windows = ash::Shell::GetAllRootWindows();
  // Reorder root_windows to take the primary root window's snapshot at first.
  aura::RootWindow* primary_root = ash::Shell::GetPrimaryRootWindow();
  if (*(root_windows.begin()) != primary_root) {
    root_windows.erase(std::find(
        root_windows.begin(), root_windows.end(), primary_root));
    root_windows.insert(root_windows.begin(), primary_root);
  }
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

  base::FilePath screenshot_directory;
  if (!ScreenshotSource::GetScreenshotDirectory(&screenshot_directory))
    return;

  scoped_refptr<base::RefCountedBytes> png_data(new base::RefCountedBytes);

  if (GrabWindowSnapshot(window, rect, &png_data->data())) {
    last_screenshot_timestamp_ = base::Time::Now();
    DisplayVisualFeedback(rect);
    PostSaveScreenshotTask(
        screenshot_directory.AppendASCII(
                   ScreenshotSource::GetScreenshotBaseFilename() + ".png"),
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
