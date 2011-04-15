// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_unload_handler.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"

// TabContentsDelegate implementation. This owns the TabContents supplied to the
// constructor.
class InstantUnloadHandler::TabContentsDelegateImpl
    : public TabContentsDelegate {
 public:
  TabContentsDelegateImpl(InstantUnloadHandler* handler,
                          TabContentsWrapper* tab_contents,
                          int index)
      : handler_(handler),
        tab_contents_(tab_contents),
        index_(index) {
    tab_contents->tab_contents()->set_delegate(this);
  }

  ~TabContentsDelegateImpl() {
  }

  // Releases ownership of the TabContentsWrapper to the caller.
  TabContentsWrapper* ReleaseTab() {
    TabContentsWrapper* tab = tab_contents_.release();
    tab->tab_contents()->set_delegate(NULL);
    return tab;
  }

  // See description above field.
  int index() const { return index_; }

  // TabContentsDelegate overrides:
  virtual void WillRunBeforeUnloadConfirm() {
    handler_->Activate(this);
  }

  virtual bool ShouldSuppressDialogs() {
    return true;  // Return true so dialogs are suppressed.
  }

  virtual void CloseContents(TabContents* source) {
    handler_->Destroy(this);
  }

  // All of the following are overriden to do nothing (they are pure
  // virtual). When we're attemping to close the tab, none of this matters.
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition) {}
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) {}
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) {}
  virtual void ActivateContents(TabContents* contents) {}
  virtual void DeactivateContents(TabContents* contents) {}
  virtual void LoadingStateChanged(TabContents* source) {}
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) {}

 private:
  InstantUnloadHandler* handler_;
  scoped_ptr<TabContentsWrapper> tab_contents_;

  // The index |tab_contents_| was originally at. If we add the tab back we add
  // it at this index.
  const int index_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsDelegateImpl);
};

InstantUnloadHandler::InstantUnloadHandler(Browser* browser)
    : browser_(browser) {
}

InstantUnloadHandler::~InstantUnloadHandler() {
}

void InstantUnloadHandler::RunUnloadListenersOrDestroy(TabContentsWrapper* tab,
                                                       int index) {
  if (!tab->tab_contents()->NeedToFireBeforeUnload()) {
    // Tab doesn't have any before unload listeners and can be safely deleted.
    delete tab;
    return;
  }

  // Tab has before unload listener. Install a delegate and fire the before
  // unload listener.
  TabContentsDelegateImpl* delegate =
      new TabContentsDelegateImpl(this, tab, index);
  delegates_.push_back(delegate);
  // TODO: decide if we really want false here. false is used for tab closes,
  // and is needed so that the tab correctly closes but it doesn't really match
  // what's logically happening.
  tab->tab_contents()->render_view_host()->FirePageBeforeUnload(false);
}

void InstantUnloadHandler::Activate(TabContentsDelegateImpl* delegate) {
  // Take ownership of the TabContents from the delegate.
  TabContentsWrapper* tab = delegate->ReleaseTab();
  browser::NavigateParams params(browser_, tab);
  params.disposition = NEW_FOREGROUND_TAB;
  params.tabstrip_index = delegate->index();

  // Remove (and delete) the delegate.
  ScopedVector<TabContentsDelegateImpl>::iterator i =
      std::find(delegates_.begin(), delegates_.end(), delegate);
  DCHECK(i != delegates_.end());
  delegates_.erase(i);
  delegate = NULL;

  // Add the tab back in.
  browser::Navigate(&params);
}

void InstantUnloadHandler::Destroy(TabContentsDelegateImpl* delegate) {
  ScopedVector<TabContentsDelegateImpl>::iterator i =
      std::find(delegates_.begin(), delegates_.end(), delegate);
  DCHECK(i != delegates_.end());
  delegates_.erase(i);
}
