// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_SHARED_RESOURCES_DATA_SOURCE_H_
#define CHROME_BROWSER_DOM_UI_SHARED_RESOURCES_DATA_SOURCE_H_
#pragma once

#include "chrome/browser/dom_ui/chrome_url_data_manager.h"

namespace net {
class URLRequest;
class URLRequestJob;
}  // namespace net

class GURL;

// A DataSource for chrome://resources/ URLs.
class SharedResourcesDataSource : public ChromeURLDataManager::DataSource {
 public:
  // Registers an instance of this data source with the ChromeUrlDataManager.
  static void Register();

  // Overridden from ChromeURLDataManager::DataSource:
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const;

 private:
  SharedResourcesDataSource();
  ~SharedResourcesDataSource();

  DISALLOW_COPY_AND_ASSIGN(SharedResourcesDataSource);
};

#endif  // CHROME_BROWSER_DOM_UI_SHARED_RESOURCES_DATA_SOURCE_H_
