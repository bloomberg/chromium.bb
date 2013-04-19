// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/unload_controller.h"

#include "base/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace chrome {

////////////////////////////////////////////////////////////////////////////////
// UnloadController, public:

UnloadController::UnloadController(Browser* browser)
    : browser_(browser),
      is_attempting_to_close_browser_(false),
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
    if (!proceed)
      contents->SetClosedByUserGesture(false);
    return proceed;
  }

  if (!proceed) {
    CancelWindowClose();
    contents->SetClosedByUserGesture(false);
    return false;
  }

  if (RemoveFromSet(&tabs_needing_before_unload_fired_, contents)) {
    // Now that beforeunload has fired, put the tab on the queue to fire
    // unload.
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
  if (is_attempting_to_close_browser_)
    ClearUnloadState(contents, false);
  registrar_.Remove(this,
                    content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                    content::Source<content::WebContents>(contents));
}

void UnloadController::ProcessPendingTabs() {
  if (!is_attempting_to_close_browser_) {
    // Because we might invoke this after a delay it's possible for the value of
    // is_attempting_to_close_browser_ to have changed since we scheduled the
    // task.
    return;
  }

  if (HasCompletedUnloadProcessing()) {
    // We've finished all the unload events and can proceed to close the
    // browser.
    browser_->OnWindowClosing();
    return;
  }

  // Process beforeunload tabs first. When that queue is empty, process
  // unload tabs.
  if (!tabs_needing_before_unload_fired_.empty()) {
    content::WebContents* web_contents =
        *(tabs_needing_before_unload_fired_.begin());
    // Null check render_view_host here as this gets called on a PostTask and
    // the tab's render_view_host may have been nulled out.
    if (web_contents->GetRenderViewHost()) {
      web_contents->GetRenderViewHost()->FirePageBeforeUnload(false);
    } else {
      ClearUnloadState(web_contents, true);
    }
  } else if (!tabs_needing_unload_fired_.empty()) {
    // We've finished firing all beforeunload events and can proceed with unload
    // events.
    // TODO(ojan): We should add a call to browser_shutdown::OnShutdownStarting
    // somewhere around here so that we have accurate measurements of shutdown
    // time.
    // TODO(ojan): We can probably fire all the unload events in parallel and
    // get a perf benefit from that in the cases where the tab hangs in it's
    // unload handler or takes a long time to page in.
    content::WebContents* web_contents = *(tabs_needing_unload_fired_.begin());
    // Null check render_view_host here as this gets called on a PostTask and
    // the tab's render_view_host may have been nulled out.
    if (web_contents->GetRenderViewHost()) {
      web_contents->GetRenderViewHost()->ClosePage();
    } else {
      ClearUnloadState(web_contents, true);
    }
  } else {
    NOTREACHED();
  }
}

bool UnloadController::HasCompletedUnloadProcessing() const {
  return is_attempting_to_close_browser_ &&
      tabs_needing_before_unload_fired_.empty() &&
      tabs_needing_unload_fired_.empty();
}

void UnloadController::CancelWindowClose() {
  // Closing of window can be canceled from a beforeunload handler.
  DCHECK(is_attempting_to_close_browser_);
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
