// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_

#include <string>

#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "views/view.h"

class DelegatedDOMView;
class SiteInstance;
namespace chromeos {
class ScreenObserver;
}  // namespace chromeos

class AccountCreationView : public views::View,
                            public TabContentsDelegate {
 public:
  explicit AccountCreationView(chromeos::ScreenObserver* observer);
  virtual ~AccountCreationView();

  virtual void Init();
  virtual void UpdateLocalizedStrings();

  // views::View implementation:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // TabContentsDelegate implementation:
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) {}
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) {}
  virtual void ActivateContents(TabContents* contents) {}
  virtual void LoadingStateChanged(TabContents* source) {}
  virtual void CloseContents(TabContents* source) {}
  virtual bool IsPopup(TabContents* source) { return false; }
  virtual void URLStarredChanged(TabContents* source, bool starred) {}
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) {
    LOG(INFO) << "Target URL: " << url.spec().c_str() << "\n";
  }
  virtual bool ShouldAddNavigationToHistory() const { return false; }
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) {}

 private:
  // Tries to create an account with user input.
  void CreateAccount();

  // Notifications receiver.
  chromeos::ScreenObserver* observer_;

  DelegatedDOMView* dom_;
  SiteInstance* site_instance_;

  DISALLOW_COPY_AND_ASSIGN(AccountCreationView);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_

