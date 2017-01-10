// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/notification_service_sessions_router.h"

#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/sync/browser_synced_window_delegates_getter.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "chrome/common/features.h"
#include "components/history/core/browser/history_service.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/features/features.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/tab_helper.h"
#endif

using content::NavigationController;
using content::WebContents;

namespace sync_sessions {

namespace {

SyncedTabDelegate* GetSyncedTabDelegateFromWebContents(
    content::WebContents* web_contents) {
#if defined(OS_ANDROID)
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  return tab ? tab->GetSyncedTabDelegate() : nullptr;
#else
  SyncedTabDelegate* delegate =
      TabContentsSyncedTabDelegate::FromWebContents(web_contents);
  return delegate;
#endif
}

}  // namespace

NotificationServiceSessionsRouter::NotificationServiceSessionsRouter(
    Profile* profile,
    SyncSessionsClient* sessions_client,
    const syncer::SyncableService::StartSyncFlare& flare)
    : handler_(nullptr),
      profile_(profile),
      sessions_client_(sessions_client),
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
  registrar_.Add(this,
      chrome::NOTIFICATION_TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED,
      content::NotificationService::AllSources());
#endif
  registrar_.Add(this,
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllBrowserContextsAndSources());
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (history_service) {
    favicon_changed_subscription_ = history_service->AddFaviconsChangedCallback(
        base::Bind(&NotificationServiceSessionsRouter::OnFaviconsChanged,
                   base::Unretained(this)));
  }
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
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
    // Source<WebContents>.
    case chrome::NOTIFICATION_TAB_PARENTED:
    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME:
    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED: {
      WebContents* web_contents = content::Source<WebContents>(source).ptr();
      if (Profile::FromBrowserContext(web_contents->GetBrowserContext()) !=
          profile_)
        return;
      SyncedTabDelegate* tab =
          GetSyncedTabDelegateFromWebContents(web_contents);
      if (!tab)
        return;
      if (handler_)
        handler_->OnLocalTabModified(tab);
      if (!tab->ShouldSync(sessions_client_))
        return;
      break;
    }
    // Source<NavigationController>.
    case content::NOTIFICATION_NAV_LIST_PRUNED:
    case content::NOTIFICATION_NAV_ENTRY_CHANGED:
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      WebContents* web_contents =
          content::Source<NavigationController>(source).ptr()->GetWebContents();
      if (Profile::FromBrowserContext(web_contents->GetBrowserContext()) !=
          profile_)
        return;
      SyncedTabDelegate* tab =
          GetSyncedTabDelegateFromWebContents(web_contents);
      if (!tab)
        return;
      if (handler_)
        handler_->OnLocalTabModified(tab);
      if (!tab->ShouldSync(sessions_client_))
        return;
      break;
    }
#if BUILDFLAG(ENABLE_EXTENSIONS)
    case chrome::NOTIFICATION_TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED: {
      extensions::TabHelper* extension_tab_helper =
          content::Source<extensions::TabHelper>(source).ptr();
      if (Profile::FromBrowserContext(
              extension_tab_helper->web_contents()->GetBrowserContext()) !=
          profile_) {
        return;
      }
      if (extension_tab_helper->extension_app()) {
        SyncedTabDelegate* tab = GetSyncedTabDelegateFromWebContents(
            extension_tab_helper->web_contents());
        if (!tab)
          return;
        if (handler_)
          handler_->OnLocalTabModified(tab);
        if (!tab->ShouldSync(sessions_client_))
          return;
        break;
      }
      return;
    }
#endif
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
  DCHECK_EQ(profile_,
            Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  SyncedTabDelegate* tab = GetSyncedTabDelegateFromWebContents(web_contents);
  if (!tab || !handler_)
    return;

  handler_->OnLocalTabModified(tab);
}

void NotificationServiceSessionsRouter::OnFaviconsChanged(
    const std::set<GURL>& page_urls,
    const GURL& icon_url) {
  if (handler_)
    handler_->OnFaviconsChanged(page_urls, icon_url);
}

void NotificationServiceSessionsRouter::StartRoutingTo(
    LocalSessionEventHandler* handler) {
  DCHECK(!handler_);
  handler_ = handler;
}

void NotificationServiceSessionsRouter::Stop() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  handler_ = nullptr;
}

}  // namespace sync_sessions
