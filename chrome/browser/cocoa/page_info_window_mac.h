// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_PAGE_INFO_WINDOW_MAC_H_
#define CHROME_BROWSER_COCOA_PAGE_INFO_WINDOW_MAC_H_

#include "chrome/browser/page_info_model.h"
#include "chrome/browser/page_info_window.h"

class Profile;
@class PageInfoWindowController;

class PageInfoWindowMac : public PageInfoModel::PageInfoModelObserver {
 public:
  virtual ~PageInfoWindowMac();

  // Creates and shows the page info.
  static void ShowPageInfo(Profile* profile,
                           const GURL& url,
                           const NavigationEntry::SSLStatus& ssl,
                           bool show_history);

  // Shows various information for the specified certificate in a new dialog.
  // The argument is ignored here and we use the |cert_id_| member that was
  // passed to us in Init().
  virtual void ShowCertDialog(int);

  // PageInfoModelObserver implementation.
  virtual void ModelChanged();

 private:
  PageInfoWindowMac(PageInfoWindowController* controller,
                    Profile* profile,
                    const GURL& url,
                    const NavigationEntry::SSLStatus& ssl,
                    bool show_history);

  void LayoutSections();

  void Show();

  PageInfoWindowController* controller_;  // WEAK, owns us.

  PageInfoModel model_;

  // The certificate ID for the page, 0 if the page is not over HTTPS.
  int cert_id_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoWindowMac);
};

#endif  // CHROME_BROWSER_COCOA_PAGE_INFO_WINDOW_MAC_H_
