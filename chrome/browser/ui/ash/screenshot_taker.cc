// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/screenshot_taker.h"

#include <climits>
#include <string>

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/system_notifier.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "grit/ash_strings.h"
#include "grit/theme_resources.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"
#include "ui/strings/grit/ui_strings.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/open_util.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chromeos/login/login_state.h"
#endif

namespace {
// The minimum interval between two screenshot commands.  It has to be
// more than 1000 to prevent the conflict of filenames.
const int kScreenshotMinimumIntervalInMS = 1000;

const char kNotificationId[] = "screenshot";

#if defined(OS_CHROMEOS)
const char kNotificationOriginUrl[] = "chrome://screenshot";
#endif

const char kImageClipboardFormatPrefix[] = "<img src='data:image/png;base64,";
const char kImageClipboardFormatSuffix[] = "'>";

void CopyScreenshotToClipboard(scoped_refptr<base::RefCountedString> png_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string encoded;
  base::Base64Encode(png_data->data(), &encoded);

  // Only cares about HTML because ChromeOS doesn't need other formats.
  // TODO(dcheng): Why don't we take advantage of the ability to write bitmaps
  // to the clipboard here?
  {
    ui::ScopedClipboardWriter scw(ui::CLIPBOARD_TYPE_COPY_PASTE);
    std::string html(kImageClipboardFormatPrefix);
    html += encoded;
    html += kImageClipboardFormatSuffix;
    scw.WriteHTML(base::UTF8ToUTF16(html), std::string());
  }
  content::RecordAction(base::UserMetricsAction("Screenshot_CopyClipboard"));
}

void ReadFileAndCopyToClipboardLocal(const base::FilePath& screenshot_path) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  scoped_refptr<base::RefCountedString> png_data(new base::RefCountedString());
  if (!base::ReadFileToString(screenshot_path, &(png_data->data()))) {
    LOG(ERROR) << "Failed to read the screenshot file: "
               << screenshot_path.value();
    return;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(CopyScreenshotToClipboard, png_data));
}

#if defined(OS_CHROMEOS)
void ReadFileAndCopyToClipboardDrive(drive::FileError error,
                                     const base::FilePath& file_path,
                                     scoped_ptr<drive::ResourceEntry> entry) {
  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Failed to read the screenshot path on drive: "
               << drive::FileErrorToString(error);
    return;
  }
  content::BrowserThread::GetBlockingPool()->PostTask(
      FROM_HERE,
      base::Bind(&ReadFileAndCopyToClipboardLocal, file_path));
}
#endif

// Delegate for a notification. This class has two roles: to implement callback
// methods for notification, and to provide an identity of the associated
// notification.
class ScreenshotTakerNotificationDelegate : public NotificationDelegate {
 public:
  ScreenshotTakerNotificationDelegate(bool success,
                                      Profile* profile,
                                      const base::FilePath& screenshot_path)
      : success_(success),
        profile_(profile),
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
    file_manager::util::ShowItemInFolder(profile_, screenshot_path_);
#else
    // TODO(sschmitz): perhaps add similar action for Windows.
#endif
  }
  virtual void ButtonClick(int button_index) OVERRIDE {
    DCHECK(success_ && button_index == 0);

    // To avoid keeping the screenshot image on memory, it will re-read the
    // screenshot file and copy it to the clipboard.
#if defined(OS_CHROMEOS)
  if (drive::util::IsUnderDriveMountPoint(screenshot_path_)) {
    drive::FileSystemInterface* file_system =
        drive::util::GetFileSystemByProfile(profile_);
    file_system->GetFile(
        drive::util::ExtractDrivePath(screenshot_path_),
        base::Bind(&ReadFileAndCopyToClipboardDrive));
    return;
  }
#endif
    content::BrowserThread::GetBlockingPool()->PostTask(
        FROM_HERE, base::Bind(
            &ReadFileAndCopyToClipboardLocal, screenshot_path_));
  }
  virtual bool HasClickedListener() OVERRIDE { return success_; }
  virtual std::string id() const OVERRIDE {
    return std::string(kNotificationId);
  }
  virtual content::WebContents* GetWebContents() const OVERRIDE {
    return NULL;
  }

 private:
  virtual ~ScreenshotTakerNotificationDelegate() {}

  const bool success_;
  Profile* profile_;
  const base::FilePath screenshot_path_;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotTakerNotificationDelegate);
};

typedef base::Callback<
  void(ScreenshotTakerObserver::Result screenshot_result,
       const base::FilePath& screenshot_path)> ShowNotificationCallback;

void SaveScreenshotInternal(const ShowNotificationCallback& callback,
                            const base::FilePath& screenshot_path,
                            const base::FilePath& local_path,
                            scoped_refptr<base::RefCountedBytes> png_data) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  DCHECK(!local_path.empty());
  ScreenshotTakerObserver::Result result =
      ScreenshotTakerObserver::SCREENSHOT_SUCCESS;
  if (static_cast<size_t>(base::WriteFile(
          local_path,
          reinterpret_cast<char*>(&(png_data->data()[0])),
          png_data->size())) != png_data->size()) {
    LOG(ERROR) << "Failed to save to " << local_path.value();
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

  if (!base::CreateDirectory(screenshot_path.DirName())) {
    LOG(ERROR) << "Failed to ensure the existence of "
               << screenshot_path.DirName().value();
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(callback,
                   ScreenshotTakerObserver::SCREENSHOT_CREATE_DIR_FAILED,
                   screenshot_path));
    return;
  }
  SaveScreenshotInternal(callback, screenshot_path, screenshot_path, png_data);
}

// TODO(kinaba): crbug.com/140425, remove this ungly #ifdef dispatch.
#if defined(OS_CHROMEOS)
void SaveScreenshotToDrive(const ShowNotificationCallback& callback,
                           const base::FilePath& screenshot_path,
                           scoped_refptr<base::RefCountedBytes> png_data,
                           drive::FileError error,
                           const base::FilePath& local_path) {
  // |screenshot_path| is used in the notification callback.
  // |local_path| is a temporary file in a hidden cache directory used for
  // internal work generated by drive::util::PrepareWritableFileAndRun.
  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Failed to write screenshot image to Google Drive: " << error;
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(callback,
                   ScreenshotTakerObserver::SCREENSHOT_CREATE_FILE_FAILED,
                   screenshot_path));
    return;
  }
  SaveScreenshotInternal(callback, screenshot_path, local_path, png_data);
}

void EnsureDirectoryExistsCallback(
    const ShowNotificationCallback& callback,
    Profile* profile,
    const base::FilePath& screenshot_path,
    scoped_refptr<base::RefCountedBytes> png_data,
    drive::FileError error) {
  // It is okay to fail with FILE_ERROR_EXISTS since anyway the directory
  // of the target file exists.
  if (error == drive::FILE_ERROR_OK ||
      error == drive::FILE_ERROR_EXISTS) {
    drive::util::PrepareWritableFileAndRun(
        profile,
        screenshot_path,
        base::Bind(&SaveScreenshotToDrive,
                   callback,
                   screenshot_path,
                   png_data));
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

bool ShouldUse24HourClock() {
#if defined(OS_CHROMEOS)
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (profile) {
    return profile->GetPrefs()->GetBoolean(prefs::kUse24HourClock);
  }
#endif
  return base::GetHourClockType() == base::k24HourClock;
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

bool GetScreenshotDirectory(base::FilePath* directory) {
  bool is_logged_in = true;

#if defined(OS_CHROMEOS)
  is_logged_in = chromeos::LoginState::Get()->IsUserLoggedIn();
#endif

  if (is_logged_in) {
    DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(
        ProfileManager::GetActiveUserProfile());
    *directory = download_prefs->DownloadPath();
  } else  {
    if (!base::GetTempDir(directory)) {
      LOG(ERROR) << "Failed to find temporary directory.";
      return false;
    }
  }
  return true;
}

#if defined(OS_CHROMEOS)
const int GetScreenshotNotificationTitle(
    ScreenshotTakerObserver::Result screenshot_result) {
  switch (screenshot_result) {
    case ScreenshotTakerObserver::SCREENSHOTS_DISABLED:
      return IDS_ASH_SCREENSHOT_NOTIFICATION_TITLE_DISABLED;
    case ScreenshotTakerObserver::SCREENSHOT_SUCCESS:
      return IDS_ASH_SCREENSHOT_NOTIFICATION_TITLE_SUCCESS;
    default:
      return IDS_ASH_SCREENSHOT_NOTIFICATION_TITLE_FAIL;
  }
}

const int GetScreenshotNotificationText(
    ScreenshotTakerObserver::Result screenshot_result) {
  switch (screenshot_result) {
    case ScreenshotTakerObserver::SCREENSHOTS_DISABLED:
      return IDS_ASH_SCREENSHOT_NOTIFICATION_TEXT_DISABLED;
    case ScreenshotTakerObserver::SCREENSHOT_SUCCESS:
      return IDS_ASH_SCREENSHOT_NOTIFICATION_TEXT_SUCCESS;
    default:
      return IDS_ASH_SCREENSHOT_NOTIFICATION_TEXT_FAIL;
  }
}
#endif

}  // namespace

ScreenshotTaker::ScreenshotTaker()
    : profile_for_test_(NULL),
      factory_(this) {
}

ScreenshotTaker::~ScreenshotTaker() {
}

void ScreenshotTaker::HandleTakeScreenshotForAllRootWindows() {
  if (g_browser_process->local_state()->
          GetBoolean(prefs::kDisableScreenshots)) {
    ShowNotification(ScreenshotTakerObserver::SCREENSHOTS_DISABLED,
                     base::FilePath());
    return;
  }
  base::FilePath screenshot_directory;
  if (!screenshot_directory_for_test_.empty()) {
    screenshot_directory = screenshot_directory_for_test_;
  } else if (!GetScreenshotDirectory(&screenshot_directory)) {
    ShowNotification(ScreenshotTakerObserver::SCREENSHOT_GET_DIR_FAILED,
                     base::FilePath());
    return;
  }
  std::string screenshot_basename = !screenshot_basename_for_test_.empty() ?
      screenshot_basename_for_test_ : GetScreenshotBaseFilename();

  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();
  // Reorder root_windows to take the primary root window's snapshot at first.
  aura::Window* primary_root = ash::Shell::GetPrimaryRootWindow();
  if (*(root_windows.begin()) != primary_root) {
    root_windows.erase(std::find(
        root_windows.begin(), root_windows.end(), primary_root));
    root_windows.insert(root_windows.begin(), primary_root);
  }
  for (size_t i = 0; i < root_windows.size(); ++i) {
    aura::Window* root_window = root_windows[i];
    std::string basename = screenshot_basename;
    gfx::Rect rect = root_window->bounds();
    if (root_windows.size() > 1)
      basename += base::StringPrintf(" - Display %d", static_cast<int>(i + 1));
    base::FilePath screenshot_path =
        screenshot_directory.AppendASCII(basename + ".png");
    GrabFullWindowSnapshotAsync(
        root_window, rect, GetProfile(), screenshot_path, i);
  }
  content::RecordAction(base::UserMetricsAction("Screenshot_TakeFull"));
}

void ScreenshotTaker::HandleTakePartialScreenshot(
    aura::Window* window, const gfx::Rect& rect) {
  if (g_browser_process->local_state()->
          GetBoolean(prefs::kDisableScreenshots)) {
    ShowNotification(ScreenshotTakerObserver::SCREENSHOTS_DISABLED,
                     base::FilePath());
    return;
  }
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::FilePath screenshot_directory;
  if (!screenshot_directory_for_test_.empty()) {
    screenshot_directory = screenshot_directory_for_test_;
  } else if (!GetScreenshotDirectory(&screenshot_directory)) {
    ShowNotification(ScreenshotTakerObserver::SCREENSHOT_GET_DIR_FAILED,
                     base::FilePath());
    return;
  }

  std::string screenshot_basename = !screenshot_basename_for_test_.empty() ?
      screenshot_basename_for_test_ : GetScreenshotBaseFilename();
  base::FilePath screenshot_path =
      screenshot_directory.AppendASCII(screenshot_basename + ".png");
  GrabPartialWindowSnapshotAsync(window, rect, GetProfile(), screenshot_path);
  content::RecordAction(base::UserMetricsAction("Screenshot_TakePartial"));
}

bool ScreenshotTaker::CanTakeScreenshot() {
  return last_screenshot_timestamp_.is_null() ||
      base::Time::Now() - last_screenshot_timestamp_ >
      base::TimeDelta::FromMilliseconds(
          kScreenshotMinimumIntervalInMS);
}

#if defined(OS_CHROMEOS)
Notification* ScreenshotTaker::CreateNotification(
    ScreenshotTakerObserver::Result screenshot_result,
    const base::FilePath& screenshot_path) {
  const std::string notification_id(kNotificationId);
  // We cancel a previous screenshot notification, if any, to ensure we get
  // a fresh notification pop-up.
  g_browser_process->notification_ui_manager()->CancelById(notification_id);
  const base::string16 replace_id(base::UTF8ToUTF16(notification_id));
  bool success =
      (screenshot_result == ScreenshotTakerObserver::SCREENSHOT_SUCCESS);
  message_center::RichNotificationData optional_field;
  if (success) {
    const base::string16 label = l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_NOTIFICATION_BUTTON_COPY_SCREENSHOT_TO_CLIPBOARD);
    optional_field.buttons.push_back(message_center::ButtonInfo(label));
  }
  return new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      GURL(kNotificationOriginUrl),
      l10n_util::GetStringUTF16(
          GetScreenshotNotificationTitle(screenshot_result)),
      l10n_util::GetStringUTF16(
          GetScreenshotNotificationText(screenshot_result)),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_SCREENSHOT_NOTIFICATION_ICON),
      blink::WebTextDirectionDefault,
      message_center::NotifierId(
          message_center::NotifierId::SYSTEM_COMPONENT,
          ash::system_notifier::kNotifierScreenshot),
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_NOTIFIER_SCREENSHOT_NAME),
      replace_id,
      optional_field,
      new ScreenshotTakerNotificationDelegate(
          success, GetProfile(), screenshot_path));
}
#endif

void ScreenshotTaker::ShowNotification(
    ScreenshotTakerObserver::Result screenshot_result,
    const base::FilePath& screenshot_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if defined(OS_CHROMEOS)
  // Do not show a notification that a screenshot was taken while no user is
  // logged in, since it is confusing for the user to get a message about it
  // after he logs in (crbug.com/235217).
  if (!chromeos::LoginState::Get()->IsUserLoggedIn())
    return;

  // TODO(sschmitz): make this work for Windows.
  DesktopNotificationService* const service =
      DesktopNotificationServiceFactory::GetForProfile(GetProfile());
  if (service->IsNotifierEnabled(message_center::NotifierId(
          message_center::NotifierId::SYSTEM_COMPONENT,
          ash::system_notifier::kNotifierScreenshot))) {
    scoped_ptr<Notification> notification(
        CreateNotification(screenshot_result, screenshot_path));
    g_browser_process->notification_ui_manager()->Add(*notification,
                                                      GetProfile());
  }
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

void ScreenshotTaker::GrabWindowSnapshotAsyncCallback(
    base::FilePath screenshot_path,
    bool is_partial,
    int window_idx,
    scoped_refptr<base::RefCountedBytes> png_data) {
  if (!png_data.get()) {
    if (is_partial) {
      LOG(ERROR) << "Failed to grab the window screenshot";
      ShowNotification(
          ScreenshotTakerObserver::SCREENSHOT_GRABWINDOW_PARTIAL_FAILED,
          screenshot_path);
    } else {
      LOG(ERROR) << "Failed to grab the window screenshot for " << window_idx;
      ShowNotification(
          ScreenshotTakerObserver::SCREENSHOT_GRABWINDOW_FULL_FAILED,
          screenshot_path);
    }
    return;
  }

  PostSaveScreenshotTask(
      base::Bind(&ScreenshotTaker::ShowNotification, factory_.GetWeakPtr()),
      GetProfile(),
      screenshot_path,
      png_data);
}

void ScreenshotTaker::GrabPartialWindowSnapshotAsync(
    aura::Window* window,
    const gfx::Rect& snapshot_bounds,
    Profile* profile,
    base::FilePath screenshot_path) {
  last_screenshot_timestamp_ = base::Time::Now();

  bool is_partial = true;
  int window_idx = -1;  // unused
  ui::GrabWindowSnapshotAsync(
      window,
      snapshot_bounds,
      content::BrowserThread::GetBlockingPool(),
      base::Bind(&ScreenshotTaker::GrabWindowSnapshotAsyncCallback,
                 factory_.GetWeakPtr(),
                 screenshot_path,
                 is_partial,
                 window_idx));
}

void ScreenshotTaker::GrabFullWindowSnapshotAsync(
    aura::Window* window,
    const gfx::Rect& snapshot_bounds,
    Profile* profile,
    base::FilePath screenshot_path,
    int window_idx) {
  last_screenshot_timestamp_ = base::Time::Now();

  bool is_partial = false;
  ui::GrabWindowSnapshotAsync(
      window,
      snapshot_bounds,
      content::BrowserThread::GetBlockingPool(),
      base::Bind(&ScreenshotTaker::GrabWindowSnapshotAsyncCallback,
                 factory_.GetWeakPtr(),
                 screenshot_path,
                 is_partial,
                 window_idx));
}

Profile* ScreenshotTaker::GetProfile() {
  if (profile_for_test_)
    return profile_for_test_;
  return ProfileManager::GetActiveUserProfile();
}

void ScreenshotTaker::SetScreenshotDirectoryForTest(
    const base::FilePath& directory) {
  screenshot_directory_for_test_ = directory;
}

void ScreenshotTaker::SetScreenshotBasenameForTest(
    const std::string& basename) {
  screenshot_basename_for_test_ = basename;
}

void ScreenshotTaker::SetScreenshotProfileForTest(Profile* profile) {
  profile_for_test_ = profile;
}
