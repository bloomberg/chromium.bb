// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_LAUNCHER_PAGE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_APP_LAUNCHER_PAGE_UI_H_

#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

class Profile;

namespace base {
class RefCountedMemory;
}

// The WebUIController used for the app launcher page UI.
class AppLauncherPageUI : public content::WebUIController {
 public:
  explicit AppLauncherPageUI(content::WebUI* web_ui);
  virtual ~AppLauncherPageUI();

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);

 private:
  class HTMLSource : public content::URLDataSource {
   public:
    explicit HTMLSource(Profile* profile);
    virtual ~HTMLSource();

    // content::URLDataSource implementation.
    virtual std::string GetSource() OVERRIDE;
    virtual void StartDataRequest(
        const std::string& path,
        bool is_incognito,
        const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
    virtual std::string GetMimeType(const std::string&) const OVERRIDE;
    virtual bool ShouldReplaceExistingSource() const OVERRIDE;
    virtual bool ShouldAddContentSecurityPolicy() const OVERRIDE;

   private:

    // Pointer back to the original profile.
    Profile* profile_;

    DISALLOW_COPY_AND_ASSIGN(HTMLSource);
  };

  Profile* GetProfile() const;

  DISALLOW_COPY_AND_ASSIGN(AppLauncherPageUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_LAUNCHER_PAGE_UI_H_
