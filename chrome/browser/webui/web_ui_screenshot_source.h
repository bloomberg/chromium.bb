// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_WEB_UI_SCREENSHOT_SOURCE_H_
#define CHROME_BROWSER_WEBUI_WEB_UI_SCREENSHOT_SOURCE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/webui/chrome_url_data_manager.h"

// ScreenshotSource is the data source that serves screenshots (saved
// or current) to the bug report html ui
class WebUIScreenshotSource : public ChromeURLDataManager::DataSource {
 public:
  explicit WebUIScreenshotSource(
      std::vector<unsigned char>* current_screenshot);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);

  virtual std::string GetMimeType(const std::string&) const;

  std::vector<unsigned char> GetScreenshot(const std::string& path);

 private:
  virtual ~WebUIScreenshotSource();

  std::vector<unsigned char> current_screenshot_;

  DISALLOW_COPY_AND_ASSIGN(WebUIScreenshotSource);
};

#endif  // CHROME_BROWSER_WEBUI_WEB_UI_SCREENSHOT_SOURCE_H_
