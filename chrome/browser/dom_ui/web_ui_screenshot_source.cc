// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/web_ui_screenshot_source.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/ref_counted_memory.h"
#include "base/synchronization/waitable_event.h"
#include "base/task.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"

static const char kCurrentScreenshot[] = "current";
#if defined(OS_CHROMEOS)
static const char kSavedScreenshots[] = "saved/";
#endif

static const char kScreenshotsRelativePath[] = "/Screenshots/";

#if defined(OS_CHROMEOS)
// Read the file from the screenshots directory into the read_bytes vector.
void ReadScreenshot(const std::string& filename,
                   std::vector<unsigned char>* read_bytes,
                   base::WaitableEvent* read_complete) {
  read_bytes->clear();

  FilePath fileshelf_path;
  if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &fileshelf_path)) {
    read_complete->Signal();
    return;
  }

  FilePath file(fileshelf_path.value() + std::string(kScreenshotsRelativePath) +
                filename);

  int64 file_size = 0;
  if (!file_util::GetFileSize(file, &file_size)) {
    read_complete->Signal();
    return;
  }

  // expand vector to file size
  read_bytes->resize(file_size);
  // read file into the vector
  int bytes_read = 0;
  if (!(bytes_read = file_util::ReadFile(file,
                                         reinterpret_cast<char*>(
                                             &read_bytes->front()),
                                             static_cast<int>(file_size))))
    read_bytes->clear();

  // We're done, if successful, read_bytes will have the data
  // otherwise, it'll be empty.
  read_complete->Signal();
}

// Get a saved screenshot - read on the FILE thread.
std::vector<unsigned char> GetSavedScreenshot(std::string filename) {
  base::WaitableEvent read_complete(true, false);
  std::vector<unsigned char> bytes;
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          NewRunnableFunction(&ReadScreenshot, filename,
                                              &bytes, &read_complete));
  read_complete.Wait();
  return bytes;
}
#endif

std::vector<unsigned char> WebUIScreenshotSource::GetScreenshot(
    const std::string& full_path) {
  // Strip the query param value - we only use it as a hack to ensure our
  // image gets reloaded instead of being pulled from the browser cache
  std::string path = full_path.substr(0, full_path.find_first_of("?"));
  if (path == kCurrentScreenshot) {
    return current_screenshot_;
#if defined(OS_CHROMEOS)
  } else if (path.compare(0, strlen(kSavedScreenshots),
                          kSavedScreenshots) == 0) {
    // Split the saved screenshot filename from the path
    std::string filename = path.substr(strlen(kSavedScreenshots));

    return GetSavedScreenshot(filename);
#endif
  } else {
    std::vector<unsigned char> ret;
    // TODO(rkc): Weird vc bug, return std::vector<unsigned char>() causes
    // the object assigned to the return value of this function magically
    // change it's address 0x0; look into this eventually.
    return ret;
  }
}

WebUIScreenshotSource::WebUIScreenshotSource(
    std::vector<unsigned char>* current_screenshot)
    : DataSource(chrome::kChromeUIScreenshotPath, MessageLoop::current()) {
  // Setup the last screenshot taken.
  if (current_screenshot)
    current_screenshot_ = *current_screenshot;
  else
    current_screenshot_.clear();
}

WebUIScreenshotSource::~WebUIScreenshotSource() {}

void WebUIScreenshotSource::StartDataRequest(const std::string& path,
                                            bool is_off_the_record,
                                            int request_id) {
  SendResponse(request_id, new RefCountedBytes(GetScreenshot(path)));
}

std::string WebUIScreenshotSource::GetMimeType(const std::string&) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}
