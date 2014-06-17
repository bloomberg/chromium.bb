// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/notification_service_sessions_router.h"

#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/favicon/favicon_changed_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/browser/sync/glue/synced_tab_delegate.h"
#include "chrome/browser/sync/sessions/sessions_util.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

using content::NavigationController;
using content::WebContents;

namespace browser_sync {

NotificationServiceSessionsRouter::NotificationServiceSessionsRouter(
    Profile* profile, const syncer::SyncableService::StartSyncFlare& flare)
        : handler_(NULL),
          profile_(profile),
          flare_(flare),
          weak_ptr_factory_(this) {

  registrar_.Add(this, chrome::NOTIFICATION_TAB_PARENTED,
      content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_NAV_LIST_PRUNED,
      content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_CHANGED,
      content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  registrar_.Add(this,
      chrome::NOTIFICATION_TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED,
      content::NotificationService::AllSources());
  registrar_.Add(this,
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_FAVICON_CHANGED,
      content::Source<Profile>(profile_));
#if defined(ENABLE_MANAGED_USERS)
  if (profile_->IsSupervised()) {
    SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(profile_);
    supervised_user_service->AddNavigationBlockedCallback(
        base::Bind(&NotificationServiceSessionsRouter::OnNavigationBlocked,
                   weak_ptr_factory_.GetWeakPtr()));
  }
#endif
}

NotificationServiceSessionsRouter::~NotificationServiceSessionsRouter() {}

void NotificationServiceSessionsRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_FAVICON_CHANGED: {
      content::Details<FaviconChangedDetails> favicon_details(details);
      if (handler_)
        handler_->OnFaviconPageUrlsUpdated(favicon_details->urls);
      return;
    }
    // Source<WebContents>.
    case chrome::NOTIFICATION_TAB_PARENTED:
    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME:
    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED: {
      WebContents* web_contents = content::Source<WebContents>(source).ptr();
      SyncedTabDelegate* tab =
          SyncedTabDelegate::ImplFromWebContents(web_contents);
      if (!tab || tab->profile() != profile_)
        return;
      if (handler_)
        handler_->OnLocalTabModified(tab);
      if (!sessions_util::ShouldSyncTab(*tab))
        return;
      break;
    }
    // Source<NavigationController>.
    case content::NOTIFICATION_NAV_LIST_PRUNED:
    case content::NOTIFICATION_NAV_ENTRY_CHANGED:
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      SyncedTabDelegate* tab = SyncedTabDelegate::ImplFromWebContents(
          content::Source<NavigationController>(source).ptr()->
              GetWebContents());
      if (!tab || tab->profile() != profile_)
        return;
      if (handler_)
        handler_->OnLocalTabModified(tab);
      if (!sessions_util::ShouldSyncTab(*tab))
        return;
      break;
    }
    case chrome::NOTIFICATION_TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED: {
      extensions::TabHelper* extension_tab_helper =
          content::Source<extensions::TabHelper>(source).ptr();
      if (extension_tab_helper->web_contents()->GetBrowserContext() !=
              profile_) {
        return;
      }
      if (extension_tab_helper->extension_app()) {
        SyncedTabDelegate* tab = SyncedTabDelegate::ImplFromWebContents(
            extension_tab_helper->web_contents());
        if (!tab || tab->profile() != profile_)
          return;
        if (handler_)
          handler_->OnLocalTabModified(tab);
        break;
      }
      return;
    }
    default:
      LOG(ERROR) << "Received unexpected notification of type " << type;
      return;
  }

  if (!flare_.is_null()) {
    flare_.Run(syncer::SESSIONS);
    flare_.Reset();
  }
}

void NotificationServiceSessionsRouter::OnNavigationBlocked(
    content::WebContents* web_contents) {
  SyncedTabDelegate* tab =
      SyncedTabDelegate::ImplFromWebContents(web_contents);
  if (!tab || !handler_)
    return;

  DCHECK(tab->profile() == profile_);
  handler_->OnLocalTabModified(tab);
}

void NotificationServiceSessionsRouter::StartRoutingTo(
    LocalSessionEventHandler* handler) {
  DCHECK(!handler_);
  handler_ = handler;
}

void NotificationServiceSessionsRouter::Stop() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  handler_ = NULL;
}

}  // namespace browser_sync
