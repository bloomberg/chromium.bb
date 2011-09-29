// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_DATA_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_DATA_SOURCE_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"

// A data source that can help with implementing the common operations
// needed by the chrome WEBUI settings/history/downloads pages.
class ChromeWebUIDataSource : public ChromeURLDataManager::DataSource {
 public:
  explicit ChromeWebUIDataSource(const std::string& source_name);
  ChromeWebUIDataSource(const std::string& source_name, MessageLoop* loop);

  // Adds a string and its equivalent to our dictionary.
  void AddString(const std::string& name, const string16& value);

  // Adds a name and its equivaled localized string to our dictionary.
  void AddLocalizedString(const std::string& name, int ids);

  // Add strings from |localized_strings| to our dictionary.
  void AddLocalizedStrings(const DictionaryValue& localized_strings);

  // Sets the path which will return the JSON strings.
  void set_json_path(const std::string& path) { json_path_ = path; }

  // Adds a mapping between a path name and a resource to return.
  void add_resource_path(const std::string &path, int resource_id) {
    path_to_idr_map_[path] = resource_id;
  }

  // Sets the resource to returned when no other paths match.
  void set_default_resource(int resource_id) {
    default_resource_ = resource_id;
  }

  virtual ~ChromeWebUIDataSource();

 protected:
  // Completes a request by sending our dictionary of localized strings.
  void SendLocalizedStringsAsJSON(int request_id);

  // Completes a request by sending the file specified by |idr|.
  void SendFromResourceBundle(int request_id, int idr);

  // ChromeURLDataManager
  virtual std::string GetMimeType(const std::string& path) const OVERRIDE;
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id) OVERRIDE;

 private:
  int default_resource_;
  std::string json_path_;
  std::map<std::string, int> path_to_idr_map_;
  DictionaryValue localized_strings_;
  DISALLOW_COPY_AND_ASSIGN(ChromeWebUIDataSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_DATA_SOURCE_H_
