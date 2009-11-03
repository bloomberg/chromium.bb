// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_observer.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string16.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "grit/generated_resources.h"

class BlockedContentNotice : public Task {
 public:
  BlockedContentNotice(const GURL& gurl,
                       const Blacklist::Match* match,
                       const ResourceDispatcherHostRequestInfo* info)
      : gurl_(gurl),
        match_(match),
        child_id_(info->child_id()),
        route_id_(info->route_id()) {
    if (match_->attributes() & Blacklist::kDontStoreCookies) {
      // No cookies stored.
      reason_ = l10n_util::GetStringUTF16(IDS_BLACKLIST_BLOCKED_COOKIES);
    } else if (match_->attributes() & Blacklist::kDontSendCookies) {
      // No cookies sent.
      reason_ = l10n_util::GetStringUTF16(IDS_BLACKLIST_BLOCKED_COOKIES);
    } else if (match_->attributes() & Blacklist::kDontSendReferrer) {
      // No referrer sent.
      reason_ = l10n_util::GetStringUTF16(IDS_BLACKLIST_BLOCKED_REFERRER);
    }
  }

  virtual void Run() {
    RenderViewHost* view = RenderViewHost::FromID(child_id_, route_id_);
    if (!view)
      return;  // The view may be gone by the time we get here.

    view->delegate()->AddBlockedNotice(gurl_, reason_);
  }

 private:
  const GURL gurl_;
  const Blacklist::Match* match_;
  const int child_id_;
  const int route_id_;

  string16 reason_;
};

void BlacklistObserver::ContentBlocked(const URLRequest* request) {
  const URLRequest::UserData* d =
      request->GetUserData(&Blacklist::kRequestDataKey);
  const Blacklist::Match* match = static_cast<const Blacklist::Match*>(d);
  const ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);
  const GURL& gurl = request->url();

  // Notify the UI that something non-visual has been blocked.
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      new BlockedContentNotice(gurl, match, info));
}
