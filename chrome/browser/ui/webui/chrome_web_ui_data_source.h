// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_DATA_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_DATA_SOURCE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"

// A data source that can help with implementing the common operations
// needed by the chrome WEBUI settings/history/downloads pages.
// DO NOT DERIVE FROM THIS CLASS! http://crbug.com/169170
class ChromeWebUIDataSource : public ChromeURLDataManager::DataSource {
 public:
  // Used as a parameter to GotDataCallback. The caller has to run this callback
  // with the result for the path that they filtered, passing ownership of the
  // memory.
  typedef base::Callback<void(base::RefCountedMemory*)> GotDataCallback;

  // Used by SetRequestFilter. The string parameter is the path of the request.
  // If the callee doesn't want to handle the data, false is returned. Otherwise
  // true is returned and the GotDataCallback parameter is called either then or
  // asynchronously with the response.
  typedef base::Callback<bool(const std::string&, const GotDataCallback&)>
      HandleRequestCallback;

  explicit ChromeWebUIDataSource(const std::string& source_name);
  ChromeWebUIDataSource(const std::string& source_name, MessageLoop* loop);

  // Adds a string keyed to its name to our dictionary.
  void AddString(const std::string& name, const string16& value);

  // Adds a string keyed to its name to our dictionary.
  void AddString(const std::string& name, const std::string& value);

  // Adds a localized string with resource |ids| keyed to its name to our
  // dictionary.
  void AddLocalizedString(const std::string& name, int ids);

  // Add strings from |localized_strings| to our dictionary.
  void AddLocalizedStrings(const DictionaryValue& localized_strings);

  // Allows a caller to add a filter for URL requests.
  void SetRequestFilter(const HandleRequestCallback& callback);

  // Accessor for |localized_strings_|.
  DictionaryValue* localized_strings() {
    return &localized_strings_;
  }

  // Sets the path which will return the JSON strings.
  void set_json_path(const std::string& path) { json_path_ = path; }

  // Sets the data source to use a slightly different format for json data. Some
  // day this should become the default.
  void set_use_json_js_format_v2() { json_js_format_v2_ = true; }

  // Adds a mapping between a path name and a resource to return.
  void add_resource_path(const std::string &path, int resource_id) {
    path_to_idr_map_[path] = resource_id;
  }

  // Sets the resource to returned when no other paths match.
  void set_default_resource(int resource_id) {
    default_resource_ = resource_id;
  }

 protected:
  virtual ~ChromeWebUIDataSource();

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
  bool json_js_format_v2_;
  std::string json_path_;
  std::map<std::string, int> path_to_idr_map_;
  DictionaryValue localized_strings_;
  HandleRequestCallback filter_callback_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebUIDataSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_DATA_SOURCE_H_
