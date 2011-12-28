// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HTML_DIALOG_TAB_CONTENTS_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_HTML_DIALOG_TAB_CONTENTS_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/public/browser/web_contents_delegate.h"

class Profile;

// This class implements (and mostly ignores) most of
// content::WebContentsDelegate for use in an HTML dialog. Subclasses need only
// override a few methods instead of the everything from
// content::WebContentsDelegate; this way, implementations on all platforms
// behave consistently.
class HtmlDialogTabContentsDelegate : public content::WebContentsDelegate {
 public:
  // Profile must be non-NULL.
  explicit HtmlDialogTabContentsDelegate(Profile* profile);

  virtual ~HtmlDialogTabContentsDelegate();

  // The returned profile is guaranteed to be original if non-NULL.
  Profile* profile() const;

  // Calling this causes all following events sent from the
  // TabContents object to be ignored.  It also makes all following
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
};

#endif  // CHROME_BROWSER_UI_WEBUI_HTML_DIALOG_TAB_CONTENTS_DELEGATE_H_
