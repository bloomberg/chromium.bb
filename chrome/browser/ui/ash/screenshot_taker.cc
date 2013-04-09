// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/screenshot_taker.h"

#include <climits>
#include <string>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/screenshot_source.h"
#include "chrome/browser/ui/window_snapshot/window_snapshot.h"
#include "content/public/browser/browser_thread.h"
#include "grit/ash_strings.h"
#include "grit/theme_resources.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_manager_util.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

namespace {
// The minimum interval between two screenshot commands.  It has to be
// more than 1000 to prevent the conflict of filenames.
const int kScreenshotMinimumIntervalInMS = 1000;

const char kNotificationOriginUrl[] = "chrome://screenshot";

// Delegate for a notification. This class has two roles: to implement callback
// methods for notification, and to provide an identity of the associated
// notification.
class ScreenshotTakerNotificationDelegate : public NotificationDelegate {
 public:
  ScreenshotTakerNotificationDelegate(const std::string& id_text,
                                      bool success,
                                      const base::FilePath& screenshot_path)
      : id_text_(id_text),
        success_(success),
        screenshot_path_(screenshot_path) {
  }

  // Overridden from NotificationDelegate:
  virtual void Display() OVERRIDE {}
  virtual void Error() OVERRIDE {}
  virtual void Close(bool by_user) OVERRIDE {}
  virtual void Click() OVERRIDE {
    if (!success_)
      return;
#if defined(OS_CHROMEOS)
    file_manager_util::ShowFileInFolder(screenshot_path_);
#else
    // TODO(sschmitz): perhaps add similar action for Windows.
#endif
  }
  virtual std::string id() const OVERRIDE { return id_text_; }
  virtual content::RenderViewHost* GetRenderViewHost() const OVERRIDE {
    return NULL;
  }

 private:
  virtual ~ScreenshotTakerNotificationDelegate() {}

  const std::string id_text_;
  const bool success_;
  const base::FilePath screenshot_path_;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotTakerNotificationDelegate);
};

typedef base::Callback<
  void(ScreenshotTakerObserver::Result screenshot_result,
       const base::FilePath& screenshot_path)> ShowNotificationCallback;

void SaveScreenshotInternal(const ShowNotificationCallback& callback,
                            const base::FilePath& screenshot_path,
                            scoped_refptr<base::RefCountedBytes> png_data) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  DCHECK(!screenshot_path.empty());
  ScreenshotTakerObserver::Result result =
      ScreenshotTakerObserver::SCREENSHOT_SUCCESS;
  if (static_cast<size_t>(file_util::WriteFile(
          screenshot_path,
          reinterpret_cast<char*>(&(png_data->data()[0])),
          png_data->size())) != png_data->size()) {
    LOG(ERROR) << "Failed to save to " << screenshot_path.value();
    result = ScreenshotTakerObserver::SCREENSHOT_WRITE_FILE_FAILED;
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(callback, result, screenshot_path));
}

void SaveScreenshot(const ShowNotificationCallback& callback,
                    const base::FilePath& screenshot_path,
                    scoped_refptr<base::RefCountedBytes> png_data) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  DCHECK(!screenshot_path.empty());

  if (!file_util::CreateDirectory(screenshot_path.DirName())) {
    LOG(ERROR) << "Failed to ensure the existence of "
               << screenshot_path.DirName().value();
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(callback,
                   ScreenshotTakerObserver::SCREENSHOT_CREATE_DIR_FAILED,
                   screenshot_path));
    return;
  }
  SaveScreenshotInternal(callback, screenshot_path, png_data);
}

// TODO(kinaba): crbug.com/140425, remove this ungly #ifdef dispatch.
#if defined(OS_CHROMEOS)
void SaveScreenshotToDrive(const ShowNotificationCallback& callback,
                           scoped_refptr<base::RefCountedBytes> png_data,
                           drive::DriveFileError error,
                           const base::FilePath& local_path) {
  if (error != drive::DRIVE_FILE_OK) {
    LOG(ERROR) << "Failed to write screenshot image to Google Drive: " << error;
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(callback,
                   ScreenshotTakerObserver::SCREENSHOT_CREATE_FILE_FAILED,
                   local_path));
    return;
  }
  SaveScreenshotInternal(callback, local_path, png_data);
}

void EnsureDirectoryExistsCallback(
    const ShowNotificationCallback& callback,
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
        base::Bind(&SaveScreenshotToDrive, callback, png_data));
  } else {
    LOG(ERROR) << "Failed to ensure the existence of the specified directory "
               << "in Google Drive: " << error;
    callback.Run(ScreenshotTakerObserver::SCREENSHOT_CHECK_DIR_FAILED,
                 screenshot_path);
  }
}

void PostSaveScreenshotTask(const ShowNotificationCallback& callback,
                            Profile* profile,
                            const base::FilePath& screenshot_path,
                            scoped_refptr<base::RefCountedBytes> png_data) {
  if (drive::util::IsUnderDriveMountPoint(screenshot_path)) {
    drive::util::EnsureDirectoryExists(
        profile,
        screenshot_path.DirName(),
        base::Bind(&EnsureDirectoryExistsCallback,
                   callback,
                   profile,
                   screenshot_path,
                   png_data));
  } else {
    content::BrowserThread::GetBlockingPool()->PostTask(
        FROM_HERE, base::Bind(&SaveScreenshot,
                              callback,
                              screenshot_path,
                              png_data));
  }
}
#else
void PostSaveScreenshotTask(const ShowNotificationCallback& callback,
                            Profile* profile,
                            const base::FilePath& screenshot_path,
                            scoped_refptr<base::RefCountedBytes> png_data) {
  content::BrowserThread::GetBlockingPool()->PostTask(
      FROM_HERE, base::Bind(&SaveScreenshot,
                            callback,
                            screenshot_path,
                            png_data));
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

ScreenshotTaker::ScreenshotTaker(Profile* profile)
    : profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {
  DCHECK(profile_);
}

ScreenshotTaker::~ScreenshotTaker() {
}

void ScreenshotTaker::HandleTakeScreenshotForAllRootWindows() {
  base::FilePath screenshot_directory;
  if (!screenshot_directory_for_test_.empty()) {
    screenshot_directory = screenshot_directory_for_test_;
  } else if (!ScreenshotSource::GetScreenshotDirectory(&screenshot_directory)) {
    ShowNotification(ScreenshotTakerObserver::SCREENSHOT_GET_DIR_FAILED,
                     base::FilePath());
    return;
  }
  std::string screenshot_basename = !screenshot_basename_for_test_.empty() ?
      screenshot_basename_for_test_ :
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
    base::FilePath screenshot_path =
        screenshot_directory.AppendASCII(basename + ".png");
    if (GrabWindowSnapshot(root_window, rect, &png_data->data())) {
      PostSaveScreenshotTask(
          base::Bind(&ScreenshotTaker::ShowNotification, factory_.GetWeakPtr()),
          profile_,
          screenshot_path,
          png_data);
    } else {
      LOG(ERROR) << "Failed to grab the window screenshot for " << i;
      ShowNotification(
          ScreenshotTakerObserver::SCREENSHOT_GRABWINDOW_FULL_FAILED,
          screenshot_path);
    }
  }
  last_screenshot_timestamp_ = base::Time::Now();
}

void ScreenshotTaker::HandleTakePartialScreenshot(
    aura::Window* window, const gfx::Rect& rect) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  base::FilePath screenshot_directory;
  if (!screenshot_directory_for_test_.empty()) {
    screenshot_directory = screenshot_directory_for_test_;
  } else if (!ScreenshotSource::GetScreenshotDirectory(&screenshot_directory)) {
    ShowNotification(ScreenshotTakerObserver::SCREENSHOT_GET_DIR_FAILED,
                     base::FilePath());
    return;
  }

  scoped_refptr<base::RefCountedBytes> png_data(new base::RefCountedBytes);

  std::string screenshot_basename = !screenshot_basename_for_test_.empty() ?
      screenshot_basename_for_test_ :
      ScreenshotSource::GetScreenshotBaseFilename();
  base::FilePath screenshot_path =
      screenshot_directory.AppendASCII(screenshot_basename + ".png");
  if (GrabWindowSnapshot(window, rect, &png_data->data())) {
    last_screenshot_timestamp_ = base::Time::Now();
    PostSaveScreenshotTask(
        base::Bind(&ScreenshotTaker::ShowNotification, factory_.GetWeakPtr()),
        profile_,
        screenshot_path,
        png_data);
  } else {
    LOG(ERROR) << "Failed to grab the window screenshot";
    ShowNotification(
        ScreenshotTakerObserver::SCREENSHOT_GRABWINDOW_PARTIAL_FAILED,
        screenshot_path);
  }
}

bool ScreenshotTaker::CanTakeScreenshot() {
  return last_screenshot_timestamp_.is_null() ||
      base::Time::Now() - last_screenshot_timestamp_ >
      base::TimeDelta::FromMilliseconds(
          kScreenshotMinimumIntervalInMS);
}

void ScreenshotTaker::ShowNotification(
    ScreenshotTakerObserver::Result screenshot_result,
    const base::FilePath& screenshot_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
#if defined(OS_CHROMEOS)
  // TODO(sschmitz): make this work for Windows.
  static int id = 0;
  std::string id_text = base::StringPrintf("screenshot_%3.3d", ++id);
  string16 replace_id = UTF8ToUTF16(id_text);
  bool success =
      (screenshot_result == ScreenshotTakerObserver::SCREENSHOT_SUCCESS);
  Notification notification(
      GURL(kNotificationOriginUrl),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_SCREENSHOT_NOTIFICATION_ICON),
      l10n_util::GetStringUTF16(
          success ?
          IDS_ASH_SCREENSHOT_NOTIFICATION_TITLE_SUCCESS :
          IDS_ASH_SCREENSHOT_NOTIFICATION_TITLE_FAIL),
      l10n_util::GetStringUTF16(
          success ?
          IDS_ASH_SCREENSHOT_NOTIFICATION_TEXT_SUCCESS :
          IDS_ASH_SCREENSHOT_NOTIFICATION_TEXT_FAIL),
      WebKit::WebTextDirectionDefault,
      string16(),
      replace_id,
      new ScreenshotTakerNotificationDelegate(id_text,
                                              success,
                                              screenshot_path));
  g_browser_process->notification_ui_manager()->Add(notification, profile_);
#endif

  FOR_EACH_OBSERVER(ScreenshotTakerObserver, observers_,
                    OnScreenshotCompleted(screenshot_result, screenshot_path));
}

void ScreenshotTaker::AddObserver(ScreenshotTakerObserver* observer) {
  observers_.AddObserver(observer);
}

void ScreenshotTaker::RemoveObserver(ScreenshotTakerObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool ScreenshotTaker::HasObserver(ScreenshotTakerObserver* observer) const {
  return observers_.HasObserver(observer);
}

void ScreenshotTaker::SetScreenshotDirectoryForTest(
    const base::FilePath& directory) {
  screenshot_directory_for_test_ = directory;
}

void ScreenshotTaker::SetScreenshotBasenameForTest(
    const std::string& basename){
  screenshot_basename_for_test_ = basename;
}
