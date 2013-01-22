// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ABOUT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ABOUT_UI_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_controller.h"

class Profile;

// We expose this class because the OOBE flow may need to explicitly add the
// chrome://terms source outside of the normal flow.
class AboutUIHTMLSource : public content::URLDataSource {
 public:
  // Construct a data source for the specified |source_name|.
  AboutUIHTMLSource(const std::string& source_name, Profile* profile);

  // content::URLDataSource implementation.
  virtual std::string GetSource() OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      bool is_incognito,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
  virtual std::string GetMimeType(const std::string& path) const OVERRIDE;
  virtual bool ShouldAddContentSecurityPolicy() const OVERRIDE;
  virtual bool ShouldDenyXFrameOptions() const OVERRIDE;

  // Send the response data.
  void FinishDataRequest(
      const std::string& html,
      const content::URLDataSource::GotDataCallback& callback);

  Profile* profile() { return profile_; }

 private:
  virtual ~AboutUIHTMLSource();

  std::string source_name_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AboutUIHTMLSource);
};

class AboutUI : public content::WebUIController {
 public:
  explicit AboutUI(content::WebUI* web_ui, const std::string& host);
  virtual ~AboutUI() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AboutUI);
};

namespace about_ui {

// Helper functions
void AppendHeader(std::string* output, int refresh,
                  const std::string& unescaped_title);
void AppendBody(std::string *output);
void AppendFooter(std::string *output);

}  // namespace about_ui

#endif  // CHROME_BROWSER_UI_WEBUI_ABOUT_UI_H_
