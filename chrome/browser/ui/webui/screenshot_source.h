// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SCREENSHOT_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_SCREENSHOT_SOURCE_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "content/public/browser/url_data_source_delegate.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#endif

typedef std::vector<unsigned char> ScreenshotData;
typedef linked_ptr<ScreenshotData> ScreenshotDataPtr;

class FilePath;
class Profile;

// ScreenshotSource is the data source that serves screenshots (saved
// or current) to the bug report html ui.
class ScreenshotSource : public content::URLDataSourceDelegate {
 public:
  explicit ScreenshotSource(
      std::vector<unsigned char>* current_screenshot,
      Profile* profile);

#if defined(USE_ASH)

  // Queries the browser process to determine if screenshots are disabled.
  static bool AreScreenshotsDisabled();

  // Common access for the screenshot directory, parameter is set to the
  // requested directory and return value of true is given upon success.
  static bool GetScreenshotDirectory(FilePath* directory);
#endif

  // Get the basefilename for screenshots
  static std::string GetScreenshotBaseFilename();

  // content::URLDataSourceDelegate implementation.
  virtual std::string GetSource() OVERRIDE;
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id) OVERRIDE;
  virtual std::string GetMimeType(const std::string&) const OVERRIDE;

  // Get the screenshot specified by the given relative path that we've cached
  // from a previous request to the screenshots source.
  // Note: This method strips the query string from the given path.
  ScreenshotDataPtr GetCachedScreenshot(const std::string& screenshot_path);

  // Url that represents the base directory for screenshots.
  static const char kScreenshotUrlRoot[];
  // Identifier for the current screenshot
  // (relative to screenshot base directory).
  static const char kScreenshotCurrent[];
  // Path for directory where screenshots are saved
  // (relative to screenshot base directory).
  static const char kScreenshotSaved[];
#if defined(OS_CHROMEOS)
  // Common prefix to screenshot filenames.
  static const char kScreenshotPrefix[];
  // Common suffix to screenshot filenames.
  static const char kScreenshotSuffix[];
#endif

 private:
  virtual ~ScreenshotSource();

  // Send the screenshot specified by the given relative path to the requestor.
  // This is the ancestor for SendSavedScreenshot and CacheAndSendScreenshot.
  // All calls to send a screenshot should only call this method.
  // Note: This method strips the query string from the given path.
  void SendScreenshot(const std::string& screenshot_path, int request_id);
#if defined(OS_CHROMEOS)
  // Send a saved screenshot image file specified by the given screenshot path
  // to the requestor.
  void SendSavedScreenshot(const std::string& screenshot_path,
                           int request_id,
                           const FilePath& file);

  // The callback for Drive's getting file method.
  void GetSavedScreenshotCallback(const std::string& screenshot_path,
                                  int request_id,
                                  drive::DriveFileError error,
                                  const FilePath& file,
                                  const std::string& unused_mime_type,
                                  drive::DriveFileType file_type);

#endif
  // Sends the screenshot data to the requestor while caching it locally to the
  // class instance, indexed by path.
  void CacheAndSendScreenshot(const std::string& screenshot_path,
                              int request_id,
                              ScreenshotDataPtr bytes);

  // Pointer to the screenshot data for the current screenshot.
  ScreenshotDataPtr current_screenshot_;

  Profile* profile_;

  // Key: Relative path to the screenshot (including filename)
  // Value: Pointer to the screenshot data associated with the path.
  std::map<std::string, ScreenshotDataPtr> cached_screenshots_;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SCREENSHOT_SOURCE_H_
