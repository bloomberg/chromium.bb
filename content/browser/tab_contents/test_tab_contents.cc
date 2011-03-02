// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/test_tab_contents.h"

#include <utility>

#include "chrome/browser/browser_url_handler.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/page_transition_types.h"
#include "content/browser/renderer_host/mock_render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/infobar_delegate.h"

TestTabContents::TestTabContents(Profile* profile, SiteInstance* instance)
    : TabContents(profile, instance, MSG_ROUTING_NONE, NULL, NULL),
      transition_cross_site(false) {
  // Listen for infobar events so we can call InfoBarClosed() on the infobar
  // delegates and give them an opportunity to delete themselves.  (Since we
  // have no InfobarContainer in TestTabContents, InfoBarClosed() is not called
  // most likely leading to the infobar delegates being leaked.)
  Source<TabContents> source(this);
  registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_REMOVED,
                 source);
  registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_REPLACED,
                 source);
}

void TestTabContents::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  // TabContents does not handle TAB_CONTENTS_INFOBAR_* so we don't pass it
  // these notifications.
  switch (type.value) {
    case NotificationType::TAB_CONTENTS_INFOBAR_REMOVED:
      Details<InfoBarDelegate>(details).ptr()->InfoBarClosed();
      break;
    case NotificationType::TAB_CONTENTS_INFOBAR_REPLACED:
      Details<std::pair<InfoBarDelegate*, InfoBarDelegate*> >(details).ptr()->
          first->InfoBarClosed();
      break;
    default:
      TabContents::Observe(type, source, details);
      break;
  }
}

TestRenderViewHost* TestTabContents::pending_rvh() const {
  return static_cast<TestRenderViewHost*>(
      render_manager_.pending_render_view_host_);
}

bool TestTabContents::CreateRenderViewForRenderManager(
    RenderViewHost* render_view_host) {
  // This will go to a TestRenderViewHost.
  render_view_host->CreateRenderView(string16());
  return true;
}

TabContents* TestTabContents::Clone() {
  TabContents* tc = new TestTabContents(
      profile(), SiteInstance::CreateSiteInstance(profile()));
  tc->controller().CopyStateFrom(controller_);
  return tc;
}

void TestTabContents::NavigateAndCommit(const GURL& url) {
  controller().LoadURL(url, GURL(), PageTransition::LINK);
  GURL loaded_url(url);
  bool reverse_on_redirect = false;
  BrowserURLHandler::RewriteURLIfNecessary(
      &loaded_url, profile(), &reverse_on_redirect);

  // LoadURL created a navigation entry, now simulate the RenderView sending
  // a notification that it actually navigated.
  CommitPendingNavigation();
}

void TestTabContents::CommitPendingNavigation() {
  // If we are doing a cross-site navigation, this simulates the current RVH
  // notifying that it has unloaded so the pending RVH is resumed and can
  // navigate.
  ProceedWithCrossSiteNavigation();
  TestRenderViewHost* rvh = pending_rvh();
  if (!rvh)
    rvh = static_cast<TestRenderViewHost*>(render_manager_.current_host());

  const NavigationEntry* entry = controller().pending_entry();
  DCHECK(entry);
  int page_id = entry->page_id();
  if (page_id == -1) {
    // It's a new navigation, assign a never-seen page id to it.
    page_id =
        static_cast<MockRenderProcessHost*>(rvh->process())->max_page_id() + 1;
  }
  rvh->SendNavigate(page_id, entry->url());
}

void TestTabContents::ProceedWithCrossSiteNavigation() {
  if (!pending_rvh())
    return;
  render_manager_.ShouldClosePage(true, true);
}
