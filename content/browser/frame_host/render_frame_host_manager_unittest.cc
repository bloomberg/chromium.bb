// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "content/browser/frame_host/cross_site_transferring_request.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/javascript_message_type.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_notification_tracker.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/load_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace content {
namespace {

class RenderFrameHostManagerTestWebUIControllerFactory
    : public WebUIControllerFactory {
 public:
  RenderFrameHostManagerTestWebUIControllerFactory()
    : should_create_webui_(false) {
  }
  ~RenderFrameHostManagerTestWebUIControllerFactory() override {}

  void set_should_create_webui(bool should_create_webui) {
    should_create_webui_ = should_create_webui;
  }

  // WebUIFactory implementation.
  WebUIController* CreateWebUIControllerForURL(WebUI* web_ui,
                                               const GURL& url) const override {
    if (!(should_create_webui_ && HasWebUIScheme(url)))
      return NULL;
    return new WebUIController(web_ui);
  }

  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) const override {
    return WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) const override {
    return HasWebUIScheme(url);
  }

  bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                              const GURL& url) const override {
    return HasWebUIScheme(url);
  }

 private:
  bool should_create_webui_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostManagerTestWebUIControllerFactory);
};

class BeforeUnloadFiredWebContentsDelegate : public WebContentsDelegate {
 public:
  BeforeUnloadFiredWebContentsDelegate() {}
  ~BeforeUnloadFiredWebContentsDelegate() override {}

  void BeforeUnloadFired(WebContents* web_contents,
                         bool proceed,
                         bool* proceed_to_fire_unload) override {
    *proceed_to_fire_unload = proceed;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BeforeUnloadFiredWebContentsDelegate);
};

class CloseWebContentsDelegate : public WebContentsDelegate {
 public:
  CloseWebContentsDelegate() : close_called_(false) {}
  ~CloseWebContentsDelegate() override {}

  void CloseContents(WebContents* web_contents) override {
    close_called_ = true;
  }

  bool is_closed() { return close_called_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(CloseWebContentsDelegate);

  bool close_called_;
};

// This observer keeps track of the last deleted RenderViewHost to avoid
// accessing it and causing use-after-free condition.
class RenderViewHostDeletedObserver : public WebContentsObserver {
 public:
  RenderViewHostDeletedObserver(RenderViewHost* rvh)
      : WebContentsObserver(WebContents::FromRenderViewHost(rvh)),
        process_id_(rvh->GetProcess()->GetID()),
        routing_id_(rvh->GetRoutingID()),
        deleted_(false) {
  }

  void RenderViewDeleted(RenderViewHost* render_view_host) override {
    if (render_view_host->GetProcess()->GetID() == process_id_ &&
        render_view_host->GetRoutingID() == routing_id_) {
      deleted_ = true;
    }
  }

  bool deleted() {
    return deleted_;
  }

 private:
  int process_id_;
  int routing_id_;
  bool deleted_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostDeletedObserver);
};

// This observer keeps track of the last created RenderFrameHost to allow tests
// to ensure that no RenderFrameHost objects are created when not expected.
class RenderFrameHostCreatedObserver : public WebContentsObserver {
 public:
  RenderFrameHostCreatedObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        created_(false) {
  }

  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    created_ = true;
  }

  bool created() {
    return created_;
  }

 private:
  bool created_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostCreatedObserver);
};

// This observer keeps track of the last deleted RenderFrameHost to avoid
// accessing it and causing use-after-free condition.
class RenderFrameHostDeletedObserver : public WebContentsObserver {
 public:
  RenderFrameHostDeletedObserver(RenderFrameHost* rfh)
      : WebContentsObserver(WebContents::FromRenderFrameHost(rfh)),
        process_id_(rfh->GetProcess()->GetID()),
        routing_id_(rfh->GetRoutingID()),
        deleted_(false) {
  }

  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override {
    if (render_frame_host->GetProcess()->GetID() == process_id_ &&
        render_frame_host->GetRoutingID() == routing_id_) {
      deleted_ = true;
    }
  }

  bool deleted() {
    return deleted_;
  }

 private:
  int process_id_;
  int routing_id_;
  bool deleted_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostDeletedObserver);
};

// This observer is used to check whether IPC messages are being filtered for
// swapped out RenderFrameHost objects. It observes the plugin crash and favicon
// update events, which the FilterMessagesWhileSwappedOut test simulates being
// sent. The test is successful if the event is not observed.
// See http://crbug.com/351815
class PluginFaviconMessageObserver : public WebContentsObserver {
 public:
  PluginFaviconMessageObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        plugin_crashed_(false),
        favicon_received_(false) { }

  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid) override {
    plugin_crashed_ = true;
  }

  void DidUpdateFaviconURL(const std::vector<FaviconURL>& candidates) override {
    favicon_received_ = true;
  }

  bool plugin_crashed() {
    return plugin_crashed_;
  }

  bool favicon_received() {
    return favicon_received_;
  }

 private:
  bool plugin_crashed_;
  bool favicon_received_;

  DISALLOW_COPY_AND_ASSIGN(PluginFaviconMessageObserver);
};

}  // namespace

class RenderFrameHostManagerTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    WebUIControllerFactory::RegisterFactory(&factory_);
  }

  void TearDown() override {
    RenderViewHostImplTestHarness::TearDown();
    WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
  }

  void set_should_create_webui(bool should_create_webui) {
    factory_.set_should_create_webui(should_create_webui);
  }

  void NavigateActiveAndCommit(const GURL& url) {
    // Note: we navigate the active RenderFrameHost because previous navigations
    // won't have committed yet, so NavigateAndCommit does the wrong thing
    // for us.
    controller().LoadURL(
        url, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

    // Simulate the BeforeUnload_ACK that is received from the current renderer
    // for a cross-site navigation.
    // PlzNavigate: it is necessary to call PrepareForCommit before getting the
    // main and the pending frame because when we are trying to navigate to a
    // WebUI from a new tab, a RenderFrameHost is created to display it that is
    // committed immediately (since it is a new tab). Therefore the main frame
    // is replaced without a pending frame being created, and we don't get the
    // right values for the RFH to navigate: we try to use the old one that has
    // been deleted in the meantime.
    contents()->GetMainFrame()->PrepareForCommit();

    TestRenderFrameHost* old_rfh = contents()->GetMainFrame();
    TestRenderFrameHost* active_rfh = contents()->GetPendingMainFrame()
                                          ? contents()->GetPendingMainFrame()
                                          : old_rfh;
    EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, old_rfh->rfh_state());

    // Commit the navigation with a new page ID.
    int32 max_page_id = contents()->GetMaxPageIDForSiteInstance(
        active_rfh->GetSiteInstance());

    // Use an observer to avoid accessing a deleted renderer later on when the
    // state is being checked.
    RenderFrameHostDeletedObserver rfh_observer(old_rfh);
    RenderViewHostDeletedObserver rvh_observer(old_rfh->GetRenderViewHost());
    active_rfh->SendNavigate(max_page_id + 1, url);

    // Make sure that we start to run the unload handler at the time of commit.
    bool expecting_rfh_shutdown = false;
    if (old_rfh != active_rfh && !rfh_observer.deleted()) {
      EXPECT_EQ(RenderFrameHostImpl::STATE_PENDING_SWAP_OUT,
                old_rfh->rfh_state());
      if (!old_rfh->GetSiteInstance()->active_frame_count()) {
        expecting_rfh_shutdown = true;
        EXPECT_TRUE(
            old_rfh->frame_tree_node()->render_manager()->IsPendingDeletion(
                old_rfh));
      }
    }

    // Simulate the swap out ACK coming from the pending renderer.  This should
    // either shut down the old RFH or leave it in a swapped out state.
    if (old_rfh != active_rfh) {
      old_rfh->OnSwappedOut();
      if (expecting_rfh_shutdown) {
        EXPECT_TRUE(rfh_observer.deleted());
        EXPECT_TRUE(rvh_observer.deleted());
      } else {
        EXPECT_EQ(RenderFrameHostImpl::STATE_SWAPPED_OUT,
                  old_rfh->rfh_state());
      }
    }
    EXPECT_EQ(active_rfh, contents()->GetMainFrame());
    EXPECT_EQ(NULL, contents()->GetPendingMainFrame());
  }

  bool ShouldSwapProcesses(RenderFrameHostManager* manager,
                           const NavigationEntryImpl* current_entry,
                           const NavigationEntryImpl* new_entry) const {
    CHECK(new_entry);
    BrowserContext* browser_context =
        manager->delegate_->GetControllerForRenderManager().GetBrowserContext();
    const GURL& current_effective_url = current_entry ?
        SiteInstanceImpl::GetEffectiveURL(browser_context,
                                          current_entry->GetURL()) :
        manager->render_frame_host_->GetSiteInstance()->GetSiteURL();
    bool current_is_view_source_mode = current_entry ?
        current_entry->IsViewSourceMode() : new_entry->IsViewSourceMode();
    return manager->ShouldSwapBrowsingInstancesForNavigation(
        current_effective_url,
        current_is_view_source_mode,
        new_entry->site_instance(),
        SiteInstanceImpl::GetEffectiveURL(browser_context, new_entry->GetURL()),
        new_entry->IsViewSourceMode());
  }

  // Creates a test RenderFrameHost that's swapped out.
  TestRenderFrameHost* CreateSwappedOutRenderFrameHost() {
    const GURL kChromeURL("chrome://foo");
    const GURL kDestUrl("http://www.google.com/");

    // Navigate our first tab to a chrome url and then to the destination.
    NavigateActiveAndCommit(kChromeURL);
    TestRenderFrameHost* ntp_rfh = contents()->GetMainFrame();

    // Navigate to a cross-site URL.
    contents()->GetController().LoadURL(
        kDestUrl, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
    contents()->GetMainFrame()->PrepareForCommit();
    EXPECT_TRUE(contents()->cross_navigation_pending());

    // Manually increase the number of active frames in the
    // SiteInstance that ntp_rfh belongs to, to prevent it from being
    // destroyed when it gets swapped out.
    ntp_rfh->GetSiteInstance()->increment_active_frame_count();

    TestRenderFrameHost* dest_rfh = contents()->GetPendingMainFrame();
    CHECK(dest_rfh);
    EXPECT_NE(ntp_rfh, dest_rfh);

    // BeforeUnload finishes.
    ntp_rfh->SendBeforeUnloadACK(true);

    dest_rfh->SendNavigate(101, kDestUrl);
    ntp_rfh->OnSwappedOut();

    EXPECT_TRUE(ntp_rfh->is_swapped_out());
    return ntp_rfh;
  }

  // Returns the RenderFrameHost that should be used in the navigation to
  // |entry|.
  RenderFrameHostImpl* GetFrameHostForNavigation(
      RenderFrameHostManager* manager,
      const NavigationEntryImpl& entry) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableBrowserSideNavigation)) {
      scoped_ptr<NavigationRequest> navigation_request =
          NavigationRequest::CreateBrowserInitiated(
              manager->frame_tree_node_, entry, FrameMsg_Navigate_Type::NORMAL,
              base::TimeTicks::Now(),
              static_cast<NavigationControllerImpl*>(&controller()));
      return manager->GetFrameHostForNavigation(*navigation_request);
    }
    return manager->Navigate(entry);
  }

  // Returns the pending RenderFrameHost.
  // PlzNavigate: returns the speculative RenderFrameHost.
  RenderFrameHostImpl* GetPendingFrameHost(
      RenderFrameHostManager* manager) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableBrowserSideNavigation)) {
      return manager->speculative_render_frame_host_.get();
    }
    return manager->pending_frame_host();
  }

 private:
  RenderFrameHostManagerTestWebUIControllerFactory factory_;
};

// Tests that when you navigate from a chrome:// url to another page, and
// then do that same thing in another tab, that the two resulting pages have
// different SiteInstances, BrowsingInstances, and RenderProcessHosts. This is
// a regression test for bug 9364.
TEST_F(RenderFrameHostManagerTest, NewTabPageProcesses) {
  set_should_create_webui(true);
  const GURL kChromeUrl("chrome://foo");
  const GURL kDestUrl("http://www.google.com/");

  // Navigate our first tab to the chrome url and then to the destination,
  // ensuring we grant bindings to the chrome URL.
  NavigateActiveAndCommit(kChromeUrl);
  EXPECT_TRUE(active_rvh()->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);
  NavigateActiveAndCommit(kDestUrl);

  EXPECT_FALSE(contents()->GetPendingMainFrame());

  // Make a second tab.
  scoped_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), NULL));

  // Load the two URLs in the second tab. Note that the first navigation creates
  // a RFH that's not pending (since there is no cross-site transition), so
  // we use the committed one.
  contents2->GetController().LoadURL(
      kChromeUrl, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  contents2->GetMainFrame()->PrepareForCommit();
  TestRenderFrameHost* ntp_rfh2 = contents2->GetMainFrame();
  EXPECT_FALSE(contents2->cross_navigation_pending());
  ntp_rfh2->SendNavigate(100, kChromeUrl);

  // The second one is the opposite, creating a cross-site transition and
  // requiring a beforeunload ack.
  contents2->GetController().LoadURL(
      kDestUrl, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  contents2->GetMainFrame()->PrepareForCommit();
  EXPECT_TRUE(contents2->cross_navigation_pending());
  TestRenderFrameHost* dest_rfh2 = contents2->GetPendingMainFrame();
  ASSERT_TRUE(dest_rfh2);

  dest_rfh2->SendNavigate(101, kDestUrl);

  // The two RFH's should be different in every way.
  EXPECT_NE(contents()->GetMainFrame()->GetProcess(), dest_rfh2->GetProcess());
  EXPECT_NE(contents()->GetMainFrame()->GetSiteInstance(),
            dest_rfh2->GetSiteInstance());
  EXPECT_FALSE(dest_rfh2->GetSiteInstance()->IsRelatedSiteInstance(
                   contents()->GetMainFrame()->GetSiteInstance()));

  // Navigate both to the new tab page, and verify that they share a
  // RenderProcessHost (not a SiteInstance).
  NavigateActiveAndCommit(kChromeUrl);
  EXPECT_FALSE(contents()->GetPendingMainFrame());

  contents2->GetController().LoadURL(
      kChromeUrl, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  contents2->GetMainFrame()->PrepareForCommit();
  contents2->GetPendingMainFrame()->SendNavigate(102, kChromeUrl);

  EXPECT_NE(contents()->GetMainFrame()->GetSiteInstance(),
            contents2->GetMainFrame()->GetSiteInstance());
  EXPECT_EQ(contents()->GetMainFrame()->GetSiteInstance()->GetProcess(),
            contents2->GetMainFrame()->GetSiteInstance()->GetProcess());
}

// Ensure that the browser ignores most IPC messages that arrive from a
// RenderViewHost that has been swapped out.  We do not want to take
// action on requests from a non-active renderer.  The main exception is
// for synchronous messages, which cannot be ignored without leaving the
// renderer in a stuck state.  See http://crbug.com/93427.
TEST_F(RenderFrameHostManagerTest, FilterMessagesWhileSwappedOut) {
  const GURL kChromeURL("chrome://foo");
  const GURL kDestUrl("http://www.google.com/");
  std::vector<FaviconURL> icons;

  // Navigate our first tab to a chrome url and then to the destination.
  NavigateActiveAndCommit(kChromeURL);
  TestRenderFrameHost* ntp_rfh = contents()->GetMainFrame();

  // Send an update favicon message and make sure it works.
  const base::string16 ntp_title = base::ASCIIToUTF16("NTP Title");
  {
    PluginFaviconMessageObserver observer(contents());
    EXPECT_TRUE(ntp_rfh->GetRenderViewHost()->OnMessageReceived(
                    ViewHostMsg_UpdateFaviconURL(
                        ntp_rfh->GetRenderViewHost()->GetRoutingID(), icons)));
    EXPECT_TRUE(observer.favicon_received());
  }
  // Create one more frame in the same SiteInstance where ntp_rfh
  // exists so that it doesn't get deleted on navigation to another
  // site.
  ntp_rfh->GetSiteInstance()->increment_active_frame_count();


  // Navigate to a cross-site URL.
  NavigateActiveAndCommit(kDestUrl);
  TestRenderFrameHost* dest_rfh = contents()->GetMainFrame();
  ASSERT_TRUE(dest_rfh);
  EXPECT_NE(ntp_rfh, dest_rfh);

  // The new RVH should be able to update its favicon.
  const base::string16 dest_title = base::ASCIIToUTF16("Google");
  {
    PluginFaviconMessageObserver observer(contents());
    EXPECT_TRUE(
        dest_rfh->GetRenderViewHost()->OnMessageReceived(
            ViewHostMsg_UpdateFaviconURL(
                dest_rfh->GetRenderViewHost()->GetRoutingID(), icons)));
    EXPECT_TRUE(observer.favicon_received());
  }

  // The old renderer, being slow, now updates the favicon. It should be
  // filtered out and not take effect.
  EXPECT_TRUE(ntp_rfh->is_swapped_out());
  {
    PluginFaviconMessageObserver observer(contents());
    EXPECT_TRUE(
        ntp_rfh->GetRenderViewHost()->OnMessageReceived(
            ViewHostMsg_UpdateFaviconURL(
                dest_rfh->GetRenderViewHost()->GetRoutingID(), icons)));
    EXPECT_FALSE(observer.favicon_received());
  }

#if defined(ENABLE_PLUGINS)
  // The same logic should apply to RenderFrameHosts as well and routing through
  // swapped out RFH shouldn't be allowed. Use a PluginCrashObserver to check
  // if the IPC message is allowed through or not.
  {
    PluginFaviconMessageObserver observer(contents());
    EXPECT_TRUE(ntp_rfh->OnMessageReceived(
                    FrameHostMsg_PluginCrashed(
                        ntp_rfh->GetRoutingID(), base::FilePath(), 0)));
    EXPECT_FALSE(observer.plugin_crashed());
  }
#endif

  // We cannot filter out synchronous IPC messages, because the renderer would
  // be left waiting for a reply.  We pick RunBeforeUnloadConfirm as an example
  // that can run easily within a unit test, and that needs to receive a reply
  // without showing an actual dialog.
  MockRenderProcessHost* ntp_process_host =
      static_cast<MockRenderProcessHost*>(ntp_rfh->GetProcess());
  ntp_process_host->sink().ClearMessages();
  const base::string16 msg = base::ASCIIToUTF16("Message");
  bool result = false;
  base::string16 unused;
  FrameHostMsg_RunBeforeUnloadConfirm before_unload_msg(
      ntp_rfh->GetRoutingID(), kChromeURL, msg, false, &result, &unused);
  // Enable pumping for check in BrowserMessageFilter::CheckCanDispatchOnUI.
  before_unload_msg.EnableMessagePumping();
  EXPECT_TRUE(ntp_rfh->OnMessageReceived(before_unload_msg));
  EXPECT_TRUE(ntp_process_host->sink().GetUniqueMessageMatching(IPC_REPLY_ID));

  // Also test RunJavaScriptMessage.
  ntp_process_host->sink().ClearMessages();
  FrameHostMsg_RunJavaScriptMessage js_msg(
      ntp_rfh->GetRoutingID(), msg, msg, kChromeURL,
      JAVASCRIPT_MESSAGE_TYPE_CONFIRM, &result, &unused);
  js_msg.EnableMessagePumping();
  EXPECT_TRUE(ntp_rfh->OnMessageReceived(js_msg));
  EXPECT_TRUE(ntp_process_host->sink().GetUniqueMessageMatching(IPC_REPLY_ID));
}

// Ensure that frames aren't added to the frame tree, if the message is coming
// from a process different than the parent frame's current RenderFrameHost
// process. Otherwise it is possible to have collisions of routing ids, as they
// are scoped per process. See https://crbug.com/415059.
TEST_F(RenderFrameHostManagerTest, DropCreateChildFrameWhileSwappedOut) {
  const GURL kUrl1("http://foo.com");
  const GURL kUrl2("http://www.google.com/");

  // Navigate to the first site.
  NavigateActiveAndCommit(kUrl1);
  TestRenderFrameHost* initial_rfh = contents()->GetMainFrame();
  {
    RenderFrameHostCreatedObserver observer(contents());
    initial_rfh->OnCreateChildFrame(
        initial_rfh->GetProcess()->GetNextRoutingID(), std::string(),
        SandboxFlags::NONE);
    EXPECT_TRUE(observer.created());
  }

  // Create one more frame in the same SiteInstance where initial_rfh
  // exists so that initial_rfh doesn't get deleted on navigation to another
  // site.
  initial_rfh->GetSiteInstance()->increment_active_frame_count();

  // Navigate to a cross-site URL.
  NavigateActiveAndCommit(kUrl2);
  EXPECT_TRUE(initial_rfh->is_swapped_out());

  TestRenderFrameHost* dest_rfh = contents()->GetMainFrame();
  ASSERT_TRUE(dest_rfh);
  EXPECT_NE(initial_rfh, dest_rfh);

  {
    // Since the old RFH is now swapped out, it shouldn't process any messages
    // to create child frames.
    RenderFrameHostCreatedObserver observer(contents());
    initial_rfh->OnCreateChildFrame(
        initial_rfh->GetProcess()->GetNextRoutingID(), std::string(),
        SandboxFlags::NONE);
    EXPECT_FALSE(observer.created());
  }
}

TEST_F(RenderFrameHostManagerTest, WhiteListSwapCompositorFrame) {
  TestRenderFrameHost* swapped_out_rfh = CreateSwappedOutRenderFrameHost();
  TestRenderWidgetHostView* swapped_out_rwhv =
      static_cast<TestRenderWidgetHostView*>(
          swapped_out_rfh->GetRenderViewHost()->GetView());
  EXPECT_FALSE(swapped_out_rwhv->did_swap_compositor_frame());

  MockRenderProcessHost* process_host =
      static_cast<MockRenderProcessHost*>(swapped_out_rfh->GetProcess());
  process_host->sink().ClearMessages();

  cc::CompositorFrame frame;
  ViewHostMsg_SwapCompositorFrame msg(
      rvh()->GetRoutingID(), 0, frame, std::vector<IPC::Message>());

  EXPECT_TRUE(swapped_out_rfh->render_view_host()->OnMessageReceived(msg));
  EXPECT_TRUE(swapped_out_rwhv->did_swap_compositor_frame());
}

// Test if RenderViewHost::GetRenderWidgetHosts() only returns active
// widgets.
TEST_F(RenderFrameHostManagerTest, GetRenderWidgetHostsReturnsActiveViews) {
  TestRenderFrameHost* swapped_out_rfh = CreateSwappedOutRenderFrameHost();
  EXPECT_TRUE(swapped_out_rfh->is_swapped_out());

  scoped_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());
  // We know that there is the only one active widget. Another view is
  // now swapped out, so the swapped out view is not included in the
  // list.
  RenderWidgetHost* widget = widgets->GetNextHost();
  EXPECT_FALSE(widgets->GetNextHost());
  RenderViewHost* rvh = RenderViewHost::From(widget);
  EXPECT_TRUE(static_cast<RenderViewHostImpl*>(rvh)->is_active());
}

// Test if RenderViewHost::GetRenderWidgetHosts() returns a subset of
// RenderViewHostImpl::GetAllRenderWidgetHosts().
// RenderViewHost::GetRenderWidgetHosts() returns only active widgets, but
// RenderViewHostImpl::GetAllRenderWidgetHosts() returns everything
// including swapped out ones.
TEST_F(RenderFrameHostManagerTest,
       GetRenderWidgetHostsWithinGetAllRenderWidgetHosts) {
  TestRenderFrameHost* swapped_out_rfh = CreateSwappedOutRenderFrameHost();
  EXPECT_TRUE(swapped_out_rfh->is_swapped_out());

  scoped_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());

  while (RenderWidgetHost* w = widgets->GetNextHost()) {
    bool found = false;
    scoped_ptr<RenderWidgetHostIterator> all_widgets(
        RenderWidgetHostImpl::GetAllRenderWidgetHosts());
    while (RenderWidgetHost* widget = all_widgets->GetNextHost()) {
      if (w == widget) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }
}

// Test if SiteInstanceImpl::active_frame_count() is correctly updated
// as frames in a SiteInstance get swapped out and in.
TEST_F(RenderFrameHostManagerTest, ActiveFrameCountWhileSwappingInAndOut) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();

  SiteInstanceImpl* instance1 = rfh1->GetSiteInstance();
  EXPECT_EQ(instance1->active_frame_count(), 1U);

  // Create 2 new tabs and simulate them being the opener chain for the main
  // tab.  They should be in the same SiteInstance.
  scoped_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), instance1));
  contents()->SetOpener(opener1.get());

  scoped_ptr<TestWebContents> opener2(
      TestWebContents::Create(browser_context(), instance1));
  opener1->SetOpener(opener2.get());

  EXPECT_EQ(instance1->active_frame_count(), 3U);

  // Navigate to a cross-site URL (different SiteInstance but same
  // BrowsingInstance).
  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  SiteInstanceImpl* instance2 = rfh2->GetSiteInstance();

  // rvh2 is on chromium.org which is different from google.com on
  // which other tabs are.
  EXPECT_EQ(instance2->active_frame_count(), 1U);

  // There are two active views on google.com now.
  EXPECT_EQ(instance1->active_frame_count(), 2U);

  // Navigate to the original origin (google.com).
  contents()->NavigateAndCommit(kUrl1);

  EXPECT_EQ(instance1->active_frame_count(), 3U);
}

// This deletes a WebContents when the given RVH is deleted. This is
// only for testing whether deleting an RVH does not cause any UaF in
// other parts of the system. For now, this class is only used for the
// next test cases to detect the bug mentioned at
// http://crbug.com/259859.
class RenderViewHostDestroyer : public WebContentsObserver {
 public:
  RenderViewHostDestroyer(RenderViewHost* render_view_host,
                          WebContents* web_contents)
      : WebContentsObserver(WebContents::FromRenderViewHost(render_view_host)),
        render_view_host_(render_view_host),
        web_contents_(web_contents) {}

  void RenderViewDeleted(RenderViewHost* render_view_host) override {
    if (render_view_host == render_view_host_)
      delete web_contents_;
  }

 private:
  RenderViewHost* render_view_host_;
  WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostDestroyer);
};

// Test if ShutdownRenderViewHostsInSiteInstance() does not touch any
// RenderWidget that has been freed while deleting a RenderViewHost in
// a previous iteration. This is a regression test for
// http://crbug.com/259859.
TEST_F(RenderFrameHostManagerTest,
       DetectUseAfterFreeInShutdownRenderViewHostsInSiteInstance) {
  const GURL kChromeURL("chrome://newtab");
  const GURL kUrl1("http://www.google.com");
  const GURL kUrl2("http://www.chromium.org");

  // Navigate our first tab to a chrome url and then to the destination.
  NavigateActiveAndCommit(kChromeURL);
  TestRenderFrameHost* ntp_rfh = contents()->GetMainFrame();

  // Create one more tab and navigate to kUrl1.  web_contents is not
  // wrapped as scoped_ptr since it intentionally deleted by destroyer
  // below as part of this test.
  TestWebContents* web_contents =
      TestWebContents::Create(browser_context(), ntp_rfh->GetSiteInstance());
  web_contents->NavigateAndCommit(kUrl1);
  RenderViewHostDestroyer destroyer(ntp_rfh->GetRenderViewHost(),
                                    web_contents);

  // This causes the first tab to navigate to kUrl2, which destroys
  // the ntp_rfh in ShutdownRenderViewHostsInSiteInstance(). When
  // ntp_rfh is destroyed, it also destroys the RVHs in web_contents
  // too. This can test whether
  // SiteInstanceImpl::ShutdownRenderViewHostsInSiteInstance() can
  // touch any object freed in this way or not while iterating through
  // all widgets.
  contents()->NavigateAndCommit(kUrl2);
}

// When there is an error with the specified page, renderer exits view-source
// mode. See WebFrameImpl::DidFail(). We check by this test that
// EnableViewSourceMode message is sent on every navigation regardless
// RenderView is being newly created or reused.
TEST_F(RenderFrameHostManagerTest, AlwaysSendEnableViewSourceMode) {
  const GURL kChromeUrl("chrome://foo");
  const GURL kUrl("view-source:http://foo");

  // We have to navigate to some page at first since without this, the first
  // navigation will reuse the SiteInstance created by Init(), and the second
  // one will create a new SiteInstance. Because current_instance and
  // new_instance will be different, a new RenderViewHost will be created for
  // the second navigation. We have to avoid this in order to exercise the
  // target code patch.
  NavigateActiveAndCommit(kChromeUrl);

  // Navigate.
  controller().LoadURL(
      kUrl, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  // Simulate response from RenderFrame for DispatchBeforeUnload.
  contents()->GetMainFrame()->PrepareForCommit();
  ASSERT_TRUE(contents()->GetPendingMainFrame())
      << "Expected new pending RenderFrameHost to be created.";
  RenderFrameHost* last_rfh = contents()->GetPendingMainFrame();
  int32 new_id =
      contents()->GetMaxPageIDForSiteInstance(last_rfh->GetSiteInstance()) + 1;
  contents()->GetPendingMainFrame()->SendNavigate(new_id, kUrl);
  EXPECT_EQ(controller().GetLastCommittedEntryIndex(), 1);
  ASSERT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_TRUE(kUrl == controller().GetLastCommittedEntry()->GetURL());
  EXPECT_FALSE(controller().GetPendingEntry());
  // Because we're using TestWebContents and TestRenderViewHost in this
  // unittest, no one calls WebContentsImpl::RenderViewCreated(). So, we see no
  // EnableViewSourceMode message, here.

  // Clear queued messages before load.
  process()->sink().ClearMessages();
  // Navigate, again.
  controller().LoadURL(
      kUrl, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->GetMainFrame()->PrepareForCommit();
  // The same RenderViewHost should be reused.
  EXPECT_FALSE(contents()->GetPendingMainFrame());
  EXPECT_TRUE(last_rfh == contents()->GetMainFrame());
  // Navigate using the returned page_id.
  contents()->GetMainFrame()->SendNavigate(new_id, kUrl);
  EXPECT_EQ(controller().GetLastCommittedEntryIndex(), 1);
  EXPECT_FALSE(controller().GetPendingEntry());
  // New message should be sent out to make sure to enter view-source mode.
  EXPECT_TRUE(process()->sink().GetUniqueMessageMatching(
      ViewMsg_EnableViewSourceMode::ID));
}

// Tests the Init function by checking the initial RenderViewHost.
TEST_F(RenderFrameHostManagerTest, Init) {
  // Using TestBrowserContext.
  SiteInstanceImpl* instance =
      static_cast<SiteInstanceImpl*>(SiteInstance::Create(browser_context()));
  EXPECT_FALSE(instance->HasSite());

  scoped_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), instance));

  RenderFrameHostManager* manager = web_contents->GetRenderManagerForTesting();
  RenderViewHostImpl* rvh = manager->current_host();
  RenderFrameHostImpl* rfh = manager->current_frame_host();
  ASSERT_TRUE(rvh);
  ASSERT_TRUE(rfh);
  EXPECT_EQ(rvh, rfh->render_view_host());
  EXPECT_EQ(instance, rvh->GetSiteInstance());
  EXPECT_EQ(web_contents.get(), rvh->GetDelegate());
  EXPECT_EQ(web_contents.get(), rfh->delegate());
  EXPECT_TRUE(manager->GetRenderWidgetHostView());
  EXPECT_FALSE(manager->pending_render_view_host());
}

// Tests the Navigate function. We navigate three sites consecutively and check
// how the pending/committed RenderViewHost are modified.
TEST_F(RenderFrameHostManagerTest, Navigate) {
  TestNotificationTracker notifications;

  SiteInstance* instance = SiteInstance::Create(browser_context());

  scoped_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), instance));
  notifications.ListenFor(NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                          Source<WebContents>(web_contents.get()));

  RenderFrameHostManager* manager = web_contents->GetRenderManagerForTesting();
  RenderFrameHostImpl* host = NULL;

  // 1) The first navigation. --------------------------
  const GURL kUrl1("http://www.google.com/");
  NavigationEntryImpl entry1(
      NULL /* instance */, -1 /* page_id */, kUrl1, Referrer(),
      base::string16() /* title */, ui::PAGE_TRANSITION_TYPED,
      false /* is_renderer_init */);
  host = GetFrameHostForNavigation(manager, entry1);

  // The RenderFrameHost created in Init will be reused.
  EXPECT_TRUE(host == manager->current_frame_host());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // Commit.
  manager->DidNavigateFrame(host, true);
  // Commit to SiteInstance should be delayed until RenderFrame commit.
  EXPECT_TRUE(host == manager->current_frame_host());
  ASSERT_TRUE(host);
  EXPECT_FALSE(host->GetSiteInstance()->HasSite());
  host->GetSiteInstance()->SetSite(kUrl1);

  // 2) Navigate to next site. -------------------------
  const GURL kUrl2("http://www.google.com/foo");
  NavigationEntryImpl entry2(
      NULL /* instance */, -1 /* page_id */, kUrl2,
      Referrer(kUrl1, blink::WebReferrerPolicyDefault),
      base::string16() /* title */, ui::PAGE_TRANSITION_LINK,
      true /* is_renderer_init */);
  host = GetFrameHostForNavigation(manager, entry2);

  // The RenderFrameHost created in Init will be reused.
  EXPECT_TRUE(host == manager->current_frame_host());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // Commit.
  manager->DidNavigateFrame(host, true);
  EXPECT_TRUE(host == manager->current_frame_host());
  ASSERT_TRUE(host);
  EXPECT_TRUE(host->GetSiteInstance()->HasSite());

  // 3) Cross-site navigate to next site. --------------
  const GURL kUrl3("http://webkit.org/");
  NavigationEntryImpl entry3(
      NULL /* instance */, -1 /* page_id */, kUrl3,
      Referrer(kUrl2, blink::WebReferrerPolicyDefault),
      base::string16() /* title */, ui::PAGE_TRANSITION_LINK,
      false /* is_renderer_init */);
  host = GetFrameHostForNavigation(manager, entry3);

  // A new RenderFrameHost should be created.
  EXPECT_TRUE(GetPendingFrameHost(manager));
  ASSERT_EQ(host, GetPendingFrameHost(manager));

  notifications.Reset();

  // Commit.
  manager->DidNavigateFrame(GetPendingFrameHost(manager), true);
  EXPECT_TRUE(host == manager->current_frame_host());
  ASSERT_TRUE(host);
  EXPECT_TRUE(host->GetSiteInstance()->HasSite());
  // Check the pending RenderFrameHost has been committed.
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // We should observe a notification.
  EXPECT_TRUE(
      notifications.Check1AndReset(NOTIFICATION_RENDER_VIEW_HOST_CHANGED));
}

// Tests WebUI creation.
TEST_F(RenderFrameHostManagerTest, WebUI) {
  set_should_create_webui(true);
  SiteInstance* instance = SiteInstance::Create(browser_context());

  scoped_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), instance));
  RenderFrameHostManager* manager = web_contents->GetRenderManagerForTesting();
  RenderFrameHostImpl* initial_rfh = manager->current_frame_host();

  EXPECT_FALSE(manager->current_host()->IsRenderViewLive());
  EXPECT_FALSE(manager->web_ui());
  EXPECT_TRUE(initial_rfh);

  const GURL kUrl("chrome://foo");
  NavigationEntryImpl entry(NULL /* instance */, -1 /* page_id */, kUrl,
                            Referrer(), base::string16() /* title */,
                            ui::PAGE_TRANSITION_TYPED,
                            false /* is_renderer_init */);
  RenderFrameHostImpl* host = GetFrameHostForNavigation(manager, entry);

  // We commit the pending RenderFrameHost immediately because the previous
  // RenderFrameHost was not live.  We test a case where it is live in
  // WebUIInNewTab.
  EXPECT_TRUE(host);
  EXPECT_NE(initial_rfh, host);
  EXPECT_EQ(host, manager->current_frame_host());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // It's important that the site instance get set on the Web UI page as soon
  // as the navigation starts, rather than lazily after it commits, so we don't
  // try to re-use the SiteInstance/process for non Web UI things that may
  // get loaded in between.
  EXPECT_TRUE(host->GetSiteInstance()->HasSite());
  EXPECT_EQ(kUrl, host->GetSiteInstance()->GetSiteURL());

  // The Web UI is committed immediately because the RenderViewHost has not been
  // used yet. UpdateStateForNavigate() took the short cut path.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation)) {
    EXPECT_FALSE(manager->speculative_web_ui());
  } else {
    EXPECT_FALSE(manager->pending_web_ui());
  }
  EXPECT_TRUE(manager->web_ui());

  // Commit.
  manager->DidNavigateFrame(host, true);
  EXPECT_TRUE(
      host->render_view_host()->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);
}

// Tests that we can open a WebUI link in a new tab from a WebUI page and still
// grant the correct bindings.  http://crbug.com/189101.
TEST_F(RenderFrameHostManagerTest, WebUIInNewTab) {
  set_should_create_webui(true);
  SiteInstance* blank_instance = SiteInstance::Create(browser_context());

  // Create a blank tab.
  scoped_ptr<TestWebContents> web_contents1(
      TestWebContents::Create(browser_context(), blank_instance));
  RenderFrameHostManager* manager1 =
      web_contents1->GetRenderManagerForTesting();
  // Test the case that new RVH is considered live.
  manager1->current_host()->CreateRenderView(
      base::string16(), -1, MSG_ROUTING_NONE, -1, false);
  EXPECT_TRUE(manager1->current_host()->IsRenderViewLive());
  EXPECT_TRUE(manager1->current_frame_host()->IsRenderFrameLive());

  // Navigate to a WebUI page.
  const GURL kUrl1("chrome://foo");
  NavigationEntryImpl entry1(NULL /* instance */, -1 /* page_id */, kUrl1,
                             Referrer(), base::string16() /* title */,
                             ui::PAGE_TRANSITION_TYPED,
                             false /* is_renderer_init */);
  RenderFrameHostImpl* host1 = GetFrameHostForNavigation(manager1, entry1);

  // We should have a pending navigation to the WebUI RenderViewHost.
  // It should already have bindings.
  EXPECT_EQ(host1, GetPendingFrameHost(manager1));
  EXPECT_NE(host1, manager1->current_frame_host());
  EXPECT_TRUE(
      host1->render_view_host()->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  // Commit and ensure we still have bindings.
  manager1->DidNavigateFrame(host1, true);
  SiteInstance* webui_instance = host1->GetSiteInstance();
  EXPECT_EQ(host1, manager1->current_frame_host());
  EXPECT_TRUE(
      host1->render_view_host()->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  // Now simulate clicking a link that opens in a new tab.
  scoped_ptr<TestWebContents> web_contents2(
      TestWebContents::Create(browser_context(), webui_instance));
  RenderFrameHostManager* manager2 =
      web_contents2->GetRenderManagerForTesting();
  // Make sure the new RVH is considered live.  This is usually done in
  // RenderWidgetHost::Init when opening a new tab from a link.
  manager2->current_host()->CreateRenderView(
      base::string16(), -1, MSG_ROUTING_NONE, -1, false);
  EXPECT_TRUE(manager2->current_host()->IsRenderViewLive());

  const GURL kUrl2("chrome://foo/bar");
  NavigationEntryImpl entry2(NULL /* instance */, -1 /* page_id */, kUrl2,
                             Referrer(), base::string16() /* title */,
                             ui::PAGE_TRANSITION_LINK,
                             true /* is_renderer_init */);
  RenderFrameHostImpl* host2 = GetFrameHostForNavigation(manager2, entry2);

  // No cross-process transition happens because we are already in the right
  // SiteInstance.  We should grant bindings immediately.
  EXPECT_EQ(host2, manager2->current_frame_host());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation)) {
    EXPECT_TRUE(manager2->speculative_web_ui());
  } else {
    EXPECT_TRUE(manager2->pending_web_ui());
  }
  EXPECT_TRUE(
      host2->render_view_host()->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  manager2->DidNavigateFrame(host2, true);
}

// Tests that we don't end up in an inconsistent state if a page does a back and
// then reload. http://crbug.com/51680
// Also tests that only user-gesture navigations can interrupt cross-process
// navigations. http://crbug.com/75195
TEST_F(RenderFrameHostManagerTest, PageDoesBackAndReload) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.evil-site.com/");

  // Navigate to a safe site, then an evil site.
  // This will switch RenderFrameHosts.  We cannot assert that the first and
  // second RFHs are different, though, because the first one may be promptly
  // deleted.
  contents()->NavigateAndCommit(kUrl1);
  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* evil_rfh = contents()->GetMainFrame();

  // Now let's simulate the evil page calling history.back().
  contents()->OnGoToEntryAtOffset(-1);
  contents()->GetMainFrame()->PrepareForCommit();
  // We should have a new pending RFH.
  // Note that in this case, the navigation has not committed, so evil_rfh will
  // not be deleted yet.
  EXPECT_NE(evil_rfh, contents()->GetPendingMainFrame());
  EXPECT_NE(evil_rfh->GetRenderViewHost(),
            contents()->GetPendingMainFrame()->GetRenderViewHost());

  // Before that RFH has committed, the evil page reloads itself.
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.page_id = 1;
  params.url = kUrl2;
  params.transition = ui::PAGE_TRANSITION_CLIENT_REDIRECT;
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.was_within_same_page = false;
  params.is_post = false;
  params.page_state = PageState::CreateFromURL(kUrl2);

  contents()->GetFrameTree()->root()->navigator()->DidNavigate(evil_rfh,
                                                               params);

  // That should NOT have cancelled the pending RFH, because the reload did
  // not have a user gesture. Thus, the pending back navigation will still
  // eventually commit.
  EXPECT_TRUE(contents()->GetRenderManagerForTesting()->
      pending_render_view_host() != NULL);
  EXPECT_TRUE(contents()->GetRenderManagerForTesting()->pending_frame_host() !=
              NULL);
  EXPECT_EQ(evil_rfh,
            contents()->GetRenderManagerForTesting()->current_frame_host());
  EXPECT_EQ(evil_rfh->GetRenderViewHost(),
            contents()->GetRenderManagerForTesting()->current_host());

  // Also we should not have a pending navigation entry.
  EXPECT_TRUE(contents()->GetController().GetPendingEntry() == NULL);
  NavigationEntry* entry = contents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(kUrl2, entry->GetURL());

  // Now do the same but as a user gesture.
  params.gesture = NavigationGestureUser;
  contents()->GetFrameTree()->root()->navigator()->DidNavigate(evil_rfh,
                                                               params);

  // User navigation should have cancelled the pending RFH.
  EXPECT_TRUE(contents()->GetRenderManagerForTesting()->
      pending_render_view_host() == NULL);
  EXPECT_TRUE(contents()->GetRenderManagerForTesting()->pending_frame_host() ==
              NULL);
  EXPECT_EQ(evil_rfh,
            contents()->GetRenderManagerForTesting()->current_frame_host());
  EXPECT_EQ(evil_rfh->GetRenderViewHost(),
            contents()->GetRenderManagerForTesting()->current_host());

  // Also we should not have a pending navigation entry.
  EXPECT_TRUE(contents()->GetController().GetPendingEntry() == NULL);
  entry = contents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(kUrl2, entry->GetURL());
}

// Ensure that we can go back and forward even if a SwapOut ACK isn't received.
// See http://crbug.com/93427.
TEST_F(RenderFrameHostManagerTest, NavigateAfterMissingSwapOutACK) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to two pages.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();

  // Keep active_frame_count nonzero so that no swapped out frames in
  // this SiteInstance get forcefully deleted.
  rfh1->GetSiteInstance()->increment_active_frame_count();

  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  rfh2->GetSiteInstance()->increment_active_frame_count();

  // Now go back, but suppose the SwapOut_ACK isn't received.  This shouldn't
  // happen, but we have seen it when going back quickly across many entries
  // (http://crbug.com/93427).
  contents()->GetController().GoBack();
  EXPECT_TRUE(rfh2->IsWaitingForBeforeUnloadACK());
  contents()->GetMainFrame()->PrepareForCommit();
  EXPECT_FALSE(rfh2->IsWaitingForBeforeUnloadACK());

  // The back navigation commits.
  const NavigationEntry* entry1 = contents()->GetController().GetPendingEntry();
  rfh1->SendNavigate(entry1->GetPageID(), entry1->GetURL());
  EXPECT_TRUE(rfh2->IsWaitingForUnloadACK());
  EXPECT_EQ(RenderFrameHostImpl::STATE_PENDING_SWAP_OUT, rfh2->rfh_state());

  // We should be able to navigate forward.
  contents()->GetController().GoForward();
  contents()->GetMainFrame()->PrepareForCommit();
  const NavigationEntry* entry2 = contents()->GetController().GetPendingEntry();
  rfh2->SendNavigate(entry2->GetPageID(), entry2->GetURL());
  EXPECT_EQ(rfh2, main_test_rfh());
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, rfh2->rfh_state());
  EXPECT_EQ(RenderFrameHostImpl::STATE_PENDING_SWAP_OUT, rfh1->rfh_state());
  rfh1->OnSwappedOut();
  EXPECT_TRUE(rfh1->is_swapped_out());
  EXPECT_EQ(RenderFrameHostImpl::STATE_SWAPPED_OUT, rfh1->rfh_state());
}

// Test that we create swapped out RFHs for the opener chain when navigating an
// opened tab cross-process.  This allows us to support certain cross-process
// JavaScript calls (http://crbug.com/99202).
TEST_F(RenderFrameHostManagerTest, CreateSwappedOutOpenerRFHs) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");
  const GURL kChromeUrl("chrome://foo");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  RenderFrameHostManager* manager = contents()->GetRenderManagerForTesting();
  TestRenderFrameHost* rfh1 = main_test_rfh();
  TestRenderViewHost* rvh1 = test_rvh();

  // Create 2 new tabs and simulate them being the opener chain for the main
  // tab.  They should be in the same SiteInstance.
  scoped_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), rfh1->GetSiteInstance()));
  RenderFrameHostManager* opener1_manager =
      opener1->GetRenderManagerForTesting();
  contents()->SetOpener(opener1.get());

  scoped_ptr<TestWebContents> opener2(
      TestWebContents::Create(browser_context(), rfh1->GetSiteInstance()));
  RenderFrameHostManager* opener2_manager =
      opener2->GetRenderManagerForTesting();
  opener1->SetOpener(opener2.get());

  // Navigate to a cross-site URL (different SiteInstance but same
  // BrowsingInstance).
  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  TestRenderViewHost* rvh2 = test_rvh();
  EXPECT_NE(rfh1->GetSiteInstance(), rfh2->GetSiteInstance());
  EXPECT_TRUE(rfh1->GetSiteInstance()->IsRelatedSiteInstance(
                  rfh2->GetSiteInstance()));

  // Ensure rvh1 is placed on swapped out list of the current tab.
  EXPECT_TRUE(manager->IsOnSwappedOutList(rfh1));
  EXPECT_TRUE(manager->IsRVHOnSwappedOutList(rvh1));
  EXPECT_EQ(rfh1,
            manager->GetRenderFrameProxyHost(rfh1->GetSiteInstance())
                ->render_frame_host());
  EXPECT_EQ(rvh1,
            manager->GetSwappedOutRenderViewHost(rvh1->GetSiteInstance()));

  // Ensure a swapped out RFH and RFH is created in the first opener tab.
  RenderFrameProxyHost* opener1_proxy =
      opener1_manager->GetRenderFrameProxyHost(rfh2->GetSiteInstance());
  RenderFrameHostImpl* opener1_rfh = opener1_proxy->render_frame_host();
  TestRenderViewHost* opener1_rvh = static_cast<TestRenderViewHost*>(
      opener1_manager->GetSwappedOutRenderViewHost(rvh2->GetSiteInstance()));
  EXPECT_TRUE(opener1_manager->IsOnSwappedOutList(opener1_rfh));
  EXPECT_TRUE(opener1_manager->IsRVHOnSwappedOutList(opener1_rvh));
  EXPECT_TRUE(opener1_rfh->is_swapped_out());
  EXPECT_FALSE(opener1_rvh->is_active());

  // Ensure a swapped out RFH and RVH is created in the second opener tab.
  RenderFrameProxyHost* opener2_proxy =
      opener2_manager->GetRenderFrameProxyHost(rfh2->GetSiteInstance());
  RenderFrameHostImpl* opener2_rfh = opener2_proxy->render_frame_host();
  TestRenderViewHost* opener2_rvh = static_cast<TestRenderViewHost*>(
      opener2_manager->GetSwappedOutRenderViewHost(rvh2->GetSiteInstance()));
  EXPECT_TRUE(opener2_manager->IsOnSwappedOutList(opener2_rfh));
  EXPECT_TRUE(opener2_manager->IsRVHOnSwappedOutList(opener2_rvh));
  EXPECT_TRUE(opener2_rfh->is_swapped_out());
  EXPECT_FALSE(opener2_rvh->is_active());

  // Navigate to a cross-BrowsingInstance URL.
  contents()->NavigateAndCommit(kChromeUrl);
  TestRenderFrameHost* rfh3 = main_test_rfh();
  EXPECT_NE(rfh1->GetSiteInstance(), rfh3->GetSiteInstance());
  EXPECT_FALSE(rfh1->GetSiteInstance()->IsRelatedSiteInstance(
                   rfh3->GetSiteInstance()));

  // No scripting is allowed across BrowsingInstances, so we should not create
  // swapped out RVHs for the opener chain in this case.
  EXPECT_FALSE(opener1_manager->GetRenderFrameProxyHost(
                   rfh3->GetSiteInstance()));
  EXPECT_FALSE(opener1_manager->GetSwappedOutRenderViewHost(
                   rfh3->GetSiteInstance()));
  EXPECT_FALSE(opener2_manager->GetRenderFrameProxyHost(
                   rfh3->GetSiteInstance()));
  EXPECT_FALSE(opener2_manager->GetSwappedOutRenderViewHost(
                   rfh3->GetSiteInstance()));
}

// Test that a page can disown the opener of the WebContents.
TEST_F(RenderFrameHostManagerTest, DisownOpener) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();

  // Create a new tab and simulate having it be the opener for the main tab.
  scoped_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), rfh1->GetSiteInstance()));
  contents()->SetOpener(opener1.get());
  EXPECT_TRUE(contents()->HasOpener());

  // Navigate to a cross-site URL (different SiteInstance but same
  // BrowsingInstance).
  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  EXPECT_NE(rfh1->GetSiteInstance(), rfh2->GetSiteInstance());

  // Disown the opener from rfh2.
  rfh2->DidDisownOpener();

  // Ensure the opener is cleared.
  EXPECT_FALSE(contents()->HasOpener());
}

// Test that a page can disown a same-site opener of the WebContents.
TEST_F(RenderFrameHostManagerTest, DisownSameSiteOpener) {
  const GURL kUrl1("http://www.google.com/");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();

  // Create a new tab and simulate having it be the opener for the main tab.
  scoped_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), rfh1->GetSiteInstance()));
  contents()->SetOpener(opener1.get());
  EXPECT_TRUE(contents()->HasOpener());

  // Disown the opener from rfh1.
  rfh1->DidDisownOpener();

  // Ensure the opener is cleared even if it is in the same process.
  EXPECT_FALSE(contents()->HasOpener());
}

// Test that a page can disown the opener just as a cross-process navigation is
// in progress.
TEST_F(RenderFrameHostManagerTest, DisownOpenerDuringNavigation) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();

  // Create a new tab and simulate having it be the opener for the main tab.
  scoped_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), rfh1->GetSiteInstance()));
  contents()->SetOpener(opener1.get());
  EXPECT_TRUE(contents()->HasOpener());

  // Navigate to a cross-site URL (different SiteInstance but same
  // BrowsingInstance).
  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  EXPECT_NE(rfh1->GetSiteInstance(), rfh2->GetSiteInstance());

  // Start a back navigation so that rfh1 becomes the pending RFH.
  contents()->GetController().GoBack();
  contents()->GetMainFrame()->PrepareForCommit();

  // Disown the opener from rfh2.
  rfh2->DidDisownOpener();

  // Ensure the opener is cleared.
  EXPECT_FALSE(contents()->HasOpener());

  // The back navigation commits.
  const NavigationEntry* entry1 = contents()->GetController().GetPendingEntry();
  rfh1->SendNavigate(entry1->GetPageID(), entry1->GetURL());

  // Ensure the opener is still cleared.
  EXPECT_FALSE(contents()->HasOpener());
}

// Test that a page can disown the opener just after a cross-process navigation
// commits.
TEST_F(RenderFrameHostManagerTest, DisownOpenerAfterNavigation) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();

  // Create a new tab and simulate having it be the opener for the main tab.
  scoped_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), rfh1->GetSiteInstance()));
  contents()->SetOpener(opener1.get());
  EXPECT_TRUE(contents()->HasOpener());

  // Navigate to a cross-site URL (different SiteInstance but same
  // BrowsingInstance).
  contents()->NavigateAndCommit(kUrl2);
  TestRenderFrameHost* rfh2 = main_test_rfh();
  EXPECT_NE(rfh1->GetSiteInstance(), rfh2->GetSiteInstance());

  // Commit a back navigation before the DidDisownOpener message arrives.
  // rfh1 will be kept alive because of the opener tab.
  contents()->GetController().GoBack();
  contents()->GetMainFrame()->PrepareForCommit();
  const NavigationEntry* entry1 = contents()->GetController().GetPendingEntry();
  rfh1->SendNavigate(entry1->GetPageID(), entry1->GetURL());

  // Disown the opener from rfh2.
  rfh2->DidDisownOpener();
  EXPECT_FALSE(contents()->HasOpener());
}

// Test that we clean up swapped out RenderViewHosts when a process hosting
// those associated RenderViews crashes. http://crbug.com/258993
TEST_F(RenderFrameHostManagerTest, CleanUpSwappedOutRVHOnProcessCrash) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderViewHost* rvh1 = test_rvh();

  // Create a new tab as an opener for the main tab.
  scoped_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), rvh1->GetSiteInstance()));
  RenderFrameHostManager* opener1_manager =
      opener1->GetRenderManagerForTesting();
  contents()->SetOpener(opener1.get());

  // Make sure the new opener RVH is considered live.
  opener1_manager->current_host()->CreateRenderView(
      base::string16(), -1, MSG_ROUTING_NONE, -1, false);
  EXPECT_TRUE(opener1_manager->current_host()->IsRenderViewLive());
  EXPECT_TRUE(opener1_manager->current_frame_host()->IsRenderFrameLive());

  // Use a cross-process navigation in the opener to swap out the old RVH.
  EXPECT_FALSE(opener1_manager->GetSwappedOutRenderViewHost(
      rvh1->GetSiteInstance()));
  opener1->NavigateAndCommit(kUrl2);
  EXPECT_TRUE(opener1_manager->GetSwappedOutRenderViewHost(
      rvh1->GetSiteInstance()));

  // Fake a process crash.
  RenderProcessHost::RendererClosedDetails details(
      base::TERMINATION_STATUS_PROCESS_CRASHED,
      0);
  // TODO(nasko): Investigate whether this test can be made more realistic by
  // not faking the notification and just doing the RenderProcessGone. This
  // should also get rid of faking |set_render_view_created()| call below.
  NotificationService::current()->Notify(
      NOTIFICATION_RENDERER_PROCESS_CLOSED,
      Source<RenderProcessHost>(rvh1->GetProcess()),
      Details<RenderProcessHost::RendererClosedDetails>(&details));
  rvh1->set_render_view_created(false);

  // Ensure that the RenderFrameProxyHost stays around and the RenderFrameProxy
  // is deleted.
  RenderFrameProxyHost* render_frame_proxy_host =
      opener1_manager->GetRenderFrameProxyHost(rvh1->GetSiteInstance());
  EXPECT_TRUE(render_frame_proxy_host);
  EXPECT_FALSE(render_frame_proxy_host->is_render_frame_proxy_live());

  // Expect the swapped out RVH to exist.
  EXPECT_TRUE(opener1_manager->GetSwappedOutRenderViewHost(
                  rvh1->GetSiteInstance()));

  // Reload the initial tab. This should recreate the opener's swapped out RVH
  // in the original SiteInstance.
  contents()->GetController().Reload(true);
  contents()->GetMainFrame()->PrepareForCommit();
  EXPECT_EQ(opener1_manager->GetSwappedOutRenderViewHost(
                rvh1->GetSiteInstance())->GetRoutingID(),
            test_rvh()->opener_route_id());
}

// Test that RenderViewHosts created for WebUI navigations are properly
// granted WebUI bindings even if an unprivileged swapped out RenderViewHost
// is in the same process (http://crbug.com/79918).
TEST_F(RenderFrameHostManagerTest, EnableWebUIWithSwappedOutOpener) {
  set_should_create_webui(true);
  const GURL kSettingsUrl("chrome://chrome/settings");
  const GURL kPluginUrl("chrome://plugins");

  // Navigate to an initial WebUI URL.
  contents()->NavigateAndCommit(kSettingsUrl);

  // Ensure the RVH has WebUI bindings.
  TestRenderViewHost* rvh1 = test_rvh();
  EXPECT_TRUE(rvh1->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  // Create a new tab and simulate it being the opener for the main
  // tab.  It should be in the same SiteInstance.
  scoped_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), rvh1->GetSiteInstance()));
  RenderFrameHostManager* opener1_manager =
      opener1->GetRenderManagerForTesting();
  contents()->SetOpener(opener1.get());

  // Navigate to a different WebUI URL (different SiteInstance, same
  // BrowsingInstance).
  contents()->NavigateAndCommit(kPluginUrl);
  TestRenderViewHost* rvh2 = test_rvh();
  EXPECT_NE(rvh1->GetSiteInstance(), rvh2->GetSiteInstance());
  EXPECT_TRUE(rvh1->GetSiteInstance()->IsRelatedSiteInstance(
                  rvh2->GetSiteInstance()));

  // Ensure a swapped out RFH and RVH is created in the first opener tab.
  RenderFrameProxyHost* opener1_proxy =
      opener1_manager->GetRenderFrameProxyHost(rvh2->GetSiteInstance());
  RenderFrameHostImpl* opener1_rfh = opener1_proxy->render_frame_host();
  TestRenderViewHost* opener1_rvh = static_cast<TestRenderViewHost*>(
      opener1_manager->GetSwappedOutRenderViewHost(rvh2->GetSiteInstance()));
  EXPECT_TRUE(opener1_manager->IsOnSwappedOutList(opener1_rfh));
  EXPECT_TRUE(opener1_manager->IsRVHOnSwappedOutList(opener1_rvh));
  EXPECT_TRUE(opener1_rfh->is_swapped_out());
  EXPECT_FALSE(opener1_rvh->is_active());

  // Ensure the new RVH has WebUI bindings.
  EXPECT_TRUE(rvh2->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);
}

// Test that we reuse the same guest SiteInstance if we navigate across sites.
TEST_F(RenderFrameHostManagerTest, NoSwapOnGuestNavigations) {
  TestNotificationTracker notifications;

  GURL guest_url(std::string(kGuestScheme).append("://abc123"));
  SiteInstance* instance =
      SiteInstance::CreateForURL(browser_context(), guest_url);
  scoped_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), instance));

  RenderFrameHostManager* manager = web_contents->GetRenderManagerForTesting();

  RenderFrameHostImpl* host = NULL;

  // 1) The first navigation. --------------------------
  const GURL kUrl1("http://www.google.com/");
  NavigationEntryImpl entry1(
      NULL /* instance */, -1 /* page_id */, kUrl1, Referrer(),
      base::string16() /* title */, ui::PAGE_TRANSITION_TYPED,
      false /* is_renderer_init */);
  host = manager->Navigate(entry1);

  // The RenderFrameHost created in Init will be reused.
  EXPECT_TRUE(host == manager->current_frame_host());
  EXPECT_FALSE(manager->pending_frame_host());
  EXPECT_EQ(manager->current_frame_host()->GetSiteInstance(), instance);

  // Commit.
  manager->DidNavigateFrame(host, true);
  // Commit to SiteInstance should be delayed until RenderFrame commit.
  EXPECT_EQ(host, manager->current_frame_host());
  ASSERT_TRUE(host);
  EXPECT_TRUE(host->GetSiteInstance()->HasSite());

  // 2) Navigate to a different domain. -------------------------
  // Guests stay in the same process on navigation.
  const GURL kUrl2("http://www.chromium.org");
  NavigationEntryImpl entry2(
      NULL /* instance */, -1 /* page_id */, kUrl2,
      Referrer(kUrl1, blink::WebReferrerPolicyDefault),
      base::string16() /* title */, ui::PAGE_TRANSITION_LINK,
      true /* is_renderer_init */);
  host = manager->Navigate(entry2);

  // The RenderFrameHost created in Init will be reused.
  EXPECT_EQ(host, manager->current_frame_host());
  EXPECT_FALSE(manager->pending_frame_host());

  // Commit.
  manager->DidNavigateFrame(host, true);
  EXPECT_EQ(host, manager->current_frame_host());
  ASSERT_TRUE(host);
  EXPECT_EQ(host->GetSiteInstance(), instance);
}

// Test that we cancel a pending RVH if we close the tab while it's pending.
// http://crbug.com/294697.
TEST_F(RenderFrameHostManagerTest, NavigateWithEarlyClose) {
  TestNotificationTracker notifications;

  SiteInstance* instance = SiteInstance::Create(browser_context());

  BeforeUnloadFiredWebContentsDelegate delegate;
  scoped_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), instance));
  web_contents->SetDelegate(&delegate);
  notifications.ListenFor(NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                          Source<WebContents>(web_contents.get()));

  RenderFrameHostManager* manager = web_contents->GetRenderManagerForTesting();

  // 1) The first navigation. --------------------------
  const GURL kUrl1("http://www.google.com/");
  NavigationEntryImpl entry1(NULL /* instance */, -1 /* page_id */, kUrl1,
                             Referrer(), base::string16() /* title */,
                             ui::PAGE_TRANSITION_TYPED,
                             false /* is_renderer_init */);
  RenderFrameHostImpl* host = manager->Navigate(entry1);

  // The RenderFrameHost created in Init will be reused.
  EXPECT_EQ(host, manager->current_frame_host());
  EXPECT_FALSE(manager->pending_frame_host());

  // We should observe a notification.
  EXPECT_TRUE(
      notifications.Check1AndReset(NOTIFICATION_RENDER_VIEW_HOST_CHANGED));
  notifications.Reset();

  // Commit.
  manager->DidNavigateFrame(host, true);

  // Commit to SiteInstance should be delayed until RenderFrame commits.
  EXPECT_EQ(host, manager->current_frame_host());
  EXPECT_FALSE(host->GetSiteInstance()->HasSite());
  host->GetSiteInstance()->SetSite(kUrl1);

  // 2) Cross-site navigate to next site. -------------------------
  const GURL kUrl2("http://www.example.com");
  NavigationEntryImpl entry2(
      NULL /* instance */, -1 /* page_id */, kUrl2, Referrer(),
      base::string16() /* title */, ui::PAGE_TRANSITION_TYPED,
      false /* is_renderer_init */);
  RenderFrameHostImpl* host2 = manager->Navigate(entry2);

  // A new RenderFrameHost should be created.
  ASSERT_EQ(host2, manager->pending_frame_host());
  EXPECT_NE(host2, host);

  EXPECT_EQ(host, manager->current_frame_host());
  EXPECT_FALSE(manager->current_frame_host()->is_swapped_out());
  EXPECT_EQ(host2, manager->pending_frame_host());

  // 3) Close the tab. -------------------------
  notifications.ListenFor(NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
                          Source<RenderWidgetHost>(host2->render_view_host()));
  manager->OnBeforeUnloadACK(false, true, base::TimeTicks());

  EXPECT_TRUE(
      notifications.Check1AndReset(NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED));
  EXPECT_FALSE(manager->pending_frame_host());
  EXPECT_EQ(host, manager->current_frame_host());
}

TEST_F(RenderFrameHostManagerTest, CloseWithPendingWhileUnresponsive) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  CloseWebContentsDelegate close_delegate;
  contents()->SetDelegate(&close_delegate);

  // Navigate to the first page.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = contents()->GetMainFrame();

  // Start to close the tab, but assume it's unresponsive.
  rfh1->render_view_host()->ClosePage();
  EXPECT_TRUE(rfh1->render_view_host()->is_waiting_for_close_ack());

  // Start a navigation to a new site.
  controller().LoadURL(
      kUrl2, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableBrowserSideNavigation)) {
    rfh1->PrepareForCommit();
  }
  EXPECT_TRUE(contents()->cross_navigation_pending());

  // Simulate the unresponsiveness timer.  The tab should close.
  contents()->RendererUnresponsive(rfh1->render_view_host());
  EXPECT_TRUE(close_delegate.is_closed());
}

// Tests that the RenderFrameHost is properly deleted when the SwapOutACK is
// received.  (SwapOut and the corresponding ACK always occur after commit.)
// Also tests that an early SwapOutACK is properly ignored.
TEST_F(RenderFrameHostManagerTest, DeleteFrameAfterSwapOutACK) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to the first page.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = contents()->GetMainFrame();
  RenderFrameHostDeletedObserver rfh_deleted_observer(rfh1);
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, rfh1->rfh_state());

  // Navigate to new site, simulating onbeforeunload approval.
  controller().LoadURL(
      kUrl2, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  contents()->GetMainFrame()->PrepareForCommit();
  EXPECT_TRUE(contents()->cross_navigation_pending());
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, rfh1->rfh_state());
  TestRenderFrameHost* rfh2 = contents()->GetPendingMainFrame();

  // Simulate the swap out ack, unexpectedly early (before commit).  It should
  // have no effect.
  rfh1->OnSwappedOut();
  EXPECT_TRUE(contents()->cross_navigation_pending());
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, rfh1->rfh_state());

  // The new page commits.
  contents()->TestDidNavigate(rfh2, 1, kUrl2, ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(rfh2, contents()->GetMainFrame());
  EXPECT_TRUE(contents()->GetPendingMainFrame() == NULL);
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, rfh2->rfh_state());
  EXPECT_EQ(RenderFrameHostImpl::STATE_PENDING_SWAP_OUT, rfh1->rfh_state());
  EXPECT_TRUE(
      rfh1->frame_tree_node()->render_manager()->IsPendingDeletion(rfh1));

  // Simulate the swap out ack.
  rfh1->OnSwappedOut();

  // rfh1 should have been deleted.
  EXPECT_TRUE(rfh_deleted_observer.deleted());
  rfh1 = NULL;
}

// Tests that the RenderFrameHost is properly swapped out when the SwapOut ACK
// is received.  (SwapOut and the corresponding ACK always occur after commit.)
TEST_F(RenderFrameHostManagerTest, SwapOutFrameAfterSwapOutACK) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to the first page.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = contents()->GetMainFrame();
  RenderFrameHostDeletedObserver rfh_deleted_observer(rfh1);
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, rfh1->rfh_state());

  // Increment the number of active frames in SiteInstanceImpl so that rfh1 is
  // not deleted on swap out.
  rfh1->GetSiteInstance()->increment_active_frame_count();

  // Navigate to new site, simulating onbeforeunload approval.
  controller().LoadURL(
      kUrl2, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  contents()->GetMainFrame()->PrepareForCommit();
  EXPECT_TRUE(contents()->cross_navigation_pending());
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, rfh1->rfh_state());
  TestRenderFrameHost* rfh2 = contents()->GetPendingMainFrame();

  // The new page commits.
  contents()->TestDidNavigate(rfh2, 1, kUrl2, ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(rfh2, contents()->GetMainFrame());
  EXPECT_TRUE(contents()->GetPendingMainFrame() == NULL);
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, rfh2->rfh_state());
  EXPECT_EQ(RenderFrameHostImpl::STATE_PENDING_SWAP_OUT, rfh1->rfh_state());

  // Simulate the swap out ack.
  rfh1->OnSwappedOut();

  // rfh1 should be swapped out.
  EXPECT_FALSE(rfh_deleted_observer.deleted());
  EXPECT_TRUE(rfh1->is_swapped_out());
}

// Test that the RenderViewHost is properly swapped out if a navigation in the
// new renderer commits before sending the SwapOut message to the old renderer.
// This simulates a cross-site navigation to a synchronously committing URL
// (e.g., a data URL) and ensures it works properly.
TEST_F(RenderFrameHostManagerTest,
       CommitNewNavigationBeforeSendingSwapOut) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to the first page.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = contents()->GetMainFrame();
  RenderFrameHostDeletedObserver rfh_deleted_observer(rfh1);
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, rfh1->rfh_state());

  // Increment the number of active frames in SiteInstanceImpl so that rfh1 is
  // not deleted on swap out.
  rfh1->GetSiteInstance()->increment_active_frame_count();

  // Navigate to new site, simulating onbeforeunload approval.
  controller().LoadURL(
      kUrl2, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  rfh1->PrepareForCommit();
  EXPECT_TRUE(contents()->cross_navigation_pending());
  TestRenderFrameHost* rfh2 = contents()->GetPendingMainFrame();

  // The new page commits.
  contents()->TestDidNavigate(rfh2, 1, kUrl2, ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(rfh2, contents()->GetMainFrame());
  EXPECT_TRUE(contents()->GetPendingMainFrame() == NULL);
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, rfh2->rfh_state());
  EXPECT_EQ(RenderFrameHostImpl::STATE_PENDING_SWAP_OUT, rfh1->rfh_state());

  // Simulate the swap out ack.
  rfh1->OnSwappedOut();

  // rfh1 should be swapped out.
  EXPECT_FALSE(rfh_deleted_observer.deleted());
  EXPECT_TRUE(rfh1->is_swapped_out());
}

// Test that a RenderFrameHost is properly deleted or swapped out when a
// cross-site navigation is cancelled.
TEST_F(RenderFrameHostManagerTest,
       CancelPendingProperlyDeletesOrSwaps) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");
  RenderFrameHostImpl* pending_rfh = NULL;
  base::TimeTicks now = base::TimeTicks::Now();

  // Navigate to the first page.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, rfh1->rfh_state());

  // Navigate to a new site, starting a cross-site navigation.
  controller().LoadURL(
      kUrl2, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  {
    pending_rfh = contents()->GetFrameTree()->root()->render_manager()
        ->pending_frame_host();
    RenderFrameHostDeletedObserver rfh_deleted_observer(pending_rfh);

    // Cancel the navigation by simulating a declined beforeunload dialog.
    contents()->GetMainFrame()->OnMessageReceived(
        FrameHostMsg_BeforeUnload_ACK(0, false, now, now));
    EXPECT_FALSE(contents()->cross_navigation_pending());

    // Since the pending RFH is the only one for the new SiteInstance, it should
    // be deleted.
    EXPECT_TRUE(rfh_deleted_observer.deleted());
  }

  // Start another cross-site navigation.
  controller().LoadURL(
      kUrl2, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  {
    pending_rfh = contents()->GetFrameTree()->root()->render_manager()
        ->pending_frame_host();
    RenderFrameHostDeletedObserver rfh_deleted_observer(pending_rfh);

    // Increment the number of active frames in the new SiteInstance, which will
    // cause the pending RFH to be swapped out instead of deleted.
    pending_rfh->GetSiteInstance()->increment_active_frame_count();

    contents()->GetMainFrame()->OnMessageReceived(
        FrameHostMsg_BeforeUnload_ACK(0, false, now, now));
    EXPECT_FALSE(contents()->cross_navigation_pending());
    EXPECT_FALSE(rfh_deleted_observer.deleted());
  }
}

// Test that a pending RenderFrameHost in a non-root frame tree node is properly
// deleted when the node is detached. Motivated by http://crbug.com/441357
TEST_F(RenderFrameHostManagerTest, DetachPendingChild) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSitePerProcess);

  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://webkit.org/");

  RenderFrameHostImpl* host = NULL;

  contents()->NavigateAndCommit(kUrl1);
  contents()->GetMainFrame()->OnCreateChildFrame(
      contents()->GetMainFrame()->GetProcess()->GetNextRoutingID(),
      std::string("frame_name"), SandboxFlags::NONE);
  RenderFrameHostManager* manager =
      contents()->GetFrameTree()->root()->child_at(0)->render_manager();

  // 1) The first navigation. --------------------------
  NavigationEntryImpl entry1(NULL /* instance */, -1 /* page_id */, kUrl1,
                             Referrer(), base::string16() /* title */,
                             ui::PAGE_TRANSITION_TYPED,
                             false /* is_renderer_init */);
  host = GetFrameHostForNavigation(manager, entry1);

  // The RenderFrameHost created in Init will be reused.
  EXPECT_TRUE(host == manager->current_frame_host());
  EXPECT_FALSE(GetPendingFrameHost(manager));

  // Commit.
  manager->DidNavigateFrame(host, true);
  // Commit to SiteInstance should be delayed until RenderFrame commit.
  EXPECT_TRUE(host == manager->current_frame_host());
  ASSERT_TRUE(host);
  EXPECT_TRUE(host->GetSiteInstance()->HasSite());

  // 2) Cross-site navigate to next site. --------------
  NavigationEntryImpl entry2(NULL /* instance */, -1 /* page_id */, kUrl2,
                             Referrer(kUrl1, blink::WebReferrerPolicyDefault),
                             base::string16() /* title */,
                             ui::PAGE_TRANSITION_LINK,
                             false /* is_renderer_init */);
  host = GetFrameHostForNavigation(manager, entry2);

  // A new RenderFrameHost should be created.
  EXPECT_TRUE(GetPendingFrameHost(manager));
  ASSERT_EQ(host, GetPendingFrameHost(manager));
  ASSERT_NE(manager->current_frame_host(), GetPendingFrameHost(manager));
  EXPECT_FALSE(contents()->cross_navigation_pending())
      << "There should be no top-level pending navigation.";

  RenderFrameHostDeletedObserver delete_watcher(GetPendingFrameHost(manager));
  EXPECT_FALSE(delete_watcher.deleted());

  // Extend the lifetime of the child frame's SiteInstance, pretending
  // that there is another reference to it.
  scoped_refptr<SiteInstanceImpl> site_instance =
      GetPendingFrameHost(manager)->GetSiteInstance();
  EXPECT_TRUE(site_instance->HasSite());
  EXPECT_NE(site_instance, contents()->GetSiteInstance());
  EXPECT_EQ(1U, site_instance->active_frame_count());
  site_instance->increment_active_frame_count();
  EXPECT_EQ(2U, site_instance->active_frame_count());

  // Now detach the child FrameTreeNode. This should kill the pending host.
  manager->current_frame_host()->OnMessageReceived(
      FrameHostMsg_Detach(manager->current_frame_host()->GetRoutingID()));

  EXPECT_TRUE(delete_watcher.deleted());

  EXPECT_EQ(1U, site_instance->active_frame_count());
  site_instance->decrement_active_frame_count();

#if 0
  // TODO(nick): Currently a proxy to the removed frame lingers in the parent.
  // Enable this assert below once the proxies to the subframe are correctly
  // cleaned up after detach. http://crbug.com/444955.
  ASSERT_TRUE(site_instance->HasOneRef())
      << "This SiteInstance should be destroyable now.";
#endif
}

}  // namespace content
