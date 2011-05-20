// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_DATA_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_DATA_SOURCE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"

// A data source that can help with implementing the common operations
// needed by the chrome WEBUI settings/history/downloads pages.
class ChromeWebUIDataSource : public ChromeURLDataManager::DataSource {
 public:
  explicit ChromeWebUIDataSource(const std::string& source_name);

  // Adds a name and its equivaled localized string to our dictionary.
  void AddLocalizedString(const std::string& name, int ids);

  // Completes a request by sending our dictionary of localized strings.
  void SendLocalizedStringsAsJSON(int request_id);

  // Completes a request by sending the file specified by |idr|.
  void SendFromResourceBundle(int request_id, int idr);

 protected:
  virtual ~ChromeWebUIDataSource();

 private:
  DictionaryValue localized_strings_;
  DISALLOW_COPY_AND_ASSIGN(ChromeWebUIDataSource);
};


#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_DATA_SOURCE_H_

