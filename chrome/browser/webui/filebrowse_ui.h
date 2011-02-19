// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_FILEBROWSE_UI_H_
#define CHROME_BROWSER_WEBUI_FILEBROWSE_UI_H_
#pragma once

#include <string>

#include "chrome/browser/history/history.h"
#include "chrome/browser/webui/chrome_url_data_manager.h"
#include "chrome/browser/webui/html_dialog_ui.h"
#include "net/base/directory_lister.h"

class Browser;
class Profile;

class FileBrowseUI : public HtmlDialogUI {
 public:
  static const int kPopupWidth;
  static const int kPopupHeight;
  static const int kSmallPopupWidth;
  static const int kSmallPopupHeight;

  explicit FileBrowseUI(TabContents* contents);

  static Browser* OpenPopup(Profile* profile,
                            const std::string& hashArgument,
                            int width,
                            int height);
  static Browser* GetPopupForPath(const std::string& path,
                                  Profile* profile);

 private:
  DISALLOW_COPY_AND_ASSIGN(FileBrowseUI);
};

#endif  // CHROME_BROWSER_WEBUI_FILEBROWSE_UI_H_
