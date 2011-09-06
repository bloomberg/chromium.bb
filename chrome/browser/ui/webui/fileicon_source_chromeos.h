// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FILEICON_SOURCE_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_FILEICON_SOURCE_CHROMEOS_H_
#pragma once

#include <string>
#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"

namespace net {
class URLRequest;
class URLRequestJob;
}  // namespace net

class GURL;

// A DataSource for chrome://fileicon/ URLs, but reads from
// resources instead of disk. Allows for parameterization
// via chrome://fileicon/path.ext?size=[small|medium|large]
class FileIconSourceCros : public ChromeURLDataManager::DataSource {
 public:
  FileIconSourceCros();

  // Overridden from ChromeURLDataManager::DataSource.
  virtual void StartDataRequest(const std::string& url,
                                bool is_incognito,
                                int request_id) OVERRIDE;
  virtual std::string GetMimeType(const std::string&) const OVERRIDE;

 private:
  virtual ~FileIconSourceCros();

  DISALLOW_COPY_AND_ASSIGN(FileIconSourceCros);
};

#endif  // CHROME_BROWSER_UI_WEBUI_FILEICON_SOURCE_CHROMEOS_H_

