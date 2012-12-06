// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_unload_handler.h"

#include <algorithm>

#include "base/message_loop.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

// WebContentsDelegate implementation. This owns the WebContents supplied to the
// constructor.
class InstantUnloadHandler::WebContentsDelegateImpl
    : public content::WebContentsDelegate {
 public:
  WebContentsDelegateImpl(InstantUnloadHandler* handler,
                          content::WebContents* contents,
                          int index)
      : handler_(handler),
        contents_(contents),
        index_(index) {
    contents->SetDelegate(this);
  }

  // content::WebContentsDelegate overrides:
  virtual void WillRunBeforeUnloadConfirm() OVERRIDE {
    content::WebContents* contents = contents_.release();
    contents->SetDelegate(NULL);
    handler_->Activate(this, contents, index_);
  }

  virtual bool ShouldSuppressDialogs() OVERRIDE {
    return true;  // Return true so dialogs are suppressed.
  }

  virtual void CloseContents(content::WebContents* source) OVERRIDE {
    contents_->SetDelegate(NULL);
    handler_->Destroy(this);
  }

 private:
  InstantUnloadHandler* const handler_;
  scoped_ptr<content::WebContents> contents_;

  // The index |contents_| was originally at. If we add the tab back we add it
  // at this index.
  const int index_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDelegateImpl);
};

InstantUnloadHandler::InstantUnloadHandler(Browser* browser)
    : browser_(browser) {
}

InstantUnloadHandler::~InstantUnloadHandler() {
}

void InstantUnloadHandler::RunUnloadListenersOrDestroy(
    content::WebContents* contents,
    int index) {
  if (!contents->NeedToFireBeforeUnload()) {
    // Tab doesn't have any beforeunload listeners and can be safely deleted.
    delete contents;
    return;
  }

  // Tab has before unload listener. Install a delegate and fire the before
  // unload listener.
  delegates_.push_back(new WebContentsDelegateImpl(this, contents, index));
  contents->GetRenderViewHost()->FirePageBeforeUnload(false);
}

void InstantUnloadHandler::Activate(WebContentsDelegateImpl* delegate,
                                    content::WebContents* contents,
                                    int index) {
  chrome::NavigateParams params(browser_, contents);
  params.disposition = NEW_FOREGROUND_TAB;
  params.tabstrip_index = index;

  // Remove (and delete) the delegate.
  Destroy(delegate);

  // Add the tab back in.
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
