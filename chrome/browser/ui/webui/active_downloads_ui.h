// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACTIVE_DOWNLOADS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ACTIVE_DOWNLOADS_UI_H_
#pragma once

#include <string>

#include "chrome/browser/history/history.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "net/base/directory_lister.h"

class Browser;
class Profile;
class DownloadItem;
class ActiveDownloadsHandler;

class ActiveDownloadsUI : public HtmlDialogUI {
 public:
  explicit ActiveDownloadsUI(TabContents* contents);

  static Browser* OpenPopup(Profile* profile, DownloadItem* download_item);
  static Browser* GetPopupForPath(const std::string& path,
                                  Profile* profile);

 private:
  ActiveDownloadsHandler* handler_;
  // TODO(achuith): Fix this.
  static DownloadItem* first_download_item_;
  DISALLOW_COPY_AND_ASSIGN(ActiveDownloadsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_ACTIVE_DOWNLOADS_UI_H_
