// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACTIVE_DOWNLOADS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ACTIVE_DOWNLOADS_UI_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/browser/history/history.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "net/base/directory_lister.h"

class ActiveDownloadsHandler;
class Browser;
class DownloadItem;
class Profile;

class ActiveDownloadsUI : public HtmlDialogUI {
 public:
  explicit ActiveDownloadsUI(TabContents* contents);

  static Browser* OpenPopup(Profile* profile);
  static Browser* GetPopup();

  // For testing.
  typedef std::vector<DownloadItem*> DownloadList;
  const DownloadList& GetDownloads() const;

 private:
  ActiveDownloadsHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(ActiveDownloadsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_ACTIVE_DOWNLOADS_UI_H_
