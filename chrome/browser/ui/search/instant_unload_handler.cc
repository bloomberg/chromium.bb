// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_unload_handler.h"

#include <algorithm>

#include "base/message_loop.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

class InstantUnloadHandler::WebContentsDelegateImpl
    : public content::WebContentsDelegate {
 public:
  WebContentsDelegateImpl(InstantUnloadHandler* handler,
                          scoped_ptr<content::WebContents> contents,
                          int index)
      : handler_(handler),
        contents_(contents.Pass()),
        index_(index) {
    contents_->SetDelegate(this);
    contents_->GetRenderViewHost()->FirePageBeforeUnload(false);
  }

  // Overridden from content::WebContentsDelegate:
  virtual void CloseContents(content::WebContents* source) OVERRIDE {
    DCHECK_EQ(contents_, source);
    // Remove ourselves as the delegate, so that CloseContents() won't be
    // called twice, leading to double deletion (http://crbug.com/155848).
    contents_->SetDelegate(NULL);
    handler_->Destroy(this);
  }

  virtual void WillRunBeforeUnloadConfirm() OVERRIDE {
    contents_->SetDelegate(NULL);
    handler_->Activate(this, contents_.Pass(), index_);
  }

  virtual bool ShouldSuppressDialogs() OVERRIDE {
    return true;
  }

 private:
  InstantUnloadHandler* const handler_;
  scoped_ptr<content::WebContents> contents_;

  // The tab strip index |contents_| was originally at. If we add the tab back
  // to the tabstrip, we add it at this index.
  const int index_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDelegateImpl);
};

InstantUnloadHandler::InstantUnloadHandler(Browser* browser)
    : browser_(browser) {
}

InstantUnloadHandler::~InstantUnloadHandler() {
}

void InstantUnloadHandler::RunUnloadListenersOrDestroy(
    scoped_ptr<content::WebContents> contents,
    int index) {
  DCHECK(!contents->GetDelegate());

  if (!contents->NeedToFireBeforeUnload()) {
    // Tab doesn't have any beforeunload listeners and can be safely deleted.
    // However, the tab object should not be deleted immediately because when we
    // get here from BrowserInstantController::TabDeactivated, other tab
    // observers may still expect to interact with the tab before the event has
    // finished propagating.
    MessageLoop::current()->DeleteSoon(FROM_HERE, contents.release());
    return;
  }

  // Tab has beforeunload listeners. Install a delegate to run them.
  delegates_.push_back(
      new WebContentsDelegateImpl(this, contents.Pass(), index));
}

void InstantUnloadHandler::Activate(WebContentsDelegateImpl* delegate,
                                    scoped_ptr<content::WebContents> contents,
                                    int index) {
  // Remove (and delete) the delegate.
  Destroy(delegate);

  // Add the tab back in.
  chrome::NavigateParams params(browser_, contents.release());
  params.disposition = NEW_FOREGROUND_TAB;
  params.tabstrip_index = index;
  chrome::Navigate(&params);
}

void InstantUnloadHandler::Destroy(WebContentsDelegateImpl* delegate) {
  ScopedVector<WebContentsDelegateImpl>::iterator i =
      std::find(delegates_.begin(), delegates_.end(), delegate);
  DCHECK(i != delegates_.end());

  // The delegate's method is a caller on the stack, so schedule the deletion
  // for later.
  delegates_.weak_erase(i);
  MessageLoop::current()->DeleteSoon(FROM_HERE, delegate);
}
