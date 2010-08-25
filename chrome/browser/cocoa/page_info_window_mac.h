// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_PAGE_INFO_WINDOW_MAC_H_
#define CHROME_BROWSER_COCOA_PAGE_INFO_WINDOW_MAC_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/page_info_model.h"
#include "chrome/browser/page_info_window.h"

class Profile;
@class PageInfoWindowController;

namespace {
class PageInfoWindowMacTest;
};

// This bridge is responsible for getting information from the cross-platform
// model and dynamically creating the contents of the window. The controller is
// responsible for managing the window's memory and user events (pressing on
// the Show Certificate button).
class PageInfoWindowMac : public PageInfoModel::PageInfoModelObserver {
 public:
  virtual ~PageInfoWindowMac();

  // Used to create the page info window; called from the cross-platform
  // function.
  static void ShowPageInfo(gfx::NativeWindow parent,
                           Profile* profile,
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
  friend class ::PageInfoWindowMacTest;

  // Constructor; private.  Called by ShowPageInfo().
  PageInfoWindowMac(PageInfoWindowController* controller,
                    Profile* profile,
                    const GURL& url,
                    const NavigationEntry::SSLStatus& ssl,
                    bool show_history);

  // Testing constructor. DO NOT USE.
  PageInfoWindowMac(PageInfoWindowController* controller,
                    PageInfoModel* model);

  // Shared constructor initialization.
  void Init();

  // Dynamically creates the window's content section.
  void LayoutSections();

  // Shows the actual window.
  void Show();

  // The window controller that manages the memory and window reference.
  PageInfoWindowController* controller_;  // WEAK, owns us.

  // The platform-independent model for the info window.
  scoped_ptr<PageInfoModel> model_;

  // Reference to the good and bad images that are placed within the UI.
  scoped_nsobject<NSImage> good_image_;
  scoped_nsobject<NSImage> bad_image_;

  // The certificate ID for the page, 0 if the page is not over HTTPS.
  int cert_id_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoWindowMac);
};

#endif  // CHROME_BROWSER_COCOA_PAGE_INFO_WINDOW_MAC_H_
