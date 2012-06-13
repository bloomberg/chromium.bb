// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEB_DIALOG_WEB_CONTENTS_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_WEB_DIALOG_WEB_CONTENTS_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/public/browser/web_contents_delegate.h"

class Browser;
class Profile;

// This class implements (and mostly ignores) most of
// content::WebContentsDelegate for use in a Web dialog. Subclasses need only
// override a few methods instead of the everything from
// content::WebContentsDelegate; this way, implementations on all platforms
// behave consistently.
class WebDialogWebContentsDelegate : public content::WebContentsDelegate {
 public:
  // Opens a new URL inside |source| (if source is NULL open in the current
  // front-most tab). |profile| is the profile that the browser should be owened
  // by. |params| contains the URL to open and various attributes such as
  // disposition. On return |out_new_contents| contains the WebContents the
  // URL is opened in. Returns the browser spawned by the operation.
  static Browser* StaticOpenURLFromTab(Profile* profile,
                                       content::WebContents* source,
                                       const content::OpenURLParams& params,
                                       content::WebContents** out_new_contents);

  // Creates a new tab with |new_contents|. |profile| is the profile that the
  // browser should be owned by. |source| is the WebContent where the operation
  // originated. |disposition| controls how the new tab should be opened.
  // |initial_pos| is the position of the window if a new window is created.
  // |user_gesture| is true if the operation was started by a user gesture.
  // Returns the browser spawned by the operation.
  static Browser* StaticAddNewContents(Profile* profile,
                                       content::WebContents* source,
                                       content::WebContents* new_contents,
                                       WindowOpenDisposition disposition,
                                       const gfx::Rect& initial_pos,
                                       bool user_gesture);

  // Profile must be non-NULL.
  explicit WebDialogWebContentsDelegate(content::BrowserContext* context);

  virtual ~WebDialogWebContentsDelegate();

  // The returned profile is guaranteed to be original if non-NULL.
  Profile* profile() const;

  // Calling this causes all following events sent from the
  // WebContents object to be ignored.  It also makes all following
  // calls to profile() return NULL.
  void Detach();

  // content::WebContentsDelegate declarations.
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;

  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) OVERRIDE;
  virtual bool IsPopupOrPanel(
      const content::WebContents* source) const OVERRIDE;
  virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      content::NavigationType navigation_type) OVERRIDE;

 private:
  Profile* profile_;  // Weak pointer.  Always an original profile.

  DISALLOW_COPY_AND_ASSIGN(WebDialogWebContentsDelegate);
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEB_DIALOG_WEB_CONTENTS_DELEGATE_H_
