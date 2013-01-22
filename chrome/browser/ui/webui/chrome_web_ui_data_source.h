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
#include "base/values.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_data_source.h"

// A data source that can help with implementing the common operations
// needed by the chrome WEBUI settings/history/downloads pages.
// DO NOT DERIVE FROM THIS CLASS! http://crbug.com/169170
class ChromeWebUIDataSource : public URLDataSourceImpl,
                              public content::WebUIDataSource {
 public:
  static content::WebUIDataSource* Create(const std::string& source_name);

  // content::WebUIDataSource implementation:
  virtual void AddString(const std::string& name,
                         const string16& value) OVERRIDE;
  virtual void AddString(const std::string& name,
                         const std::string& value) OVERRIDE;
  virtual void AddLocalizedString(const std::string& name, int ids) OVERRIDE;
  virtual void AddLocalizedStrings(
      const DictionaryValue& localized_strings) OVERRIDE;
  virtual void AddBoolean(const std::string& name, bool value) OVERRIDE;
  virtual void SetJsonPath(const std::string& path) OVERRIDE;
  virtual void SetUseJsonJSFormatV2() OVERRIDE;
  virtual void AddResourcePath(const std::string &path,
                               int resource_id) OVERRIDE;
  virtual void SetDefaultResource(int resource_id) OVERRIDE;
  virtual void SetRequestFilter(
      const content::WebUIDataSource::HandleRequestCallback& callback) OVERRIDE;
  virtual void DisableContentSecurityPolicy() OVERRIDE;
  virtual void OverrideContentSecurityPolicyObjectSrc(
      const std::string& data) OVERRIDE;
  virtual void OverrideContentSecurityPolicyFrameSrc(
      const std::string& data) OVERRIDE;
  virtual void DisableDenyXFrameOptions() OVERRIDE;

 protected:
  virtual ~ChromeWebUIDataSource();

  // Completes a request by sending our dictionary of localized strings.
  void SendLocalizedStringsAsJSON(
      const content::URLDataSource::GotDataCallback& callback);

  // Completes a request by sending the file specified by |idr|.
  void SendFromResourceBundle(
      const content::URLDataSource::GotDataCallback& callback, int idr);

 private:
  class InternalDataSource;
  friend class InternalDataSource;
  friend class MockChromeWebUIDataSource;

  explicit ChromeWebUIDataSource(const std::string& source_name);

  // Methods that match content::URLDataSource which are called by
  // InternalDataSource.
  std::string GetSource();
  std::string GetMimeType(const std::string& path) const;
  void StartDataRequest(
      const std::string& path,
      bool is_incognito,
      const content::URLDataSource::GotDataCallback& callback);

  // The name of this source.
  // E.g., for favicons, this could be "favicon", which results in paths for
  // specific resources like "favicon/34" getting sent to this source.
  std::string source_name_;
  int default_resource_;
  bool json_js_format_v2_;
  std::string json_path_;
  std::map<std::string, int> path_to_idr_map_;
  DictionaryValue localized_strings_;
  content::WebUIDataSource::HandleRequestCallback filter_callback_;
  bool add_csp_;
  bool object_src_set_;
  std::string object_src_;
  bool frame_src_set_;
  std::string frame_src_;
  bool deny_xframe_options_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebUIDataSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_DATA_SOURCE_H_
