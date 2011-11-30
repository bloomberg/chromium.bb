// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ABOUT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ABOUT_UI_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"

class Profile;
class TabContents;

// We expose this class because the OOBE flow may need to explicitly add the
// chrome://terms source outside of the normal flow.
class AboutUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  // Construct a data source for the specified |source_name|.
  AboutUIHTMLSource(const std::string& source_name, Profile* profile);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id) OVERRIDE;

  virtual std::string GetMimeType(const std::string& path) const OVERRIDE;

  // Send the response data.
  void FinishDataRequest(const std::string& html, int request_id);

  Profile* profile() { return profile_; }

 private:
  virtual ~AboutUIHTMLSource();

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AboutUIHTMLSource);
};

class AboutUI : public ChromeWebUI {
 public:
  explicit AboutUI(TabContents* contents, const std::string& host);
  virtual ~AboutUI() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AboutUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_ABOUT_UI_H_
