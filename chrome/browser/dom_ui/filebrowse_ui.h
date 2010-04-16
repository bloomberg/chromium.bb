// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_FILEBROWSE_UI_H_
#define CHROME_BROWSER_DOM_UI_FILEBROWSE_UI_H_

#include <string>

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/history/history.h"
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
  static Browser* GetPopupForPath(const std::string& path);

 private:
  DISALLOW_COPY_AND_ASSIGN(FileBrowseUI);
};

#endif  // CHROME_BROWSER_DOM_UI_FILEBROWSE_UI_H_
