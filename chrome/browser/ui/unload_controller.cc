// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/unload_controller.h"

#include <set>

#include "base/callback.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

namespace chrome {

typedef base::Callback<void(void)> DetachedTabsClosedCallback;

////////////////////////////////////////////////////////////////////////////////
// UnloadDetachedHandler is used to close tabs quickly, http://crbug.com/142458.
//   - Allows unload handlers to run in the background.
//   - Comes into play after the beforeunload handlers (if any) have run.
//   - Does not close the tabs; it holds tabs while they are closed.
class UnloadDetachedHandler : public content::WebContentsDelegate {
 public:
  explicit UnloadDetachedHandler(const DetachedTabsClosedCallback& callback)
      : tabs_closed_callback_(callback) { }
  virtual ~UnloadDetachedHandler() { }

  // Returns true if it succeeds.
  bool DetachWebContents(TabStripModel* tab_strip_model,
                         content::WebContents* web_contents) {
    int index = tab_strip_model->GetIndexOfWebContents(web_contents);
    if (index != TabStripModel::kNoTab &&
        web_contents->NeedToFireBeforeUnload()) {
      tab_strip_model->DetachWebContentsAt(index);
      detached_web_contents_.insert(web_contents);
      web_contents->SetDelegate(this);
      web_contents->OnUnloadDetachedStarted();
      return true;
    }
    return false;
  }

  bool HasTabs() const {
    return !detached_web_contents_.empty();
  }

 private:
  // WebContentsDelegate implementation.
  virtual bool ShouldSuppressDialogs() OVERRIDE {
    return true;  // Return true so dialogs are suppressed.
  }
  virtual void CloseContents(content::WebContents* source) OVERRIDE {
    detached_web_contents_.erase(source);
    delete source;
    if (detached_web_contents_.empty())
      tabs_closed_callback_.Run();
  }

  const DetachedTabsClosedCallback tabs_closed_callback_;
  std::set<content::WebContents*> detached_web_contents_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(UnloadDetachedHandler);
};


////////////////////////////////////////////////////////////////////////////////
// UnloadController, public:

UnloadController::UnloadController(Browser* browser)
    : browser_(browser),
      is_attempting_to_close_browser_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          unload_detached_handler_(new UnloadDetachedHandler(
              base::Bind(&UnloadController::ProcessPendingTabs,
                         base::Unretained(this))))),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  browser_->tab_strip_model()->AddObserver(this);
}

UnloadController::~UnloadController() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

bool UnloadController::CanCloseContents(content::WebContents* contents) {
  // Don't try to close the tab when the whole browser is being closed, since
  // that avoids the fast shutdown path where we just kill all the renderers.
  if (is_attempting_to_close_browser_)
    ClearUnloadState(contents, true);
  return !is_attempting_to_close_browser_;
}

bool UnloadController::BeforeUnloadFired(content::WebContents* contents,
                                         bool proceed) {
  if (!is_attempting_to_close_browser_) {
    if (!proceed) {
      contents->SetClosedByUserGesture(false);
    } else {
      // No more dialogs are possible, so remove the tab and finish
      // running unload listeners asynchrounously.
      TabStripModel* model = browser_->tab_strip_model();
      model->delegate()->CreateHistoricalTab(contents);
      unload_detached_handler_->DetachWebContents(model, contents);
    }
    return proceed;
  }

  if (!proceed) {
    CancelWindowClose();
    contents->SetClosedByUserGesture(false);
    return false;
  }

  if (RemoveFromSet(&tabs_needing_before_unload_fired_, contents)) {
    // Now that beforeunload has fired, queue the tab to fire unload.
    tabs_needing_unload_fired_.insert(contents);
    ProcessPendingTabs();
    // We want to handle firing the unload event ourselves since we want to
    // fire all the beforeunload events before attempting to fire the unload
    // events should the user cancel closing the browser.
    return false;
  }

  return true;
}

bool UnloadController::ShouldCloseWindow() {
  if (HasCompletedUnloadProcessing())
    return true;

  is_attempting_to_close_browser_ = true;

  if (!TabsNeedBeforeUnloadFired())
    return true;

  ProcessPendingTabs();
  return false;
}

bool UnloadController::TabsNeedBeforeUnloadFired() {
  if (tabs_needing_before_unload_fired_.empty()) {
    for (int i = 0; i < browser_->tab_strip_model()->count(); ++i) {
      content::WebContents* contents =
          browser_->tab_strip_model()->GetWebContentsAt(i);
      if (contents->NeedToFireBeforeUnload())
        tabs_needing_before_unload_fired_.insert(contents);
    }
  }
  return !tabs_needing_before_unload_fired_.empty();
}

////////////////////////////////////////////////////////////////////////////////
// UnloadController, content::NotificationObserver implementation:

void UnloadController::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED:
      if (is_attempting_to_close_browser_) {
        ClearUnloadState(content::Source<content::WebContents>(source).ptr(),
                         false);  // See comment for ClearUnloadState().
      }
      break;
    default:
      NOTREACHED() << "Got a notification we didn't register for.";
  }
}

////////////////////////////////////////////////////////////////////////////////
// UnloadController, TabStripModelObserver implementation:

void UnloadController::TabInsertedAt(content::WebContents* contents,
                                     int index,
                                     bool foreground) {
  TabAttachedImpl(contents);
}

void UnloadController::TabDetachedAt(content::WebContents* contents,
                                     int index) {
  TabDetachedImpl(contents);
}

void UnloadController::TabReplacedAt(TabStripModel* tab_strip_model,
                                     content::WebContents* old_contents,
                                     content::WebContents* new_contents,
                                     int index) {
  TabDetachedImpl(old_contents);
  TabAttachedImpl(new_contents);
}

void UnloadController::TabStripEmpty() {
  // Set is_attempting_to_close_browser_ here, so that extensions, etc, do not
  // attempt to add tabs to the browser before it closes.
  is_attempting_to_close_browser_ = true;
}

////////////////////////////////////////////////////////////////////////////////
// UnloadController, private:

void UnloadController::TabAttachedImpl(content::WebContents* contents) {
  // If the tab crashes in the beforeunload or unload handler, it won't be
  // able to ack. But we know we can close it.
  registrar_.Add(
      this,
      content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
      content::Source<content::WebContents>(contents));
}

void UnloadController::TabDetachedImpl(content::WebContents* contents) {
  if (is_attempting_to_close_browser_) {
    if (RemoveFromSet(&tabs_needing_before_unload_fired_, contents) &&
        tabs_needing_before_unload_fired_.empty() &&
        tabs_needing_unload_fired_.empty()) {
      // The last tab needing beforeunload crashed.
      // Continue with the close (ProcessPendingTabs would miss this).
      browser_->OnWindowClosing();
      if (browser_->tab_strip_model()->empty()) {
        browser_->TabStripEmpty();
      } else {
        browser_->tab_strip_model()->CloseAllTabs();
      }
    }

    ClearUnloadState(contents, false);
  }
  registrar_.Remove(this,
                    content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                    content::Source<content::WebContents>(contents));
}

void UnloadController::ProcessPendingTabs() {
  if (HasCompletedUnloadProcessing()) {
    browser_->OnWindowClosing();
    browser_->OnUnloadProcessingCompleted();
  } else if (!tabs_needing_before_unload_fired_.empty()) {
    // Process all the beforeunload handlers before the unload handlers.
    content::WebContents* web_contents =
        *(tabs_needing_before_unload_fired_.begin());
    // Null check render_view_host here as this gets called on a PostTask and
    // the tab's render_view_host may have been nulled out.
    if (web_contents->GetRenderViewHost()) {
      web_contents->OnCloseStarted();
      web_contents->GetRenderViewHost()->FirePageBeforeUnload(false);
    } else {
      ClearUnloadState(web_contents, true);
    }
  } else if (!tabs_needing_unload_fired_.empty()) {
    // All beforeunload handlers have fired. Proceed with unload handlers.

    browser_->OnWindowClosing();

    // Copy unload tabs to avoid iterator issues when detaching tabs.
    UnloadListenerSet unload_tabs = tabs_needing_unload_fired_;

    // Run unload handlers detached since no more interaction is possible.
    for (UnloadListenerSet::iterator it = unload_tabs.begin();
         it != unload_tabs.end(); it++) {
      content::WebContents* web_contents = *it;
      // Null check render_view_host here as this gets called on a PostTask
      // and the tab's render_view_host may have been nulled out.
      if (web_contents->GetRenderViewHost()) {
        web_contents->OnUnloadStarted();
        unload_detached_handler_->DetachWebContents(
            browser_->tab_strip_model(), web_contents);
        web_contents->GetRenderViewHost()->ClosePage();
      }
    }
    tabs_needing_unload_fired_.clear();
    if (browser_->tab_strip_model()->empty()) {
      browser_->TabStripEmpty();
    } else {
      browser_->tab_strip_model()->CloseAllTabs();
    }
  }
}

bool UnloadController::HasCompletedUnloadProcessing() const {
  return is_attempting_to_close_browser_ &&
      tabs_needing_before_unload_fired_.empty() &&
      tabs_needing_unload_fired_.empty() &&
      !unload_detached_handler_->HasTabs();
}

void UnloadController::CancelWindowClose() {
  // Closing of window can be canceled from a beforeunload handler.
  DCHECK(is_attempting_to_close_browser_);
  for (UnloadListenerSet::iterator it = tabs_needing_unload_fired_.begin();
       it != tabs_needing_unload_fired_.end(); it++) {
    content::WebContents* web_contents = *it;
    web_contents->OnCloseCanceled();
  }
  tabs_needing_before_unload_fired_.clear();
  tabs_needing_unload_fired_.clear();
  is_attempting_to_close_browser_ = false;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
      content::Source<Browser>(browser_),
      content::NotificationService::NoDetails());
}

bool UnloadController::RemoveFromSet(UnloadListenerSet* set,
                                     content::WebContents* web_contents) {
  DCHECK(is_attempting_to_close_browser_);

  UnloadListenerSet::iterator iter =
      std::find(set->begin(), set->end(), web_contents);
  if (iter != set->end()) {
    set->erase(iter);
    return true;
  }
  return false;
}

void UnloadController::ClearUnloadState(content::WebContents* web_contents,
                                        bool process_now) {
  if (is_attempting_to_close_browser_) {
    RemoveFromSet(&tabs_needing_before_unload_fired_, web_contents);
    RemoveFromSet(&tabs_needing_unload_fired_, web_contents);
    if (process_now) {
      ProcessPendingTabs();
    } else {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&UnloadController::ProcessPendingTabs,
                     weak_factory_.GetWeakPtr()));
    }
  }
}

}  // namespace chrome
