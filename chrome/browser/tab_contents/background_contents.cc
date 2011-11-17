// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/background_contents.h"

#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ui/webui/chrome_web_ui_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/chrome_view_types.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "ui/gfx/rect.h"

////////////////
// BackgroundContents

BackgroundContents::BackgroundContents(SiteInstance* site_instance,
                                       int routing_id,
                                       Delegate* delegate)
    : delegate_(delegate) {
  profile_ = Profile::FromBrowserContext(
      site_instance->browsing_instance()->browser_context());

  // TODO(rafaelw): Implement correct session storage.
  tab_contents_.reset(new TabContents(
      profile_, site_instance, routing_id, NULL, NULL));
  tab_contents_->set_view_type(chrome::VIEW_TYPE_BACKGROUND_CONTENTS);
  tab_contents_->set_delegate(this);
  TabContentsObserver::Observe(tab_contents_.get());

  // Close ourselves when the application is shutting down.
  registrar_.Add(this, content::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());

  // Register for our parent profile to shutdown, so we can shut ourselves down
  // as well (should only be called for OTR profiles, as we should receive
  // APP_TERMINATING before non-OTR profiles are destroyed).
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::Source<Profile>(profile_));
}

// Exposed to allow creating mocks.
BackgroundContents::BackgroundContents()
    : delegate_(NULL),
      profile_(NULL) {
}

BackgroundContents::~BackgroundContents() {
  if (!tab_contents_.get())   // Will be null for unit tests.
    return;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED,
      content::Source<Profile>(profile_),
      content::Details<BackgroundContents>(this));
}

const GURL& BackgroundContents::GetURL() const {
  return tab_contents_.get() ? tab_contents_->GetURL() : GURL::EmptyGURL();
}

void BackgroundContents::CloseContents(TabContents* source) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BACKGROUND_CONTENTS_CLOSED,
      content::Source<Profile>(profile_),
      content::Details<BackgroundContents>(this));
  delete this;
}

bool BackgroundContents::ShouldSuppressDialogs() {
  return true;
}

void BackgroundContents::DidNavigateMainFramePostCommit(TabContents* tab) {
  // Note: because BackgroundContents are only available to extension apps,
  // navigation is limited to urls within the app's extent. This is enforced in
  // RenderView::decidePolicyForNavigation. If BackgroundContents become
  // available as a part of the web platform, it probably makes sense to have
  // some way to scope navigation of a background page to its opener's security
  // origin. Note: if the first navigation is to a URL outside the app's
  // extent a background page will be opened but will remain at about:blank.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED,
      content::Source<Profile>(profile_),
      content::Details<BackgroundContents>(this));
}

void BackgroundContents::RenderViewGone(base::TerminationStatus status) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BACKGROUND_CONTENTS_TERMINATED,
      content::Source<Profile>(profile_),
      content::Details<BackgroundContents>(this));

  // Our RenderView went away, so we should go away also, so killing the process
  // via the TaskManager doesn't permanently leave a BackgroundContents hanging
  // around the system, blocking future instances from being created
  // <http://crbug.com/65189>.
  delete this;
}

void BackgroundContents::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  // TODO(rafaelw): Implement pagegroup ref-counting so that non-persistent
  // background pages are closed when the last referencing frame is closed.
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_DESTROYED:
    case content::NOTIFICATION_APP_TERMINATING: {
      delete this;
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification sent.";
      break;
  }
}
