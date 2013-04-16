// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/background_contents.h"

#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "ui/gfx/rect.h"

using content::SiteInstance;
using content::WebContents;

BackgroundContents::BackgroundContents(SiteInstance* site_instance,
                                       int routing_id,
                                       Delegate* delegate)
    : delegate_(delegate) {
  profile_ = Profile::FromBrowserContext(
      site_instance->GetBrowserContext());

  // TODO(rafaelw): Implement correct session storage.
  WebContents::CreateParams create_params(profile_, site_instance);
  create_params.routing_id = routing_id;
  web_contents_.reset(WebContents::Create(create_params));
  extensions::SetViewType(
      web_contents_.get(), extensions::VIEW_TYPE_BACKGROUND_CONTENTS);
  web_contents_->SetDelegate(this);
  content::WebContentsObserver::Observe(web_contents_.get());

  // Close ourselves when the application is shutting down.
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
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
  if (!web_contents_.get())   // Will be null for unit tests.
    return;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED,
      content::Source<Profile>(profile_),
      content::Details<BackgroundContents>(this));
}

const GURL& BackgroundContents::GetURL() const {
  return web_contents_.get() ? web_contents_->GetURL() : GURL::EmptyGURL();
}

void BackgroundContents::CloseContents(WebContents* source) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BACKGROUND_CONTENTS_CLOSED,
      content::Source<Profile>(profile_),
      content::Details<BackgroundContents>(this));
  delete this;
}

bool BackgroundContents::ShouldSuppressDialogs() {
  return true;
}

void BackgroundContents::DidNavigateMainFramePostCommit(WebContents* tab) {
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

// Forward requests to add a new WebContents to our delegate.
void BackgroundContents::AddNewContents(WebContents* source,
                                        WebContents* new_contents,
                                        WindowOpenDisposition disposition,
                                        const gfx::Rect& initial_pos,
                                        bool user_gesture,
                                        bool* was_blocked) {
  delegate_->AddWebContents(
      new_contents, disposition, initial_pos, user_gesture, was_blocked);
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
    case chrome::NOTIFICATION_APP_TERMINATING: {
      delete this;
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification sent.";
      break;
  }
}
