// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/screenshot_source.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_util.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_interface.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"
#include "chrome/browser/chromeos/gdata/drive_system_service.h"
#include "content/public/browser/browser_thread.h"
#endif

static const char kCurrentScreenshotFilename[] = "current";
#if defined(OS_CHROMEOS)
static const char kSavedScreenshotsBasePath[] = "saved/";
#endif

ScreenshotSource::ScreenshotSource(
    std::vector<unsigned char>* current_screenshot,
    Profile* profile)
    : DataSource(chrome::kChromeUIScreenshotPath, MessageLoop::current()),
      profile_(profile) {
  // Setup the last screenshot taken.
  if (current_screenshot)
    current_screenshot_.reset(new ScreenshotData(*current_screenshot));
  else
    current_screenshot_.reset(new ScreenshotData());
}

ScreenshotSource::~ScreenshotSource() {}

void ScreenshotSource::StartDataRequest(const std::string& path, bool,
                                        int request_id) {
  SendScreenshot(path, request_id);
}

std::string ScreenshotSource::GetMimeType(const std::string&) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

ScreenshotDataPtr ScreenshotSource::GetCachedScreenshot(
    const std::string& screenshot_path) {
  std::map<std::string, ScreenshotDataPtr>::iterator pos;
  std::string path = screenshot_path.substr(
      0, screenshot_path.find_first_of("?"));
  if ((pos = cached_screenshots_.find(path)) != cached_screenshots_.end()) {
    return pos->second;
  } else {
    return ScreenshotDataPtr(new ScreenshotData);
  }
}

void ScreenshotSource::SendScreenshot(const std::string& screenshot_path,
                                      int request_id) {
  // Strip the query param value - we only use it as a hack to ensure our
  // image gets reloaded instead of being pulled from the browser cache
  std::string path = screenshot_path.substr(
      0, screenshot_path.find_first_of("?"));
  if (path == kCurrentScreenshotFilename) {
    CacheAndSendScreenshot(path, request_id, current_screenshot_);
#if defined(OS_CHROMEOS)
  } else if (path.compare(0, strlen(kSavedScreenshotsBasePath),
                          kSavedScreenshotsBasePath) == 0) {
    using content::BrowserThread;

    std::string filename = path.substr(strlen(kSavedScreenshotsBasePath));

    url_canon::RawCanonOutputT<char16> decoded;
    url_util::DecodeURLEscapeSequences(
        filename.data(), filename.size(), &decoded);
    // Screenshot filenames don't use non-ascii characters.
    std::string decoded_filename = UTF16ToASCII(string16(
        decoded.data(), decoded.length()));

    DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(
        ash::Shell::GetInstance()->delegate()->GetCurrentBrowserContext());
    FilePath download_path = download_prefs->DownloadPath();
    if (gdata::util::IsUnderDriveMountPoint(download_path)) {
      gdata::DriveFileSystemInterface* file_system =
          gdata::DriveSystemServiceFactory::GetForProfile(
              profile_)->file_system();
      file_system->GetFileByResourceId(
          decoded_filename,
          base::Bind(&ScreenshotSource::GetSavedScreenshotCallback,
                     base::Unretained(this), screenshot_path, request_id),
          gdata::GetContentCallback());
    } else {
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&ScreenshotSource::SendSavedScreenshot,
                     base::Unretained(this),
                     screenshot_path,
                     request_id, download_path.Append(decoded_filename)));
    }
#endif
  } else {
    CacheAndSendScreenshot(
        path, request_id, ScreenshotDataPtr(new ScreenshotData()));
  }
}

#if defined(OS_CHROMEOS)
void ScreenshotSource::SendSavedScreenshot(
    const std::string& screenshot_path,
    int request_id,
    const FilePath& file) {
  ScreenshotDataPtr read_bytes(new ScreenshotData);
  int64 file_size = 0;

  if (!file_util::GetFileSize(file, &file_size)) {
    CacheAndSendScreenshot(screenshot_path, request_id, read_bytes);
    return;
  }

  read_bytes->resize(file_size);
  if (!file_util::ReadFile(file, reinterpret_cast<char*>(&read_bytes->front()),
                           static_cast<int>(file_size)))
    read_bytes->clear();

  CacheAndSendScreenshot(screenshot_path, request_id, read_bytes);
}

void ScreenshotSource::GetSavedScreenshotCallback(
    const std::string& screenshot_path,
    int request_id,
    gdata::DriveFileError error,
    const FilePath& file,
    const std::string& unused_mime_type,
    gdata::DriveFileType file_type) {
  if (error != gdata::DRIVE_FILE_OK || file_type != gdata::REGULAR_FILE) {
    ScreenshotDataPtr read_bytes(new ScreenshotData);
    CacheAndSendScreenshot(screenshot_path, request_id, read_bytes);
    return;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&ScreenshotSource::SendSavedScreenshot,
                 base::Unretained(this), screenshot_path, request_id, file));
}
#endif

void ScreenshotSource::CacheAndSendScreenshot(
    const std::string& screenshot_path,
    int request_id,
    ScreenshotDataPtr bytes) {
  cached_screenshots_[screenshot_path] = bytes;
  SendResponse(request_id, new base::RefCountedBytes(*bytes));
}
