// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_HTML_DIALOG_TAB_CONTENTS_DELEGATE_H_
#define CHROME_BROWSER_WEBUI_HTML_DIALOG_TAB_CONTENTS_DELEGATE_H_
#pragma once

#include "chrome/browser/tab_contents/tab_contents_delegate.h"

class Browser;
class Profile;

// This class implements (and mostly ignores) most of TabContentsDelegate for
// use in an HTML dialog.  Subclasses need only override a few methods instead
// of the everything from TabContentsDelegate; this way, implementations on
// all platforms behave consistently.

class HtmlDialogTabContentsDelegate : public TabContentsDelegate {
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

  // TabContentsDelegate declarations.  Subclasses of this still need to
  // override:
  //   virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  //   virtual void ToolbarSizeChanged(TabContents* source, bool is_animating);

  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags);
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);
  virtual void ActivateContents(TabContents* contents);
  virtual void DeactivateContents(TabContents* contents);
  virtual void LoadingStateChanged(TabContents* source);
  virtual void CloseContents(TabContents* source);
  virtual bool IsPopup(const TabContents* source) const;
  virtual void UpdateTargetURL(TabContents* source, const GURL& url);
  virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      NavigationType::Type navigation_type);

 private:
  Profile* profile_;  // Weak pointer.  Always an original profile.
};

#endif  // CHROME_BROWSER_WEBUI_HTML_DIALOG_TAB_CONTENTS_DELEGATE_H_
