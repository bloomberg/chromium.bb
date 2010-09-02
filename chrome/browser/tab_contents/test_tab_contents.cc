// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/test_tab_contents.h"

#include "chrome/browser/browser_url_handler.h"
#include "chrome/browser/renderer_host/mock_render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/common/notification_service.h"

TestTabContents::TestTabContents(Profile* profile, SiteInstance* instance)
    : TabContents(profile, instance, MSG_ROUTING_NONE, NULL),
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

TestRenderViewHost* TestTabContents::pending_rvh() {
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
  controller().LoadURL(url, GURL(), 0);
  GURL loaded_url(url);
  bool reverse_on_redirect = false;
  BrowserURLHandler::RewriteURLIfNecessary(
      &loaded_url, profile(), &reverse_on_redirect);
  static_cast<TestRenderViewHost*>(render_view_host())->SendNavigate(
      static_cast<MockRenderProcessHost*>(render_view_host()->process())->
      max_page_id() + 1, loaded_url);
}
