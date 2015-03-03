// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/frame_host/cross_site_transferring_request.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/frame_messages.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/view_messages.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const char kTestWebUIUrl[] = "chrome://blah";

class WebContentsImplTestWebUIControllerFactory
    : public WebUIControllerFactory {
 public:
  WebUIController* CreateWebUIControllerForURL(WebUI* web_ui,
                                               const GURL& url) const override {
    if (!UseWebUI(url))
      return NULL;
    return new WebUIController(web_ui);
  }

  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) const override {
    return WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) const override {
    return UseWebUI(url);
  }

  bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                              const GURL& url) const override {
    return UseWebUI(url);
  }

 private:
  bool UseWebUI(const GURL& url) const {
    return url == GURL(kTestWebUIUrl);
  }
};

class TestInterstitialPage;

class TestInterstitialPageDelegate : public InterstitialPageDelegate {
 public:
  explicit TestInterstitialPageDelegate(TestInterstitialPage* interstitial_page)
      : interstitial_page_(interstitial_page) {}
  void CommandReceived(const std::string& command) override;
  std::string GetHTMLContents() override { return std::string(); }
  void OnDontProceed() override;
  void OnProceed() override;

 private:
  TestInterstitialPage* interstitial_page_;
};

class TestInterstitialPage : public InterstitialPageImpl {
 public:
  enum InterstitialState {
    INVALID = 0,    // Hasn't yet been initialized.
    UNDECIDED,      // Initialized, but no decision taken yet.
    OKED,           // Proceed was called.
    CANCELED        // DontProceed was called.
  };

  class Delegate {
   public:
    virtual void TestInterstitialPageDeleted(
        TestInterstitialPage* interstitial) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // IMPORTANT NOTE: if you pass stack allocated values for |state| and
  // |deleted| (like all interstitial related tests do at this point), make sure
  // to create an instance of the TestInterstitialPageStateGuard class on the
  // stack in your test.  This will ensure that the TestInterstitialPage states
  // are cleared when the test finishes.
  // Not doing so will cause stack trashing if your test does not hide the
  // interstitial, as in such a case it will be destroyed in the test TearDown
  // method and will dereference the |deleted| local variable which by then is
  // out of scope.
  TestInterstitialPage(WebContentsImpl* contents,
                       bool new_navigation,
                       const GURL& url,
                       InterstitialState* state,
                       bool* deleted)
      : InterstitialPageImpl(
            contents,
            static_cast<RenderWidgetHostDelegate*>(contents),
            new_navigation, url, new TestInterstitialPageDelegate(this)),
        state_(state),
        deleted_(deleted),
        command_received_count_(0),
        delegate_(NULL) {
    *state_ = UNDECIDED;
    *deleted_ = false;
  }

  ~TestInterstitialPage() override {
    if (deleted_)
      *deleted_ = true;
    if (delegate_)
      delegate_->TestInterstitialPageDeleted(this);
  }

  void OnDontProceed() {
    if (state_)
      *state_ = CANCELED;
  }
  void OnProceed() {
    if (state_)
      *state_ = OKED;
  }

  int command_received_count() const {
    return command_received_count_;
  }

  void TestDomOperationResponse(const std::string& json_string) {
    if (enabled())
      CommandReceived();
  }

  void TestDidNavigate(int page_id, const GURL& url) {
    FrameHostMsg_DidCommitProvisionalLoad_Params params;
    InitNavigateParams(&params, page_id, url, ui::PAGE_TRANSITION_TYPED);
    DidNavigate(GetMainFrame()->GetRenderViewHost(), params);
  }

  void TestRenderViewTerminated(base::TerminationStatus status,
                                int error_code) {
    RenderViewTerminated(GetMainFrame()->GetRenderViewHost(), status,
                         error_code);
  }

  bool is_showing() const {
    return static_cast<TestRenderWidgetHostView*>(
               GetMainFrame()->GetRenderViewHost()->GetView())->is_showing();
  }

  void ClearStates() {
    state_ = NULL;
    deleted_ = NULL;
    delegate_ = NULL;
  }

  void CommandReceived() {
    command_received_count_++;
  }

  void set_delegate(Delegate* delegate) {
    delegate_ = delegate;
  }

 protected:
  WebContentsView* CreateWebContentsView() override { return NULL; }

 private:
  InterstitialState* state_;
  bool* deleted_;
  int command_received_count_;
  Delegate* delegate_;
};

void TestInterstitialPageDelegate::CommandReceived(const std::string& command) {
  interstitial_page_->CommandReceived();
}

void TestInterstitialPageDelegate::OnDontProceed() {
  interstitial_page_->OnDontProceed();
}

void TestInterstitialPageDelegate::OnProceed() {
  interstitial_page_->OnProceed();
}

class TestInterstitialPageStateGuard : public TestInterstitialPage::Delegate {
 public:
  explicit TestInterstitialPageStateGuard(
      TestInterstitialPage* interstitial_page)
      : interstitial_page_(interstitial_page) {
    DCHECK(interstitial_page_);
    interstitial_page_->set_delegate(this);
  }
  ~TestInterstitialPageStateGuard() override {
    if (interstitial_page_)
      interstitial_page_->ClearStates();
  }

  void TestInterstitialPageDeleted(
      TestInterstitialPage* interstitial) override {
    DCHECK(interstitial_page_ == interstitial);
    interstitial_page_ = NULL;
  }

 private:
  TestInterstitialPage* interstitial_page_;
};

class WebContentsImplTestBrowserClient : public TestContentBrowserClient {
 public:
  WebContentsImplTestBrowserClient()
      : assign_site_for_url_(false) {}

  ~WebContentsImplTestBrowserClient() override {}

  bool ShouldAssignSiteForURL(const GURL& url) override {
    return assign_site_for_url_;
  }

  void set_assign_site_for_url(bool assign) {
    assign_site_for_url_ = assign;
  }

 private:
  bool assign_site_for_url_;
};

class WebContentsImplTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    WebUIControllerFactory::RegisterFactory(&factory_);
  }

  void TearDown() override {
    WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
    RenderViewHostImplTestHarness::TearDown();
  }

 private:
  WebContentsImplTestWebUIControllerFactory factory_;
};

class TestWebContentsObserver : public WebContentsObserver {
 public:
  explicit TestWebContentsObserver(WebContents* contents)
      : WebContentsObserver(contents) {
  }
  ~TestWebContentsObserver() override {}

  void DidFinishLoad(RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    last_url_ = validated_url;
  }
  void DidFailLoad(RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description) override {
    last_url_ = validated_url;
  }

  const GURL& last_url() const { return last_url_; }

 private:
  GURL last_url_;

  DISALLOW_COPY_AND_ASSIGN(TestWebContentsObserver);
};

// Pretends to be a normal browser that receives toggles and transitions to/from
// a fullscreened state.
class FakeFullscreenDelegate : public WebContentsDelegate {
 public:
  FakeFullscreenDelegate() : fullscreened_contents_(NULL) {}
  ~FakeFullscreenDelegate() override {}

  void EnterFullscreenModeForTab(WebContents* web_contents,
                                 const GURL& origin) override {
    fullscreened_contents_ = web_contents;
  }

  void ExitFullscreenModeForTab(WebContents* web_contents) override {
    fullscreened_contents_ = NULL;
  }

  bool IsFullscreenForTabOrPending(
      const WebContents* web_contents) const override {
    return fullscreened_contents_ && web_contents == fullscreened_contents_;
  }

 private:
  WebContents* fullscreened_contents_;

  DISALLOW_COPY_AND_ASSIGN(FakeFullscreenDelegate);
};

class FakeValidationMessageDelegate : public WebContentsDelegate {
 public:
  FakeValidationMessageDelegate()
      : hide_validation_message_was_called_(false) {}
  ~FakeValidationMessageDelegate() override {}

  void HideValidationMessage(WebContents* web_contents) override {
    hide_validation_message_was_called_ = true;
  }

  bool hide_validation_message_was_called() const {
    return hide_validation_message_was_called_;
  }

 private:
  bool hide_validation_message_was_called_;

  DISALLOW_COPY_AND_ASSIGN(FakeValidationMessageDelegate);
};

}  // namespace

// Test to make sure that title updates get stripped of whitespace.
TEST_F(WebContentsImplTest, UpdateTitle) {
  NavigationControllerImpl& cont =
      static_cast<NavigationControllerImpl&>(controller());
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  InitNavigateParams(
      &params, 0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED);

  LoadCommittedDetails details;
  cont.RendererDidNavigate(contents()->GetMainFrame(), params, &details);

  contents()->UpdateTitle(contents()->GetMainFrame(), 0,
                          base::ASCIIToUTF16("    Lots O' Whitespace\n"),
                          base::i18n::LEFT_TO_RIGHT);
  EXPECT_EQ(base::ASCIIToUTF16("Lots O' Whitespace"), contents()->GetTitle());
}

TEST_F(WebContentsImplTest, DontUseTitleFromPendingEntry) {
  const GURL kGURL("chrome://blah");
  controller().LoadURL(
      kGURL, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(base::string16(), contents()->GetTitle());
}

TEST_F(WebContentsImplTest, UseTitleFromPendingEntryIfSet) {
  const GURL kGURL("chrome://blah");
  const base::string16 title = base::ASCIIToUTF16("My Title");
  controller().LoadURL(
      kGURL, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());

  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_EQ(kGURL, entry->GetURL());
  entry->SetTitle(title);

  EXPECT_EQ(title, contents()->GetTitle());
}

// Test view source mode for a webui page.
TEST_F(WebContentsImplTest, NTPViewSource) {
  NavigationControllerImpl& cont =
      static_cast<NavigationControllerImpl&>(controller());
  const char kUrl[] = "view-source:chrome://blah";
  const GURL kGURL(kUrl);

  process()->sink().ClearMessages();

  cont.LoadURL(
      kGURL, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  rvh()->GetDelegate()->RenderViewCreated(rvh());
  // Did we get the expected message?
  EXPECT_TRUE(process()->sink().GetFirstMessageMatching(
      ViewMsg_EnableViewSourceMode::ID));

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  InitNavigateParams(&params, 0, kGURL, ui::PAGE_TRANSITION_TYPED);
  LoadCommittedDetails details;
  cont.RendererDidNavigate(contents()->GetMainFrame(), params, &details);
  // Also check title and url.
  EXPECT_EQ(base::ASCIIToUTF16(kUrl), contents()->GetTitle());
}

// Test to ensure UpdateMaxPageID is working properly.
TEST_F(WebContentsImplTest, UpdateMaxPageID) {
  SiteInstance* instance1 = contents()->GetSiteInstance();
  scoped_refptr<SiteInstance> instance2(SiteInstance::Create(NULL));

  // Starts at -1.
  EXPECT_EQ(-1, contents()->GetMaxPageID());
  EXPECT_EQ(-1, contents()->GetMaxPageIDForSiteInstance(instance1));
  EXPECT_EQ(-1, contents()->GetMaxPageIDForSiteInstance(instance2.get()));

  // Make sure max_page_id_ is monotonically increasing per SiteInstance.
  contents()->UpdateMaxPageID(3);
  contents()->UpdateMaxPageID(1);
  EXPECT_EQ(3, contents()->GetMaxPageID());
  EXPECT_EQ(3, contents()->GetMaxPageIDForSiteInstance(instance1));
  EXPECT_EQ(-1, contents()->GetMaxPageIDForSiteInstance(instance2.get()));

  contents()->UpdateMaxPageIDForSiteInstance(instance2.get(), 7);
  EXPECT_EQ(3, contents()->GetMaxPageID());
  EXPECT_EQ(3, contents()->GetMaxPageIDForSiteInstance(instance1));
  EXPECT_EQ(7, contents()->GetMaxPageIDForSiteInstance(instance2.get()));
}

// Test simple same-SiteInstance navigation.
TEST_F(WebContentsImplTest, SimpleNavigation) {
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();
  SiteInstance* instance1 = contents()->GetSiteInstance();
  EXPECT_TRUE(contents()->GetPendingMainFrame() == NULL);

  // Navigate to URL
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(instance1, orig_rfh->GetSiteInstance());
  // Controller's pending entry will have a NULL site instance until we assign
  // it in DidNavigate.
  EXPECT_TRUE(
      NavigationEntryImpl::FromNavigationEntry(controller().GetVisibleEntry())->
          site_instance() == NULL);

  // DidNavigate from the page
  contents()->TestDidNavigate(orig_rfh, 1, url, ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rfh, contents()->GetMainFrame());
  EXPECT_EQ(instance1, orig_rfh->GetSiteInstance());
  // Controller's entry should now have the SiteInstance, or else we won't be
  // able to find it later.
  EXPECT_EQ(
      instance1,
      NavigationEntryImpl::FromNavigationEntry(controller().GetVisibleEntry())->
          site_instance());
}

// Test that we reject NavigateToEntry if the url is over kMaxURLChars.
TEST_F(WebContentsImplTest, NavigateToExcessivelyLongURL) {
  // Construct a URL that's kMaxURLChars + 1 long of all 'a's.
  const GURL url(std::string("http://example.org/").append(
      GetMaxURLChars() + 1, 'a'));

  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_GENERATED, std::string());
  EXPECT_TRUE(controller().GetVisibleEntry() == NULL);
}

// Test that navigating across a site boundary creates a new RenderViewHost
// with a new SiteInstance.  Going back should do the same.
TEST_F(WebContentsImplTest, CrossSiteBoundaries) {
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();
  int orig_rvh_delete_count = 0;
  orig_rfh->GetRenderViewHost()->set_delete_counter(&orig_rvh_delete_count);
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  orig_rfh->PrepareForCommit(url);
  contents()->TestDidNavigate(orig_rfh, 1, url, ui::PAGE_TRANSITION_TYPED);

  // Keep the number of active frames in orig_rfh's SiteInstance non-zero so
  // that orig_rfh doesn't get deleted when it gets swapped out.
  orig_rfh->GetSiteInstance()->increment_active_frame_count();

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rfh->GetRenderViewHost(), contents()->GetRenderViewHost());
  EXPECT_EQ(url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url, contents()->GetVisibleURL());

  // Navigate to new site
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation)) {
    orig_rfh->PrepareForCommit(url2);
  }
  EXPECT_TRUE(contents()->cross_navigation_pending());
  EXPECT_EQ(url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  TestRenderFrameHost* pending_rfh = contents()->GetPendingMainFrame();
  int pending_rvh_delete_count = 0;
  pending_rfh->GetRenderViewHost()->set_delete_counter(
      &pending_rvh_delete_count);

  // Navigations should be suspended in pending_rfh until BeforeUnloadACK.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation)) {
    EXPECT_TRUE(pending_rfh->are_navigations_suspended());
    orig_rfh->SendBeforeUnloadACK(true);
    EXPECT_FALSE(pending_rfh->are_navigations_suspended());
  }

  // DidNavigate from the pending page
  pending_rfh->PrepareForCommit(url2);
  contents()->TestDidNavigate(
      pending_rfh, 1, url2, ui::PAGE_TRANSITION_TYPED);
  SiteInstance* instance2 = contents()->GetSiteInstance();

  // Keep the number of active frames in pending_rfh's SiteInstance
  // non-zero so that orig_rfh doesn't get deleted when it gets
  // swapped out.
  pending_rfh->GetSiteInstance()->increment_active_frame_count();

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(pending_rfh, contents()->GetMainFrame());
  EXPECT_EQ(url2, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  EXPECT_NE(instance1, instance2);
  EXPECT_TRUE(contents()->GetPendingMainFrame() == NULL);
  // We keep the original RFH around, swapped out.
  EXPECT_TRUE(contents()->GetRenderManagerForTesting()->IsOnSwappedOutList(
      orig_rfh));
  EXPECT_EQ(orig_rvh_delete_count, 0);

  // Going back should switch SiteInstances again.  The first SiteInstance is
  // stored in the NavigationEntry, so it should be the same as at the start.
  // We should use the same RFH as before, swapping it back in.
  controller().GoBack();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation)) {
    contents()->GetMainFrame()->PrepareForCommit(url);
  }
  TestRenderFrameHost* goback_rfh = contents()->GetPendingMainFrame();
  EXPECT_EQ(orig_rfh, goback_rfh);
  EXPECT_TRUE(contents()->cross_navigation_pending());

  // Navigations should be suspended in goback_rfh until BeforeUnloadACK.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation)) {
    EXPECT_TRUE(goback_rfh->are_navigations_suspended());
    pending_rfh->SendBeforeUnloadACK(true);
    EXPECT_FALSE(goback_rfh->are_navigations_suspended());
  }

  // DidNavigate from the back action
  contents()->TestDidNavigate(goback_rfh, 1, url2, ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(goback_rfh, contents()->GetMainFrame());
  EXPECT_EQ(instance1, contents()->GetSiteInstance());
  // The pending RFH should now be swapped out, not deleted.
  EXPECT_TRUE(contents()->GetRenderManagerForTesting()->
      IsOnSwappedOutList(pending_rfh));
  EXPECT_EQ(pending_rvh_delete_count, 0);
  pending_rfh->OnSwappedOut();

  // Close contents and ensure RVHs are deleted.
  DeleteContents();
  EXPECT_EQ(orig_rvh_delete_count, 1);
  EXPECT_EQ(pending_rvh_delete_count, 1);
}

// Test that navigating across a site boundary after a crash creates a new
// RFH without requiring a cross-site transition (i.e., PENDING state).
TEST_F(WebContentsImplTest, CrossSiteBoundariesAfterCrash) {
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();

  int orig_rvh_delete_count = 0;
  orig_rfh->GetRenderViewHost()->set_delete_counter(&orig_rvh_delete_count);
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->GetMainFrame()->PrepareForCommit(url);
  contents()->TestDidNavigate(orig_rfh, 1, url, ui::PAGE_TRANSITION_TYPED);

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rfh->GetRenderViewHost(), contents()->GetRenderViewHost());

  // Simulate a renderer crash.
  orig_rfh->GetRenderViewHost()->set_render_view_created(false);
  orig_rfh->SetRenderFrameCreated(false);

  // Navigate to new site.  We should not go into PENDING.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->GetMainFrame()->PrepareForCommit(url2);
  TestRenderFrameHost* new_rfh = contents()->GetMainFrame();
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_TRUE(contents()->GetPendingMainFrame() == NULL);
  EXPECT_NE(orig_rfh, new_rfh);
  EXPECT_EQ(orig_rvh_delete_count, 1);

  // DidNavigate from the new page
  contents()->TestDidNavigate(new_rfh, 1, url2, ui::PAGE_TRANSITION_TYPED);
  SiteInstance* instance2 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(new_rfh, main_rfh());
  EXPECT_NE(instance1, instance2);
  EXPECT_TRUE(contents()->GetPendingMainFrame() == NULL);

  // Close contents and ensure RVHs are deleted.
  DeleteContents();
  EXPECT_EQ(orig_rvh_delete_count, 1);
}

// Test that opening a new contents in the same SiteInstance and then navigating
// both contentses to a new site will place both contentses in a single
// SiteInstance.
TEST_F(WebContentsImplTest, NavigateTwoTabsCrossSite) {
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->GetMainFrame()->PrepareForCommit(url);
  contents()->TestDidNavigate(orig_rfh, 1, url, ui::PAGE_TRANSITION_TYPED);

  // Open a new contents with the same SiteInstance, navigated to the same site.
  scoped_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), instance1));
  contents2->GetController().LoadURL(url, Referrer(),
                                     ui::PAGE_TRANSITION_TYPED,
                                     std::string());
  contents2->GetMainFrame()->PrepareForCommit(url);
  // Need this page id to be 2 since the site instance is the same (which is the
  // scope of page IDs) and we want to consider this a new page.
  contents2->TestDidNavigate(
      contents2->GetMainFrame(), 2, url, ui::PAGE_TRANSITION_TYPED);

  // Navigate first contents to a new site.
  const GURL url2a("http://www.yahoo.com");
  controller().LoadURL(
      url2a, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  orig_rfh->PrepareForCommit(url2a);
  TestRenderFrameHost* pending_rfh_a = contents()->GetPendingMainFrame();
  contents()->TestDidNavigate(
      pending_rfh_a, 1, url2a, ui::PAGE_TRANSITION_TYPED);
  SiteInstance* instance2a = contents()->GetSiteInstance();
  EXPECT_NE(instance1, instance2a);

  // Navigate second contents to the same site as the first tab.
  const GURL url2b("http://mail.yahoo.com");
  contents2->GetController().LoadURL(url2b, Referrer(),
                                     ui::PAGE_TRANSITION_TYPED,
                                     std::string());
  TestRenderFrameHost* rfh2 = contents2->GetMainFrame();
  rfh2->PrepareForCommit(url2b);
  TestRenderFrameHost* pending_rfh_b = contents2->GetPendingMainFrame();
  EXPECT_TRUE(pending_rfh_b != NULL);
  EXPECT_TRUE(contents2->cross_navigation_pending());

  // NOTE(creis): We used to be in danger of showing a crash page here if the
  // second contents hadn't navigated somewhere first (bug 1145430).  That case
  // is now covered by the CrossSiteBoundariesAfterCrash test.
  contents2->TestDidNavigate(
      pending_rfh_b, 2, url2b, ui::PAGE_TRANSITION_TYPED);
  SiteInstance* instance2b = contents2->GetSiteInstance();
  EXPECT_NE(instance1, instance2b);

  // Both contentses should now be in the same SiteInstance.
  EXPECT_EQ(instance2a, instance2b);
}

// The embedder can request sites for certain urls not be be assigned to the
// SiteInstance through ShouldAssignSiteForURL() in content browser client,
// allowing to reuse the renderer backing certain chrome urls for subsequent
// navigation. The test verifies that the override is honored.
TEST_F(WebContentsImplTest, NavigateFromSitelessUrl) {
  WebContentsImplTestBrowserClient browser_client;
  SetBrowserClientForTesting(&browser_client);

  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();
  int orig_rvh_delete_count = 0;
  orig_rfh->GetRenderViewHost()->set_delete_counter(&orig_rvh_delete_count);
  SiteInstanceImpl* orig_instance = contents()->GetSiteInstance();

  browser_client.set_assign_site_for_url(false);
  // Navigate to an URL that will not assign a new SiteInstance.
  const GURL native_url("non-site-url://stuffandthings");
  controller().LoadURL(
      native_url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->GetMainFrame()->PrepareForCommit(native_url);
  contents()->TestDidNavigate(
      orig_rfh, 1, native_url, ui::PAGE_TRANSITION_TYPED);

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rfh, contents()->GetMainFrame());
  EXPECT_EQ(native_url, contents()->GetLastCommittedURL());
  EXPECT_EQ(native_url, contents()->GetVisibleURL());
  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());
  EXPECT_EQ(GURL(), contents()->GetSiteInstance()->GetSiteURL());
  EXPECT_FALSE(orig_instance->HasSite());

  browser_client.set_assign_site_for_url(true);
  // Navigate to new site (should keep same site instance).
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->GetMainFrame()->PrepareForCommit(url);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(native_url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url, contents()->GetVisibleURL());
  EXPECT_FALSE(contents()->GetPendingMainFrame());
  contents()->TestDidNavigate(orig_rfh, 1, url, ui::PAGE_TRANSITION_TYPED);

  // Keep the number of active frames in orig_rfh's SiteInstance
  // non-zero so that orig_rfh doesn't get deleted when it gets
  // swapped out.
  orig_rfh->GetSiteInstance()->increment_active_frame_count();

  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());
  EXPECT_TRUE(
      contents()->GetSiteInstance()->GetSiteURL().DomainIs("google.com"));
  EXPECT_EQ(url, contents()->GetLastCommittedURL());

  // Navigate to another new site (should create a new site instance).
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation)) {
    orig_rfh->PrepareForCommit(url2);
  }
  EXPECT_TRUE(contents()->cross_navigation_pending());
  EXPECT_EQ(url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  TestRenderFrameHost* pending_rfh = contents()->GetPendingMainFrame();
  int pending_rvh_delete_count = 0;
  pending_rfh->GetRenderViewHost()->set_delete_counter(
      &pending_rvh_delete_count);

  // Navigations should be suspended in pending_rvh until BeforeUnloadACK.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation)) {
    EXPECT_TRUE(pending_rfh->are_navigations_suspended());
    orig_rfh->SendBeforeUnloadACK(true);
    EXPECT_FALSE(pending_rfh->are_navigations_suspended());
  }

  // DidNavigate from the pending page.
  contents()->TestDidNavigate(
      pending_rfh, 1, url2, ui::PAGE_TRANSITION_TYPED);
  SiteInstance* new_instance = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(pending_rfh, contents()->GetMainFrame());
  EXPECT_EQ(url2, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  EXPECT_NE(new_instance, orig_instance);
  EXPECT_FALSE(contents()->GetPendingMainFrame());
  // We keep the original RFH around, swapped out.
  EXPECT_TRUE(contents()->GetRenderManagerForTesting()->IsOnSwappedOutList(
      orig_rfh));
  EXPECT_EQ(orig_rvh_delete_count, 0);
  orig_rfh->OnSwappedOut();

  // Close contents and ensure RVHs are deleted.
  DeleteContents();
  EXPECT_EQ(orig_rvh_delete_count, 1);
  EXPECT_EQ(pending_rvh_delete_count, 1);
}

// Regression test for http://crbug.com/386542 - variation of
// NavigateFromSitelessUrl in which the original navigation is a session
// restore.
TEST_F(WebContentsImplTest, NavigateFromRestoredSitelessUrl) {
  WebContentsImplTestBrowserClient browser_client;
  SetBrowserClientForTesting(&browser_client);
  SiteInstanceImpl* orig_instance = contents()->GetSiteInstance();
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();

  // Restore a navigation entry for URL that should not assign site to the
  // SiteInstance.
  browser_client.set_assign_site_for_url(false);
  const GURL native_url("non-site-url://stuffandthings");
  std::vector<NavigationEntry*> entries;
  NavigationEntry* entry = NavigationControllerImpl::CreateNavigationEntry(
      native_url, Referrer(), ui::PAGE_TRANSITION_LINK, false, std::string(),
      browser_context());
  entry->SetPageID(0);
  entries.push_back(entry);
  controller().Restore(
      0,
      NavigationController::RESTORE_LAST_SESSION_EXITED_CLEANLY,
      &entries);
  ASSERT_EQ(0u, entries.size());
  ASSERT_EQ(1, controller().GetEntryCount());
  controller().GoToIndex(0);
  contents()->TestDidNavigate(
      orig_rfh, 0, native_url, ui::PAGE_TRANSITION_RELOAD);
  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());
  EXPECT_EQ(GURL(), contents()->GetSiteInstance()->GetSiteURL());
  EXPECT_FALSE(orig_instance->HasSite());

  // Navigate to a regular site and verify that the SiteInstance was kept.
  browser_client.set_assign_site_for_url(true);
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(orig_rfh, 2, url, ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());

  // Cleanup.
  DeleteContents();
}

// Complement for NavigateFromRestoredSitelessUrl, verifying that when a regular
// tab is restored, the SiteInstance will change upon navigation.
TEST_F(WebContentsImplTest, NavigateFromRestoredRegularUrl) {
  WebContentsImplTestBrowserClient browser_client;
  SetBrowserClientForTesting(&browser_client);
  SiteInstanceImpl* orig_instance = contents()->GetSiteInstance();
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();

  // Restore a navigation entry for a regular URL ensuring that the embedder
  // ShouldAssignSiteForUrl override is disabled (i.e. returns true).
  browser_client.set_assign_site_for_url(true);
  const GURL regular_url("http://www.yahoo.com");
  std::vector<NavigationEntry*> entries;
  NavigationEntry* entry = NavigationControllerImpl::CreateNavigationEntry(
      regular_url, Referrer(), ui::PAGE_TRANSITION_LINK, false, std::string(),
      browser_context());
  entry->SetPageID(0);
  entries.push_back(entry);
  controller().Restore(
      0,
      NavigationController::RESTORE_LAST_SESSION_EXITED_CLEANLY,
      &entries);
  ASSERT_EQ(0u, entries.size());
  ASSERT_EQ(1, controller().GetEntryCount());
  controller().GoToIndex(0);
  orig_rfh->PrepareForCommit(regular_url);
  contents()->TestDidNavigate(
      orig_rfh, 0, regular_url, ui::PAGE_TRANSITION_RELOAD);
  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());
  EXPECT_TRUE(orig_instance->HasSite());

  // Navigate to another site and verify that a new SiteInstance was created.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  orig_rfh->PrepareForCommit(url);
  contents()->TestDidNavigate(
      contents()->GetPendingMainFrame(), 2, url, ui::PAGE_TRANSITION_TYPED);
  EXPECT_NE(orig_instance, contents()->GetSiteInstance());

  // Cleanup.
  DeleteContents();
}

// Test that we can find an opener RVH even if it's pending.
// http://crbug.com/176252.
TEST_F(WebContentsImplTest, FindOpenerRVHWhenPending) {
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();

  // Navigate to a URL.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  orig_rfh->PrepareForCommit(url);
  contents()->TestDidNavigate(orig_rfh, 1, url, ui::PAGE_TRANSITION_TYPED);

  // Start to navigate first tab to a new site, so that it has a pending RVH.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  orig_rfh->PrepareForCommit(url2);
  TestRenderFrameHost* pending_rfh = contents()->GetPendingMainFrame();

  // While it is still pending, simulate opening a new tab with the first tab
  // as its opener.  This will call WebContentsImpl::CreateOpenerRenderViews
  // on the opener to ensure that an RVH exists.
  int opener_routing_id =
      contents()->CreateOpenerRenderViews(pending_rfh->GetSiteInstance());

  // We should find the pending RVH and not create a new one.
  EXPECT_EQ(pending_rfh->GetRenderViewHost()->GetRoutingID(),
            opener_routing_id);
}

// Tests that WebContentsImpl uses the current URL, not the SiteInstance's site,
// to determine whether a navigation is cross-site.
TEST_F(WebContentsImplTest, CrossSiteComparesAgainstCurrentPage) {
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->GetMainFrame()->PrepareForCommit(url);
  contents()->TestDidNavigate(
      orig_rfh, 1, url, ui::PAGE_TRANSITION_TYPED);

  // Open a related contents to a second site.
  scoped_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), instance1));
  const GURL url2("http://www.yahoo.com");
  contents2->GetController().LoadURL(url2, Referrer(),
                                     ui::PAGE_TRANSITION_TYPED,
                                     std::string());
  contents2->GetMainFrame()->PrepareForCommit(url2);
  // The first RVH in contents2 isn't live yet, so we shortcut the cross site
  // pending.
  TestRenderFrameHost* rfh2 = contents2->GetMainFrame();
  EXPECT_FALSE(contents2->cross_navigation_pending());
  contents2->TestDidNavigate(rfh2, 2, url2, ui::PAGE_TRANSITION_TYPED);
  SiteInstance* instance2 = contents2->GetSiteInstance();
  EXPECT_NE(instance1, instance2);
  EXPECT_FALSE(contents2->cross_navigation_pending());

  // Simulate a link click in first contents to second site.  Doesn't switch
  // SiteInstances, because we don't intercept WebKit navigations.
  contents()->TestDidNavigate(
      orig_rfh, 2, url2, ui::PAGE_TRANSITION_TYPED);
  SiteInstance* instance3 = contents()->GetSiteInstance();
  EXPECT_EQ(instance1, instance3);
  EXPECT_FALSE(contents()->cross_navigation_pending());

  // Navigate to the new site.  Doesn't switch SiteInstancees, because we
  // compare against the current URL, not the SiteInstance's site.
  const GURL url3("http://mail.yahoo.com");
  controller().LoadURL(
      url3, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_FALSE(contents()->cross_navigation_pending());
  contents()->GetMainFrame()->PrepareForCommit(url3);
  contents()->TestDidNavigate(
      orig_rfh, 3, url3, ui::PAGE_TRANSITION_TYPED);
  SiteInstance* instance4 = contents()->GetSiteInstance();
  EXPECT_EQ(instance1, instance4);
}

// Test that the onbeforeunload and onunload handlers run when navigating
// across site boundaries.
TEST_F(WebContentsImplTest, CrossSiteUnloadHandlers) {
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->GetMainFrame()->PrepareForCommit(url);
  contents()->TestDidNavigate(orig_rfh, 1, url, ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rfh, contents()->GetMainFrame());

  // Navigate to new site, but simulate an onbeforeunload denial.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(orig_rfh->IsWaitingForBeforeUnloadACK());
  base::TimeTicks now = base::TimeTicks::Now();
  orig_rfh->OnMessageReceived(
      FrameHostMsg_BeforeUnload_ACK(0, false, now, now));
  EXPECT_FALSE(orig_rfh->IsWaitingForBeforeUnloadACK());
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rfh, contents()->GetMainFrame());

  // Navigate again, but simulate an onbeforeunload approval.
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(orig_rfh->IsWaitingForBeforeUnloadACK());
  now = base::TimeTicks::Now();
  orig_rfh->PrepareForCommit(url2);
  EXPECT_FALSE(orig_rfh->IsWaitingForBeforeUnloadACK());
  EXPECT_TRUE(contents()->cross_navigation_pending());
  TestRenderFrameHost* pending_rfh = contents()->GetPendingMainFrame();

  // We won't hear DidNavigate until the onunload handler has finished running.

  // DidNavigate from the pending page.
  contents()->TestDidNavigate(
      pending_rfh, 1, url2, ui::PAGE_TRANSITION_TYPED);
  SiteInstance* instance2 = contents()->GetSiteInstance();
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(pending_rfh, contents()->GetMainFrame());
  EXPECT_NE(instance1, instance2);
  EXPECT_TRUE(contents()->GetPendingMainFrame() == NULL);
}

// Test that during a slow cross-site navigation, the original renderer can
// navigate to a different URL and have it displayed, canceling the slow
// navigation.
TEST_F(WebContentsImplTest, CrossSiteNavigationPreempted) {
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderFrameHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->GetMainFrame()->PrepareForCommit(url);
  contents()->TestDidNavigate(orig_rfh, 1, url, ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rfh, contents()->GetMainFrame());

  // Navigate to new site, simulating an onbeforeunload approval.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(orig_rfh->IsWaitingForBeforeUnloadACK());
  orig_rfh->PrepareForCommit(url2);
  EXPECT_TRUE(contents()->cross_navigation_pending());

  // Suppose the original renderer navigates before the new one is ready.
  orig_rfh->SendNavigate(2, GURL("http://www.google.com/foo"));

  // Verify that the pending navigation is cancelled.
  EXPECT_FALSE(orig_rfh->IsWaitingForBeforeUnloadACK());
  SiteInstance* instance2 = contents()->GetSiteInstance();
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rfh, contents()->GetMainFrame());
  EXPECT_EQ(instance1, instance2);
  EXPECT_TRUE(contents()->GetPendingMainFrame() == NULL);
}

TEST_F(WebContentsImplTest, CrossSiteNavigationBackPreempted) {
  // Start with a web ui page, which gets a new RVH with WebUI bindings.
  const GURL url1("chrome://blah");
  controller().LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  TestRenderFrameHost* ntp_rfh = contents()->GetMainFrame();
  ntp_rfh->PrepareForCommit(url1);
  contents()->TestDidNavigate(ntp_rfh, 1, url1, ui::PAGE_TRANSITION_TYPED);
  NavigationEntry* entry1 = controller().GetLastCommittedEntry();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(ntp_rfh, contents()->GetMainFrame());
  EXPECT_EQ(url1, entry1->GetURL());
  EXPECT_EQ(instance1,
            NavigationEntryImpl::FromNavigationEntry(entry1)->site_instance());
  EXPECT_TRUE(ntp_rfh->GetRenderViewHost()->GetEnabledBindings() &
              BINDINGS_POLICY_WEB_UI);

  // Navigate to new site.
  const GURL url2("http://www.google.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(contents()->cross_navigation_pending());
  TestRenderFrameHost* google_rfh = contents()->GetPendingMainFrame();

  // Simulate beforeunload approval.
  EXPECT_TRUE(ntp_rfh->IsWaitingForBeforeUnloadACK());
  base::TimeTicks now = base::TimeTicks::Now();
  ntp_rfh->PrepareForCommit(url2);

  // DidNavigate from the pending page.
  contents()->TestDidNavigate(
      google_rfh, 1, url2, ui::PAGE_TRANSITION_TYPED);
  NavigationEntry* entry2 = controller().GetLastCommittedEntry();
  SiteInstance* instance2 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(google_rfh, contents()->GetMainFrame());
  EXPECT_NE(instance1, instance2);
  EXPECT_FALSE(contents()->GetPendingMainFrame());
  EXPECT_EQ(url2, entry2->GetURL());
  EXPECT_EQ(instance2,
            NavigationEntryImpl::FromNavigationEntry(entry2)->site_instance());
  EXPECT_FALSE(google_rfh->GetRenderViewHost()->GetEnabledBindings() &
               BINDINGS_POLICY_WEB_UI);

  // Navigate to third page on same site.
  const GURL url3("http://news.google.com");
  controller().LoadURL(
      url3, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_FALSE(contents()->cross_navigation_pending());
  contents()->GetMainFrame()->PrepareForCommit(url3);
  contents()->TestDidNavigate(
      google_rfh, 2, url3, ui::PAGE_TRANSITION_TYPED);
  NavigationEntry* entry3 = controller().GetLastCommittedEntry();
  SiteInstance* instance3 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(google_rfh, contents()->GetMainFrame());
  EXPECT_EQ(instance2, instance3);
  EXPECT_FALSE(contents()->GetPendingMainFrame());
  EXPECT_EQ(url3, entry3->GetURL());
  EXPECT_EQ(instance3,
            NavigationEntryImpl::FromNavigationEntry(entry3)->site_instance());

  // Go back within the site.
  controller().GoBack();
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(entry2, controller().GetPendingEntry());

  // Before that commits, go back again.
  controller().GoBack();
  EXPECT_TRUE(contents()->cross_navigation_pending());
  EXPECT_TRUE(contents()->GetPendingMainFrame());
  EXPECT_EQ(entry1, controller().GetPendingEntry());

  // Simulate beforeunload approval.
  EXPECT_TRUE(google_rfh->IsWaitingForBeforeUnloadACK());
  now = base::TimeTicks::Now();
  google_rfh->PrepareForCommit(url2);
  google_rfh->OnMessageReceived(
      FrameHostMsg_BeforeUnload_ACK(0, true, now, now));

  // DidNavigate from the first back. This aborts the second back's pending RFH.
  contents()->TestDidNavigate(google_rfh, 1, url2, ui::PAGE_TRANSITION_TYPED);

  // We should commit this page and forget about the second back.
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_FALSE(controller().GetPendingEntry());
  EXPECT_EQ(google_rfh, contents()->GetMainFrame());
  EXPECT_EQ(url2, controller().GetLastCommittedEntry()->GetURL());

  // We should not have corrupted the NTP entry.
  EXPECT_EQ(instance3,
            NavigationEntryImpl::FromNavigationEntry(entry3)->site_instance());
  EXPECT_EQ(instance2,
            NavigationEntryImpl::FromNavigationEntry(entry2)->site_instance());
  EXPECT_EQ(instance1,
            NavigationEntryImpl::FromNavigationEntry(entry1)->site_instance());
  EXPECT_EQ(url1, entry1->GetURL());
}

// Test that during a slow cross-site navigation, a sub-frame navigation in the
// original renderer will not cancel the slow navigation (bug 42029).
TEST_F(WebContentsImplTest, CrossSiteNavigationNotPreemptedByFrame) {
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();

  // Navigate to URL.  First URL should use the original RenderFrameHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->GetMainFrame()->PrepareForCommit(url);
  contents()->TestDidNavigate(orig_rfh, 1, url, ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rfh, contents()->GetMainFrame());

  // Start navigating to new site.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());

  // Simulate a sub-frame navigation arriving and ensure the RVH is still
  // waiting for a before unload response.
  TestRenderFrameHost* child_rfh = orig_rfh->AppendChild("subframe");
  child_rfh->SendNavigateWithTransition(
      1, GURL("http://google.com/frame"), ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  EXPECT_TRUE(orig_rfh->IsWaitingForBeforeUnloadACK());

  // Now simulate the onbeforeunload approval and verify the navigation is
  // not canceled.
  orig_rfh->PrepareForCommit(url2);
  EXPECT_FALSE(orig_rfh->IsWaitingForBeforeUnloadACK());
  EXPECT_TRUE(contents()->cross_navigation_pending());
}

// Test that a cross-site navigation is not preempted if the previous
// renderer sends a FrameNavigate message just before being told to stop.
// We should only preempt the cross-site navigation if the previous renderer
// has started a new navigation.  See http://crbug.com/79176.
TEST_F(WebContentsImplTest, CrossSiteNotPreemptedDuringBeforeUnload) {
  // Navigate to NTP URL.
  const GURL url("chrome://blah");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();
  EXPECT_FALSE(contents()->cross_navigation_pending());

  // Navigate to new site, with the beforeunload request in flight.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  TestRenderFrameHost* pending_rfh = contents()->GetPendingMainFrame();
  EXPECT_TRUE(contents()->cross_navigation_pending());
  EXPECT_TRUE(orig_rfh->IsWaitingForBeforeUnloadACK());

  // Suppose the first navigation tries to commit now, with a
  // FrameMsg_Stop in flight.  This should not cancel the pending navigation,
  // but it should act as if the beforeunload ack arrived.
  orig_rfh->SendNavigate(1, GURL("chrome://blah"));
  EXPECT_TRUE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rfh, contents()->GetMainFrame());
  EXPECT_FALSE(orig_rfh->IsWaitingForBeforeUnloadACK());

  // The pending navigation should be able to commit successfully.
  contents()->TestDidNavigate(pending_rfh, 1, url2, ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(pending_rfh, contents()->GetMainFrame());
}

// Test that a cross-site navigation that doesn't commit after the unload
// handler doesn't leave the contents in a stuck state.  http://crbug.com/88562
TEST_F(WebContentsImplTest, CrossSiteNavigationCanceled) {
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use original RenderFrameHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->GetMainFrame()->PrepareForCommit(url);
  contents()->TestDidNavigate(orig_rfh, 1, url, ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rfh, contents()->GetMainFrame());

  // Navigate to new site, simulating an onbeforeunload approval.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(orig_rfh->IsWaitingForBeforeUnloadACK());
  contents()->GetMainFrame()->PrepareForCommit(url2);
  EXPECT_TRUE(contents()->cross_navigation_pending());

  // Simulate swap out message when the response arrives.
  orig_rfh->OnSwappedOut();

  // Suppose the navigation doesn't get a chance to commit, and the user
  // navigates in the current RFH's SiteInstance.
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());

  // Verify that the pending navigation is cancelled and the renderer is no
  // longer swapped out.
  EXPECT_FALSE(orig_rfh->IsWaitingForBeforeUnloadACK());
  SiteInstance* instance2 = contents()->GetSiteInstance();
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rfh, contents()->GetMainFrame());
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, orig_rfh->rfh_state());
  EXPECT_EQ(instance1, instance2);
  EXPECT_TRUE(contents()->GetPendingMainFrame() == NULL);
}

// Test that NavigationEntries have the correct page state after going
// forward and back.  Prevents regression for bug 1116137.
TEST_F(WebContentsImplTest, NavigationEntryContentState) {
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();

  // Navigate to URL.  There should be no committed entry yet.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  NavigationEntry* entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry == NULL);

  // Committed entry should have page state after DidNavigate.
  contents()->TestDidNavigate(orig_rfh, 1, url, ui::PAGE_TRANSITION_TYPED);
  entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());

  // Navigate to same site.
  const GURL url2("http://images.google.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());

  // Committed entry should have page state after DidNavigate.
  contents()->TestDidNavigate(orig_rfh, 2, url2, ui::PAGE_TRANSITION_TYPED);
  entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());

  // Now go back.  Committed entry should still have page state.
  controller().GoBack();
  contents()->TestDidNavigate(orig_rfh, 1, url, ui::PAGE_TRANSITION_TYPED);
  entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());
}

// Test that NavigationEntries have the correct page state and SiteInstance
// state after opening a new window to about:blank.  Prevents regression for
// bugs b/1116137 and http://crbug.com/111975.
TEST_F(WebContentsImplTest, NavigationEntryContentStateNewWindow) {
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();

  // When opening a new window, it is navigated to about:blank internally.
  // Currently, this results in two DidNavigate events.
  const GURL url(url::kAboutBlankURL);
  contents()->TestDidNavigate(orig_rfh, 1, url, ui::PAGE_TRANSITION_TYPED);
  contents()->TestDidNavigate(orig_rfh, 1, url, ui::PAGE_TRANSITION_TYPED);

  // Should have a page state here.
  NavigationEntry* entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());

  // The SiteInstance should be available for other navigations to use.
  NavigationEntryImpl* entry_impl =
      NavigationEntryImpl::FromNavigationEntry(entry);
  EXPECT_FALSE(entry_impl->site_instance()->HasSite());
  int32 site_instance_id = entry_impl->site_instance()->GetId();

  // Navigating to a normal page should not cause a process swap.
  const GURL new_url("http://www.google.com");
  controller().LoadURL(new_url, Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rfh, contents()->GetMainFrame());
  contents()->TestDidNavigate(orig_rfh, 1, new_url, ui::PAGE_TRANSITION_TYPED);
  NavigationEntryImpl* entry_impl2 = NavigationEntryImpl::FromNavigationEntry(
      controller().GetLastCommittedEntry());
  EXPECT_EQ(site_instance_id, entry_impl2->site_instance()->GetId());
  EXPECT_TRUE(entry_impl2->site_instance()->HasSite());
}

// Tests that fullscreen is exited throughout the object hierarchy when
// navigating to a new page.
TEST_F(WebContentsImplTest, NavigationExitsFullscreen) {
  FakeFullscreenDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();
  TestRenderViewHost* orig_rvh = orig_rfh->GetRenderViewHost();

  // Navigate to a site.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->GetMainFrame()->PrepareForCommit(url);
  contents()->TestDidNavigate(orig_rfh, 1, url, ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(orig_rfh, contents()->GetMainFrame());

  // Toggle fullscreen mode on (as if initiated via IPC from renderer).
  EXPECT_FALSE(orig_rvh->IsFullscreen());
  EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));
  orig_rfh->OnMessageReceived(
      FrameHostMsg_ToggleFullscreen(orig_rfh->GetRoutingID(), true));
  EXPECT_TRUE(orig_rvh->IsFullscreen());
  EXPECT_TRUE(contents()->IsFullscreenForCurrentTab());
  EXPECT_TRUE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  // Navigate to a new site.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->GetMainFrame()->PrepareForCommit(url2);
  TestRenderFrameHost* const pending_rfh = contents()->GetPendingMainFrame();
  contents()->TestDidNavigate(
      pending_rfh, 1, url2, ui::PAGE_TRANSITION_TYPED);

  // Confirm fullscreen has exited.
  EXPECT_FALSE(orig_rvh->IsFullscreen());
  EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  contents()->SetDelegate(NULL);
}

// Tests that fullscreen is exited throughout the object hierarchy when
// instructing NavigationController to GoBack() or GoForward().
TEST_F(WebContentsImplTest, HistoryNavigationExitsFullscreen) {
  FakeFullscreenDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);
  TestRenderFrameHost* const orig_rfh = contents()->GetMainFrame();
  TestRenderViewHost* const orig_rvh = orig_rfh->GetRenderViewHost();

  // Navigate to a site.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(orig_rfh, 1, url, ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(orig_rfh, contents()->GetMainFrame());

  // Now, navigate to another page on the same site.
  const GURL url2("http://www.google.com/search?q=kittens");
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_FALSE(contents()->cross_navigation_pending());
  contents()->TestDidNavigate(orig_rfh, 2, url2, ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(orig_rfh, contents()->GetMainFrame());

  // Sanity-check: Confirm we're not starting out in fullscreen mode.
  EXPECT_FALSE(orig_rvh->IsFullscreen());
  EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  for (int i = 0; i < 2; ++i) {
    // Toggle fullscreen mode on (as if initiated via IPC from renderer).
    orig_rfh->OnMessageReceived(
        FrameHostMsg_ToggleFullscreen(orig_rfh->GetRoutingID(), true));
    EXPECT_TRUE(orig_rvh->IsFullscreen());
    EXPECT_TRUE(contents()->IsFullscreenForCurrentTab());
    EXPECT_TRUE(fake_delegate.IsFullscreenForTabOrPending(contents()));

    // Navigate backward (or forward).
    if (i == 0)
      controller().GoBack();
    else
      controller().GoForward();
    EXPECT_FALSE(contents()->cross_navigation_pending());
    EXPECT_EQ(orig_rfh, contents()->GetMainFrame());
    contents()->TestDidNavigate(
        orig_rfh, i + 1, url, ui::PAGE_TRANSITION_FORWARD_BACK);

    // Confirm fullscreen has exited.
    EXPECT_FALSE(orig_rvh->IsFullscreen());
    EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
    EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));
  }

  contents()->SetDelegate(NULL);
}

TEST_F(WebContentsImplTest, TerminateHidesValidationMessage) {
  FakeValidationMessageDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);
  EXPECT_FALSE(fake_delegate.hide_validation_message_was_called());

  // Crash the renderer.
  contents()->GetMainFrame()->OnMessageReceived(
      FrameHostMsg_RenderProcessGone(
          0, base::TERMINATION_STATUS_PROCESS_CRASHED, -1));

  // Confirm HideValidationMessage was called.
  EXPECT_TRUE(fake_delegate.hide_validation_message_was_called());

  contents()->SetDelegate(NULL);
}

// Tests that fullscreen is exited throughout the object hierarchy on a renderer
// crash.
TEST_F(WebContentsImplTest, CrashExitsFullscreen) {
  FakeFullscreenDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);

  // Navigate to a site.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(
      contents()->GetMainFrame(), 1, url, ui::PAGE_TRANSITION_TYPED);

  // Toggle fullscreen mode on (as if initiated via IPC from renderer).
  EXPECT_FALSE(test_rvh()->IsFullscreen());
  EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));
  contents()->GetMainFrame()->OnMessageReceived(FrameHostMsg_ToggleFullscreen(
      contents()->GetMainFrame()->GetRoutingID(), true));
  EXPECT_TRUE(test_rvh()->IsFullscreen());
  EXPECT_TRUE(contents()->IsFullscreenForCurrentTab());
  EXPECT_TRUE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  // Crash the renderer.
  main_rfh()->OnMessageReceived(
      FrameHostMsg_RenderProcessGone(
          0, base::TERMINATION_STATUS_PROCESS_CRASHED, -1));

  // Confirm fullscreen has exited.
  EXPECT_FALSE(test_rvh()->IsFullscreen());
  EXPECT_FALSE(contents()->IsFullscreenForCurrentTab());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  contents()->SetDelegate(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// Interstitial Tests
////////////////////////////////////////////////////////////////////////////////

// Test navigating to a page (with the navigation initiated from the browser,
// as when a URL is typed in the location bar) that shows an interstitial and
// creates a new navigation entry, then hiding it without proceeding.
TEST_F(WebContentsImplTest,
       ShowInterstitialFromBrowserWithNewNavigationDontProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  contents()->GetMainFrame()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Initiate a browser navigation that will trigger the interstitial
  controller().LoadURL(GURL("http://www.evil.com"), Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url2);

  // Now don't proceed.
  interstitial->DontProceed();
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page (with the navigation initiated from the renderer,
// as when clicking on a link in the page) that shows an interstitial and
// creates a new navigation entry, then hiding it without proceeding.
TEST_F(WebContentsImplTest,
       ShowInterstitiaFromRendererlWithNewNavigationDontProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  contents()->GetMainFrame()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial (no pending entry, the interstitial would have been
  // triggered by clicking on a link).
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url2);

  // Now don't proceed.
  interstitial->DontProceed();
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial without creating a new
// navigation entry (this happens when the interstitial is triggered by a
// sub-resource in the page), then hiding it without proceeding.
TEST_F(WebContentsImplTest, ShowInterstitialNoNewNavigationDontProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  contents()->GetMainFrame()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), false, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  // The URL specified to the interstitial should have been ignored.
  EXPECT_TRUE(entry->GetURL() == url1);

  // Now don't proceed.
  interstitial->DontProceed();
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page (with the navigation initiated from the browser,
// as when a URL is typed in the location bar) that shows an interstitial and
// creates a new navigation entry, then proceeding.
TEST_F(WebContentsImplTest,
       ShowInterstitialFromBrowserNewNavigationProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  contents()->GetMainFrame()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Initiate a browser navigation that will trigger the interstitial
  controller().LoadURL(GURL("http://www.evil.com"), Referrer(),
                        ui::PAGE_TRANSITION_TYPED, std::string());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url2);

  // Then proceed.
  interstitial->Proceed();
  // The interstitial should show until the new navigation commits.
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(deleted);
  EXPECT_EQ(TestInterstitialPage::OKED, state);
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);

  // Simulate the navigation to the page, that's when the interstitial gets
  // hidden.
  GURL url3("http://www.thepage.com");
  contents()->GetMainFrame()->SendNavigate(2, url3);

  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url3);

  EXPECT_EQ(2, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page (with the navigation initiated from the renderer,
// as when clicking on a link in the page) that shows an interstitial and
// creates a new navigation entry, then proceeding.
TEST_F(WebContentsImplTest,
       ShowInterstitialFromRendererNewNavigationProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  contents()->GetMainFrame()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url2);

  // Then proceed.
  interstitial->Proceed();
  // The interstitial should show until the new navigation commits.
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(deleted);
  EXPECT_EQ(TestInterstitialPage::OKED, state);
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);

  // Simulate the navigation to the page, that's when the interstitial gets
  // hidden.
  GURL url3("http://www.thepage.com");
  contents()->GetMainFrame()->SendNavigate(2, url3);

  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url3);

  EXPECT_EQ(2, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial without creating a new
// navigation entry (this happens when the interstitial is triggered by a
// sub-resource in the page), then proceeding.
TEST_F(WebContentsImplTest, ShowInterstitialNoNewNavigationProceed) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL url1("http://www.google.com");
  contents()->GetMainFrame()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), false, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == interstitial);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  // The URL specified to the interstitial should have been ignored.
  EXPECT_TRUE(entry->GetURL() == url1);

  // Then proceed.
  interstitial->Proceed();
  // Since this is not a new navigation, the previous page is dismissed right
  // away and shows the original page.
  EXPECT_EQ(TestInterstitialPage::OKED, state);
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == url1);

  EXPECT_EQ(1, controller().GetEntryCount());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial, then navigating away.
TEST_F(WebContentsImplTest, ShowInterstitialThenNavigate) {
  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url);

  // While interstitial showing, navigate to a new URL.
  const GURL url2("http://www.yahoo.com");
  contents()->GetMainFrame()->SendNavigate(1, url2);

  EXPECT_EQ(TestInterstitialPage::CANCELED, state);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial, then going back.
TEST_F(WebContentsImplTest, ShowInterstitialThenGoBack) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL url1("http://www.google.com");
  contents()->GetMainFrame()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL interstitial_url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(2, interstitial_url);

  // While the interstitial is showing, go back.
  controller().GoBack();
  contents()->GetMainFrame()->SendNavigate(1, url1);

  // Make sure we are back to the original page and that the interstitial is
  // gone.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(url1.spec(), entry->GetURL().spec());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial, has a renderer crash,
// and then goes back.
TEST_F(WebContentsImplTest, ShowInterstitialCrashRendererThenGoBack) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL url1("http://www.google.com");
  contents()->GetMainFrame()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL interstitial_url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(2, interstitial_url);

  // Crash the renderer
  main_rfh()->OnMessageReceived(
      FrameHostMsg_RenderProcessGone(
          0, base::TERMINATION_STATUS_PROCESS_CRASHED, -1));

  // While the interstitial is showing, go back.
  controller().GoBack();
  contents()->GetMainFrame()->SendNavigate(1, url1);

  // Make sure we are back to the original page and that the interstitial is
  // gone.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(url1.spec(), entry->GetURL().spec());

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial, has the renderer crash,
// and then navigates to the interstitial.
TEST_F(WebContentsImplTest, ShowInterstitialCrashRendererThenNavigate) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL url1("http://www.google.com");
  contents()->GetMainFrame()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL interstitial_url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();

  // Crash the renderer
  main_rfh()->OnMessageReceived(
      FrameHostMsg_RenderProcessGone(
          0, base::TERMINATION_STATUS_PROCESS_CRASHED, -1));

  interstitial->TestDidNavigate(2, interstitial_url);
}

// Test navigating to a page that shows an interstitial, then close the
// contents.
TEST_F(WebContentsImplTest, ShowInterstitialThenCloseTab) {
  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url);

  // Now close the contents.
  DeleteContents();
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test navigating to a page that shows an interstitial, then close the
// contents.
TEST_F(WebContentsImplTest, ShowInterstitialThenCloseAndShutdown) {
  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url);
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      interstitial->GetMainFrame()->GetRenderViewHost());

  // Now close the contents.
  DeleteContents();
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);

  // Before the interstitial has a chance to process its shutdown task,
  // simulate quitting the browser.  This goes through all processes and
  // tells them to destruct.
  rvh->GetMainFrame()->OnMessageReceived(
        FrameHostMsg_RenderProcessGone(0, 0, 0));

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test that after Proceed is called and an interstitial is still shown, no more
// commands get executed.
TEST_F(WebContentsImplTest, ShowInterstitialProceedMultipleCommands) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL url1("http://www.google.com");
  contents()->GetMainFrame()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url2);

  // Run a command.
  EXPECT_EQ(0, interstitial->command_received_count());
  interstitial->TestDomOperationResponse("toto");
  EXPECT_EQ(1, interstitial->command_received_count());

  // Then proceed.
  interstitial->Proceed();
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(deleted);

  // While the navigation to the new page is pending, send other commands, they
  // should be ignored.
  interstitial->TestDomOperationResponse("hello");
  interstitial->TestDomOperationResponse("hi");
  EXPECT_EQ(1, interstitial->command_received_count());
}

// Test showing an interstitial while another interstitial is already showing.
TEST_F(WebContentsImplTest, ShowInterstitialOnInterstitial) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL start_url("http://www.google.com");
  contents()->GetMainFrame()->SendNavigate(1, start_url);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state1 =
      TestInterstitialPage::INVALID;
  bool deleted1 = false;
  GURL url1("http://interstitial1");
  TestInterstitialPage* interstitial1 =
      new TestInterstitialPage(contents(), true, url1, &state1, &deleted1);
  TestInterstitialPageStateGuard state_guard1(interstitial1);
  interstitial1->Show();
  interstitial1->TestDidNavigate(1, url1);

  // Now show another interstitial.
  TestInterstitialPage::InterstitialState state2 =
      TestInterstitialPage::INVALID;
  bool deleted2 = false;
  GURL url2("http://interstitial2");
  TestInterstitialPage* interstitial2 =
      new TestInterstitialPage(contents(), true, url2, &state2, &deleted2);
  TestInterstitialPageStateGuard state_guard2(interstitial2);
  interstitial2->Show();
  interstitial2->TestDidNavigate(1, url2);

  // Showing interstitial2 should have caused interstitial1 to go away.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state1);
  EXPECT_EQ(TestInterstitialPage::UNDECIDED, state2);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted1);
  ASSERT_FALSE(deleted2);

  // Let's make sure interstitial2 is working as intended.
  interstitial2->Proceed();
  GURL landing_url("http://www.thepage.com");
  contents()->GetMainFrame()->SendNavigate(2, landing_url);

  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == landing_url);
  EXPECT_EQ(2, controller().GetEntryCount());
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted2);
}

// Test showing an interstitial, proceeding and then navigating to another
// interstitial.
TEST_F(WebContentsImplTest, ShowInterstitialProceedShowInterstitial) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL start_url("http://www.google.com");
  contents()->GetMainFrame()->SendNavigate(1, start_url);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state1 =
      TestInterstitialPage::INVALID;
  bool deleted1 = false;
  GURL url1("http://interstitial1");
  TestInterstitialPage* interstitial1 =
      new TestInterstitialPage(contents(), true, url1, &state1, &deleted1);
  TestInterstitialPageStateGuard state_guard1(interstitial1);
  interstitial1->Show();
  interstitial1->TestDidNavigate(1, url1);

  // Take action.  The interstitial won't be hidden until the navigation is
  // committed.
  interstitial1->Proceed();
  EXPECT_EQ(TestInterstitialPage::OKED, state1);

  // Now show another interstitial (simulating the navigation causing another
  // interstitial).
  TestInterstitialPage::InterstitialState state2 =
      TestInterstitialPage::INVALID;
  bool deleted2 = false;
  GURL url2("http://interstitial2");
  TestInterstitialPage* interstitial2 =
      new TestInterstitialPage(contents(), true, url2, &state2, &deleted2);
  TestInterstitialPageStateGuard state_guard2(interstitial2);
  interstitial2->Show();
  interstitial2->TestDidNavigate(1, url2);

  // Showing interstitial2 should have caused interstitial1 to go away.
  EXPECT_EQ(TestInterstitialPage::UNDECIDED, state2);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted1);
  ASSERT_FALSE(deleted2);

  // Let's make sure interstitial2 is working as intended.
  interstitial2->Proceed();
  GURL landing_url("http://www.thepage.com");
  contents()->GetMainFrame()->SendNavigate(2, landing_url);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted2);
  EXPECT_FALSE(contents()->ShowingInterstitialPage());
  EXPECT_TRUE(contents()->GetInterstitialPage() == NULL);
  NavigationEntry* entry = controller().GetVisibleEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetURL() == landing_url);
  EXPECT_EQ(2, controller().GetEntryCount());
}

// Test that navigating away from an interstitial while it's loading cause it
// not to show.
TEST_F(WebContentsImplTest, NavigateBeforeInterstitialShows) {
  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL interstitial_url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();

  // Let's simulate a navigation initiated from the browser before the
  // interstitial finishes loading.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_FALSE(interstitial->is_showing());
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(deleted);

  // Now let's make the interstitial navigation commit.
  interstitial->TestDidNavigate(1, interstitial_url);

  // After it loaded the interstitial should be gone.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Test that a new request to show an interstitial while an interstitial is
// pending does not cause problems. htp://crbug/29655 and htp://crbug/9442.
TEST_F(WebContentsImplTest, TwoQuickInterstitials) {
  GURL interstitial_url("http://interstitial");

  // Show a first interstitial.
  TestInterstitialPage::InterstitialState state1 =
      TestInterstitialPage::INVALID;
  bool deleted1 = false;
  TestInterstitialPage* interstitial1 =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state1, &deleted1);
  TestInterstitialPageStateGuard state_guard1(interstitial1);
  interstitial1->Show();

  // Show another interstitial on that same contents before the first one had
  // time to load.
  TestInterstitialPage::InterstitialState state2 =
      TestInterstitialPage::INVALID;
  bool deleted2 = false;
  TestInterstitialPage* interstitial2 =
      new TestInterstitialPage(contents(), true, interstitial_url,
                               &state2, &deleted2);
  TestInterstitialPageStateGuard state_guard2(interstitial2);
  interstitial2->Show();

  // The first interstitial should have been closed and deleted.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state1);
  // The 2nd one should still be OK.
  EXPECT_EQ(TestInterstitialPage::UNDECIDED, state2);

  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted1);
  ASSERT_FALSE(deleted2);

  // Make the interstitial navigation commit it should be showing.
  interstitial2->TestDidNavigate(1, interstitial_url);
  EXPECT_EQ(interstitial2, contents()->GetInterstitialPage());
}

// Test showing an interstitial and have its renderer crash.
TEST_F(WebContentsImplTest, InterstitialCrasher) {
  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // Simulate a renderer crash before the interstitial is shown.
  interstitial->TestRenderViewTerminated(
      base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  // The interstitial should have been dismissed.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);

  // Now try again but this time crash the intersitial after it was shown.
  interstitial =
      new TestInterstitialPage(contents(), true, url, &state, &deleted);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url);
  // Simulate a renderer crash.
  interstitial->TestRenderViewTerminated(
      base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  // The interstitial should have been dismissed.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
}

// Tests that showing an interstitial as a result of a browser initiated
// navigation while an interstitial is showing does not remove the pending
// entry (see http://crbug.com/9791).
TEST_F(WebContentsImplTest, NewInterstitialDoesNotCancelPendingEntry) {
  const char kUrl[] = "http://www.badguys.com/";
  const GURL kGURL(kUrl);

  // Start a navigation to a page
  contents()->GetController().LoadURL(
      kGURL, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());

  // Simulate that navigation triggering an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, kGURL, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, kGURL);

  // Initiate a new navigation from the browser that also triggers an
  // interstitial.
  contents()->GetController().LoadURL(
      kGURL, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  TestInterstitialPage::InterstitialState state2 =
      TestInterstitialPage::INVALID;
  bool deleted2 = false;
  TestInterstitialPage* interstitial2 =
      new TestInterstitialPage(contents(), true, kGURL, &state2, &deleted2);
  TestInterstitialPageStateGuard state_guard2(interstitial2);
  interstitial2->Show();
  interstitial2->TestDidNavigate(1, kGURL);

  // Make sure we still have an entry.
  NavigationEntry* entry = contents()->GetController().GetPendingEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(kUrl, entry->GetURL().spec());

  // And that the first interstitial is gone, but not the second.
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  EXPECT_EQ(TestInterstitialPage::UNDECIDED, state2);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(deleted);
  EXPECT_FALSE(deleted2);
}

// Tests that Javascript messages are not shown while an interstitial is
// showing.
TEST_F(WebContentsImplTest, NoJSMessageOnInterstitials) {
  const char kUrl[] = "http://www.badguys.com/";
  const GURL kGURL(kUrl);

  // Start a navigation to a page
  contents()->GetController().LoadURL(
      kGURL, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  // DidNavigate from the page
  contents()->TestDidNavigate(
      contents()->GetMainFrame(), 1, kGURL, ui::PAGE_TRANSITION_TYPED);

  // Simulate showing an interstitial while the page is showing.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, kGURL, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, kGURL);

  // While the interstitial is showing, let's simulate the hidden page
  // attempting to show a JS message.
  IPC::Message* dummy_message = new IPC::Message;
  contents()->RunJavaScriptMessage(contents()->GetMainFrame(),
      base::ASCIIToUTF16("This is an informative message"),
      base::ASCIIToUTF16("OK"),
      kGURL, JAVASCRIPT_MESSAGE_TYPE_ALERT, dummy_message);
  EXPECT_TRUE(contents()->last_dialog_suppressed_);
}

// Makes sure that if the source passed to CopyStateFromAndPrune has an
// interstitial it isn't copied over to the destination.
TEST_F(WebContentsImplTest, CopyStateFromAndPruneSourceInterstitial) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  contents()->GetMainFrame()->SendNavigate(1, url1);
  EXPECT_EQ(1, controller().GetEntryCount());

  // Initiate a browser navigation that will trigger the interstitial
  controller().LoadURL(GURL("http://www.evil.com"), Referrer(),
                        ui::PAGE_TRANSITION_TYPED, std::string());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents(), true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_EQ(2, controller().GetEntryCount());

  // Create another NavigationController.
  GURL url3("http://foo2");
  scoped_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents()));
  NavigationControllerImpl& other_controller = other_contents->GetController();
  other_contents->NavigateAndCommit(url3);
  other_contents->ExpectSetHistoryOffsetAndLength(1, 2);
  other_controller.CopyStateFromAndPrune(&controller(), false);

  // The merged controller should only have two entries: url1 and url2.
  ASSERT_EQ(2, other_controller.GetEntryCount());
  EXPECT_EQ(1, other_controller.GetCurrentEntryIndex());
  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3, other_controller.GetEntryAtIndex(1)->GetURL());

  // And the merged controller shouldn't be showing an interstitial.
  EXPECT_FALSE(other_contents->ShowingInterstitialPage());
}

// Makes sure that CopyStateFromAndPrune cannot be called if the target is
// showing an interstitial.
TEST_F(WebContentsImplTest, CopyStateFromAndPruneTargetInterstitial) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  contents()->NavigateAndCommit(url1);

  // Create another NavigationController.
  scoped_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents()));
  NavigationControllerImpl& other_controller = other_contents->GetController();

  // Navigate it to url2.
  GURL url2("http://foo2");
  other_contents->NavigateAndCommit(url2);

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::INVALID;
  bool deleted = false;
  GURL url3("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(other_contents.get(), true, url3, &state,
                               &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url3);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_EQ(2, other_controller.GetEntryCount());

  // Ensure that we do not allow calling CopyStateFromAndPrune when an
  // interstitial is showing in the target.
  EXPECT_FALSE(other_controller.CanPruneAllButLastCommitted());
}

// Regression test for http://crbug.com/168611 - the URLs passed by the
// DidFinishLoad and DidFailLoadWithError IPCs should get filtered.
TEST_F(WebContentsImplTest, FilterURLs) {
  TestWebContentsObserver observer(contents());

  // A navigation to about:whatever should always look like a navigation to
  // about:blank
  GURL url_normalized(url::kAboutBlankURL);
  GURL url_from_ipc("about:whatever");

  // We navigate the test WebContents to about:blank, since NavigateAndCommit
  // will use the given URL to create the NavigationEntry as well, and that
  // entry should contain the filtered URL.
  contents()->NavigateAndCommit(url_normalized);

  // Check that an IPC with about:whatever is correctly normalized.
  contents()->TestDidFinishLoad(url_from_ipc);

  EXPECT_EQ(url_normalized, observer.last_url());

  // Create and navigate another WebContents.
  scoped_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents()));
  TestWebContentsObserver other_observer(other_contents.get());
  other_contents->NavigateAndCommit(url_normalized);

  // Check that an IPC with about:whatever is correctly normalized.
  other_contents->TestDidFailLoadWithError(
      url_from_ipc, 1, base::string16());
  EXPECT_EQ(url_normalized, other_observer.last_url());
}

// Test that if a pending contents is deleted before it is shown, we don't
// crash.
TEST_F(WebContentsImplTest, PendingContents) {
  scoped_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents()));
  contents()->AddPendingContents(other_contents.get());
  int route_id = other_contents->GetRenderViewHost()->GetRoutingID();
  other_contents.reset();
  EXPECT_EQ(NULL, contents()->GetCreatedWindow(route_id));
}

TEST_F(WebContentsImplTest, CapturerOverridesPreferredSize) {
  const gfx::Size original_preferred_size(1024, 768);
  contents()->UpdatePreferredSize(original_preferred_size);

  // With no capturers, expect the preferred size to be the one propagated into
  // WebContentsImpl via the RenderViewHostDelegate interface.
  EXPECT_EQ(contents()->GetCapturerCount(), 0);
  EXPECT_EQ(original_preferred_size, contents()->GetPreferredSize());

  // Increment capturer count, but without specifying a capture size.  Expect
  // a "not set" preferred size.
  contents()->IncrementCapturerCount(gfx::Size());
  EXPECT_EQ(1, contents()->GetCapturerCount());
  EXPECT_EQ(gfx::Size(), contents()->GetPreferredSize());

  // Increment capturer count again, but with an overriding capture size.
  // Expect preferred size to now be overridden to the capture size.
  const gfx::Size capture_size(1280, 720);
  contents()->IncrementCapturerCount(capture_size);
  EXPECT_EQ(2, contents()->GetCapturerCount());
  EXPECT_EQ(capture_size, contents()->GetPreferredSize());

  // Increment capturer count a third time, but the expect that the preferred
  // size is still the first capture size.
  const gfx::Size another_capture_size(720, 480);
  contents()->IncrementCapturerCount(another_capture_size);
  EXPECT_EQ(3, contents()->GetCapturerCount());
  EXPECT_EQ(capture_size, contents()->GetPreferredSize());

  // Decrement capturer count twice, but expect the preferred size to still be
  // overridden.
  contents()->DecrementCapturerCount();
  contents()->DecrementCapturerCount();
  EXPECT_EQ(1, contents()->GetCapturerCount());
  EXPECT_EQ(capture_size, contents()->GetPreferredSize());

  // Decrement capturer count, and since the count has dropped to zero, the
  // original preferred size should be restored.
  contents()->DecrementCapturerCount();
  EXPECT_EQ(0, contents()->GetCapturerCount());
  EXPECT_EQ(original_preferred_size, contents()->GetPreferredSize());
}

TEST_F(WebContentsImplTest, CapturerPreventsHiding) {
  const gfx::Size original_preferred_size(1024, 768);
  contents()->UpdatePreferredSize(original_preferred_size);

  TestRenderWidgetHostView* view = static_cast<TestRenderWidgetHostView*>(
      contents()->GetMainFrame()->GetRenderViewHost()->GetView());

  // With no capturers, setting and un-setting occlusion should change the
  // view's occlusion state.
  EXPECT_FALSE(view->is_showing());
  contents()->WasShown();
  EXPECT_TRUE(view->is_showing());
  contents()->WasHidden();
  EXPECT_FALSE(view->is_showing());
  contents()->WasShown();
  EXPECT_TRUE(view->is_showing());

  // Add a capturer and try to hide the contents. The view will remain visible.
  contents()->IncrementCapturerCount(gfx::Size());
  contents()->WasHidden();
  EXPECT_TRUE(view->is_showing());

  // Remove the capturer, and the WasHidden should take effect.
  contents()->DecrementCapturerCount();
  EXPECT_FALSE(view->is_showing());
}

TEST_F(WebContentsImplTest, CapturerPreventsOcclusion) {
  const gfx::Size original_preferred_size(1024, 768);
  contents()->UpdatePreferredSize(original_preferred_size);

  TestRenderWidgetHostView* view = static_cast<TestRenderWidgetHostView*>(
      contents()->GetMainFrame()->GetRenderViewHost()->GetView());

  // With no capturers, setting and un-setting occlusion should change the
  // view's occlusion state.
  EXPECT_FALSE(view->is_occluded());
  contents()->WasOccluded();
  EXPECT_TRUE(view->is_occluded());
  contents()->WasUnOccluded();
  EXPECT_FALSE(view->is_occluded());
  contents()->WasOccluded();
  EXPECT_TRUE(view->is_occluded());

  // Add a capturer. This should cause the view to be un-occluded.
  contents()->IncrementCapturerCount(gfx::Size());
  EXPECT_FALSE(view->is_occluded());

  // Try to occlude the view. This will fail to propagate because of the
  // active capturer.
  contents()->WasOccluded();
  EXPECT_FALSE(view->is_occluded());

  // Remove the capturer and try again.
  contents()->DecrementCapturerCount();
  EXPECT_FALSE(view->is_occluded());
  contents()->WasOccluded();
  EXPECT_TRUE(view->is_occluded());
}

// Tests that GetLastActiveTime starts with a real, non-zero time and updates
// on activity.
TEST_F(WebContentsImplTest, GetLastActiveTime) {
  // The WebContents starts with a valid creation time.
  EXPECT_FALSE(contents()->GetLastActiveTime().is_null());

  // Reset the last active time to a known-bad value.
  contents()->last_active_time_ = base::TimeTicks();
  ASSERT_TRUE(contents()->GetLastActiveTime().is_null());

  // Simulate activating the WebContents. The active time should update.
  contents()->WasShown();
  EXPECT_FALSE(contents()->GetLastActiveTime().is_null());
}

class ContentsZoomChangedDelegate : public WebContentsDelegate {
 public:
  ContentsZoomChangedDelegate() :
    contents_zoom_changed_call_count_(0),
    last_zoom_in_(false) {
  }

  int GetAndResetContentsZoomChangedCallCount() {
    int count = contents_zoom_changed_call_count_;
    contents_zoom_changed_call_count_ = 0;
    return count;
  }

  bool last_zoom_in() const {
    return last_zoom_in_;
  }

  // WebContentsDelegate:
  void ContentsZoomChange(bool zoom_in) override {
    contents_zoom_changed_call_count_++;
    last_zoom_in_ = zoom_in;
  }

 private:
  int contents_zoom_changed_call_count_;
  bool last_zoom_in_;

  DISALLOW_COPY_AND_ASSIGN(ContentsZoomChangedDelegate);
};

// Tests that some mouseehweel events get turned into browser zoom requests.
TEST_F(WebContentsImplTest, HandleWheelEvent) {
  using blink::WebInputEvent;

  scoped_ptr<ContentsZoomChangedDelegate> delegate(
      new ContentsZoomChangedDelegate());
  contents()->SetDelegate(delegate.get());

  int modifiers = 0;
  // Verify that normal mouse wheel events do nothing to change the zoom level.
  blink::WebMouseWheelEvent event =
      SyntheticWebMouseWheelEventBuilder::Build(0, 1, modifiers, false);
  EXPECT_FALSE(contents()->HandleWheelEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  modifiers = WebInputEvent::ShiftKey | WebInputEvent::AltKey |
      WebInputEvent::ControlKey;
  event = SyntheticWebMouseWheelEventBuilder::Build(0, 1, modifiers, false);
  EXPECT_FALSE(contents()->HandleWheelEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  // But whenever the ctrl modifier is applied with canScroll=false, they can
  // increase/decrease zoom. Except on MacOS where we never want to adjust zoom
  // with mousewheel.
  modifiers = WebInputEvent::ControlKey;
  event = SyntheticWebMouseWheelEventBuilder::Build(0, 1, modifiers, false);
  event.canScroll = false;
  bool handled = contents()->HandleWheelEvent(event);
#if defined(OS_MACOSX)
  EXPECT_FALSE(handled);
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());
#else
  EXPECT_TRUE(handled);
  EXPECT_EQ(1, delegate->GetAndResetContentsZoomChangedCallCount());
  EXPECT_TRUE(delegate->last_zoom_in());
#endif

  modifiers = WebInputEvent::ControlKey | WebInputEvent::ShiftKey |
      WebInputEvent::AltKey;
  event = SyntheticWebMouseWheelEventBuilder::Build(2, -5, modifiers, false);
  event.canScroll = false;
  handled = contents()->HandleWheelEvent(event);
#if defined(OS_MACOSX)
  EXPECT_FALSE(handled);
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());
#else
  EXPECT_TRUE(handled);
  EXPECT_EQ(1, delegate->GetAndResetContentsZoomChangedCallCount());
  EXPECT_FALSE(delegate->last_zoom_in());
#endif

  // Unless there is no vertical movement.
  event = SyntheticWebMouseWheelEventBuilder::Build(2, 0, modifiers, false);
  EXPECT_FALSE(contents()->HandleWheelEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  // Events containing precise scrolling deltas also shouldn't result in the
  // zoom being adjusted, to avoid accidental adjustments caused by
  // two-finger-scrolling on a touchpad.
  modifiers = WebInputEvent::ControlKey;
  event = SyntheticWebMouseWheelEventBuilder::Build(0, 5, modifiers, true);
  EXPECT_FALSE(contents()->HandleWheelEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  // Ensure pointers to the delegate aren't kept beyond its lifetime.
  contents()->SetDelegate(NULL);
}

// Tests that GetRelatedActiveContentsCount is shared between related
// SiteInstances and includes WebContents that have not navigated yet.
TEST_F(WebContentsImplTest, ActiveContentsCountBasic) {
  scoped_refptr<SiteInstance> instance1(
      SiteInstance::CreateForURL(browser_context(), GURL("http://a.com")));
  scoped_refptr<SiteInstance> instance2(
      instance1->GetRelatedSiteInstance(GURL("http://b.com")));

  EXPECT_EQ(0u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(0u, instance2->GetRelatedActiveContentsCount());

  scoped_ptr<TestWebContents> contents1(
      TestWebContents::Create(browser_context(), instance1.get()));
  EXPECT_EQ(1u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(1u, instance2->GetRelatedActiveContentsCount());

  scoped_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), instance1.get()));
  EXPECT_EQ(2u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(2u, instance2->GetRelatedActiveContentsCount());

  contents1.reset();
  EXPECT_EQ(1u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(1u, instance2->GetRelatedActiveContentsCount());

  contents2.reset();
  EXPECT_EQ(0u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(0u, instance2->GetRelatedActiveContentsCount());
}

// Tests that GetRelatedActiveContentsCount is preserved correctly across
// same-site and cross-site navigations.
TEST_F(WebContentsImplTest, ActiveContentsCountNavigate) {
  scoped_refptr<SiteInstance> instance(
      SiteInstance::Create(browser_context()));

  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());

  scoped_ptr<TestWebContents> contents(
      TestWebContents::Create(browser_context(), instance.get()));
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL.
  contents->GetController().LoadURL(GURL("http://a.com/1"),
                                    Referrer(),
                                    ui::PAGE_TRANSITION_TYPED,
                                    std::string());
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  contents->CommitPendingNavigation();
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL in the same site.
  contents->GetController().LoadURL(GURL("http://a.com/2"),
                                    Referrer(),
                                    ui::PAGE_TRANSITION_TYPED,
                                    std::string());
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  contents->CommitPendingNavigation();
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL in a different site.
  const GURL kUrl = GURL("http://b.com");
  contents->GetController().LoadURL(kUrl,
                                    Referrer(),
                                    ui::PAGE_TRANSITION_TYPED,
                                    std::string());
  contents->GetMainFrame()->PrepareForCommit(kUrl);
  EXPECT_TRUE(contents->cross_navigation_pending());
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  contents->CommitPendingNavigation();
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  contents.reset();
  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
}

// Tests that GetRelatedActiveContentsCount tracks BrowsingInstance changes
// from WebUI.
TEST_F(WebContentsImplTest, ActiveContentsCountChangeBrowsingInstance) {
  scoped_refptr<SiteInstance> instance(
      SiteInstance::Create(browser_context()));

  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());

  scoped_ptr<TestWebContents> contents(
      TestWebContents::Create(browser_context(), instance.get()));
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL.
  contents->NavigateAndCommit(GURL("http://a.com"));
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL with WebUI. This will change BrowsingInstances.
  contents->GetController().LoadURL(GURL(kTestWebUIUrl),
                                    Referrer(),
                                    ui::PAGE_TRANSITION_TYPED,
                                    std::string());
  contents->GetMainFrame()->PrepareForCommit(GURL(kTestWebUIUrl));
  EXPECT_TRUE(contents->cross_navigation_pending());
  scoped_refptr<SiteInstance> instance_webui(
      contents->GetPendingMainFrame()->GetSiteInstance());
  EXPECT_FALSE(instance->IsRelatedSiteInstance(instance_webui.get()));

  // At this point, contents still counts for the old BrowsingInstance.
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  EXPECT_EQ(0u, instance_webui->GetRelatedActiveContentsCount());

  // Commit and contents counts for the new one.
  contents->CommitPendingNavigation();
  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
  EXPECT_EQ(1u, instance_webui->GetRelatedActiveContentsCount());

  contents.reset();
  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
  EXPECT_EQ(0u, instance_webui->GetRelatedActiveContentsCount());
}

class LoadingWebContentsObserver : public WebContentsObserver {
 public:
  explicit LoadingWebContentsObserver(WebContents* contents)
      : WebContentsObserver(contents),
        is_loading_(false) {
  }
  ~LoadingWebContentsObserver() override {}

  void DidStartLoading(RenderViewHost* rvh) override {
    is_loading_ = true;
  }
  void DidStopLoading(RenderViewHost* rvh) override {
    is_loading_ = false;
  }

  bool is_loading() const { return is_loading_; }

 private:
  bool is_loading_;

  DISALLOW_COPY_AND_ASSIGN(LoadingWebContentsObserver);
};

// Ensure that DidStartLoading/DidStopLoading events balance out properly with
// interleaving cross-process navigations in multiple subframes.
// See https://crbug.com/448601 for details of the underlying issue. The
// sequence of events that reproduce it are as follows:
// * Navigate top-level frame with one subframe.
// * Subframe navigates more than once before the top-level frame has had a
//   chance to complete the load.
// The subframe navigations cause the loading_frames_in_progress_ to drop down
// to 0, while the loading_progresses_ map is not reset.
TEST_F(WebContentsImplTest, StartStopEventsBalance) {
  // The bug manifests itself in regular mode as well, but browser-initiated
  // navigation of subframes is only possible in --site-per-process mode within
  // unit tests.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSitePerProcess);
  const GURL main_url("http://www.chromium.org");
  const GURL foo_url("http://foo.chromium.org");
  const GURL bar_url("http://bar.chromium.org");
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();

  // Use a WebContentsObserver to approximate the behavior of the tab's spinner.
  LoadingWebContentsObserver observer(contents());

  // Navigate the main RenderFrame, simulate the DidStartLoading, and commit.
  // The frame should still be loading.
  controller().LoadURL(
      main_url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  orig_rfh->OnMessageReceived(
      FrameHostMsg_DidStartLoading(orig_rfh->GetRoutingID(), false));
  contents()->TestDidNavigate(orig_rfh, 1, main_url, ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(orig_rfh, contents()->GetMainFrame());
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_TRUE(observer.is_loading());

  // Create a child frame to navigate multiple times.
  TestRenderFrameHost* subframe = orig_rfh->AppendChild("subframe");

  // Navigate the child frame to about:blank, which will send both
  // DidStartLoading and DidStopLoading messages.
  {
    subframe->OnMessageReceived(
        FrameHostMsg_DidStartLoading(subframe->GetRoutingID(), true));
    subframe->SendNavigateWithTransition(
        1, GURL("about:blank"), ui::PAGE_TRANSITION_AUTO_SUBFRAME);
    subframe->OnMessageReceived(
        FrameHostMsg_DidStopLoading(subframe->GetRoutingID()));
  }

  // Navigate the frame to another URL, which will send again
  // DidStartLoading and DidStopLoading messages.
  {
    subframe->OnMessageReceived(
        FrameHostMsg_DidStartLoading(subframe->GetRoutingID(), true));
    subframe->SendNavigateWithTransition(
        1, foo_url, ui::PAGE_TRANSITION_AUTO_SUBFRAME);
    subframe->OnMessageReceived(
        FrameHostMsg_DidStopLoading(subframe->GetRoutingID()));
  }

  // Since the main frame hasn't sent any DidStopLoading messages, it is
  // expected that the WebContents is still in loading state.
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_TRUE(observer.is_loading());

  // Navigate the frame again, this time using LoadURLWithParams. This causes
  // RenderFrameHost to call into WebContents::DidStartLoading, which starts
  // the spinner.
  {
    NavigationController::LoadURLParams load_params(bar_url);
    load_params.referrer =
        Referrer(GURL("http://referrer"), blink::WebReferrerPolicyDefault);
    load_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
    load_params.extra_headers = "content-type: text/plain";
    load_params.load_type = NavigationController::LOAD_TYPE_DEFAULT;
    load_params.is_renderer_initiated = false;
    load_params.override_user_agent = NavigationController::UA_OVERRIDE_TRUE;
    load_params.frame_tree_node_id =
        subframe->frame_tree_node()->frame_tree_node_id();
    controller().LoadURLWithParams(load_params);

    subframe->OnMessageReceived(
        FrameHostMsg_DidStartLoading(subframe->GetRoutingID(), true));

    // Commit the navigation in the child frame and send the DidStopLoading
    // message.
    contents()->TestDidNavigate(
        subframe, 3, bar_url, ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
    subframe->OnMessageReceived(
        FrameHostMsg_DidStopLoading(subframe->GetRoutingID()));
  }

  // At this point the status should still be loading, since the main frame
  // hasn't sent the DidstopLoading message yet.
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_TRUE(observer.is_loading());

  // Send the DidStopLoading for the main frame and ensure it isn't loading
  // anymore.
  orig_rfh->OnMessageReceived(
      FrameHostMsg_DidStopLoading(orig_rfh->GetRoutingID()));
  EXPECT_FALSE(contents()->IsLoading());
  EXPECT_FALSE(observer.is_loading());
}

TEST_F(WebContentsImplTest, MediaPowerSaveBlocking) {
  // PlayerIDs are actually pointers cast to int64, so verify that both negative
  // and positive player ids don't blow up.
  const int kPlayerAudioVideoId = 15;
  const int kPlayerAudioOnlyId = -15;
  const int kPlayerVideoOnlyId = 30;
  const int kPlayerRemoteId = -30;

  EXPECT_FALSE(contents()->has_audio_power_save_blocker_for_testing());
  EXPECT_FALSE(contents()->has_video_power_save_blocker_for_testing());

  TestRenderFrameHost* rfh = contents()->GetMainFrame();
  AudioStreamMonitor* monitor = contents()->audio_stream_monitor();

  // The audio power save blocker should not be based on having a media player
  // when audio stream monitoring is available.
  if (AudioStreamMonitor::monitoring_available()) {
    // Send a fake audio stream monitor notification.  The audio power save
    // blocker should be created.
    monitor->set_was_recently_audible_for_testing(true);
    contents()->NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
    EXPECT_TRUE(contents()->has_audio_power_save_blocker_for_testing());

    // Send another fake notification, this time when WasRecentlyAudible() will
    // be false.  The power save blocker should be released.
    monitor->set_was_recently_audible_for_testing(false);
    contents()->NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
    EXPECT_FALSE(contents()->has_audio_power_save_blocker_for_testing());
  }

  // Start a player with both audio and video.  A video power save blocker
  // should be created.  If audio stream monitoring is available, an audio power
  // save blocker should be created too.
  rfh->OnMessageReceived(FrameHostMsg_MediaPlayingNotification(
      0, kPlayerAudioVideoId, true, true, false));
  EXPECT_TRUE(contents()->has_video_power_save_blocker_for_testing());
  EXPECT_EQ(contents()->has_audio_power_save_blocker_for_testing(),
            !AudioStreamMonitor::monitoring_available());

  // Upon hiding the video power save blocker should be released.
  contents()->WasHidden();
  EXPECT_FALSE(contents()->has_video_power_save_blocker_for_testing());

  // Start another player that only has video.  There should be no change in
  // the power save blockers.  The notification should take into account the
  // visibility state of the WebContents.
  rfh->OnMessageReceived(FrameHostMsg_MediaPlayingNotification(
      0, kPlayerVideoOnlyId, true, false, false));
  EXPECT_FALSE(contents()->has_video_power_save_blocker_for_testing());
  EXPECT_EQ(contents()->has_audio_power_save_blocker_for_testing(),
            !AudioStreamMonitor::monitoring_available());

  // Showing the WebContents should result in the creation of the blocker.
  contents()->WasShown();
  EXPECT_TRUE(contents()->has_video_power_save_blocker_for_testing());

  // Start another player that only has audio.  There should be no change in
  // the power save blockers.
  rfh->OnMessageReceived(FrameHostMsg_MediaPlayingNotification(
      0, kPlayerAudioOnlyId, false, true, false));
  EXPECT_TRUE(contents()->has_video_power_save_blocker_for_testing());
  EXPECT_EQ(contents()->has_audio_power_save_blocker_for_testing(),
            !AudioStreamMonitor::monitoring_available());

  // Start a remote player. There should be no change in the power save
  // blockers.
  rfh->OnMessageReceived(FrameHostMsg_MediaPlayingNotification(
      0, kPlayerRemoteId, true, true, true));
  EXPECT_TRUE(contents()->has_video_power_save_blocker_for_testing());
  EXPECT_EQ(contents()->has_audio_power_save_blocker_for_testing(),
            !AudioStreamMonitor::monitoring_available());

  // Destroy the original audio video player.  Both power save blockers should
  // remain.
  rfh->OnMessageReceived(
      FrameHostMsg_MediaPausedNotification(0, kPlayerAudioVideoId));
  EXPECT_TRUE(contents()->has_video_power_save_blocker_for_testing());
  EXPECT_EQ(contents()->has_audio_power_save_blocker_for_testing(),
            !AudioStreamMonitor::monitoring_available());

  // Destroy the audio only player.  The video power save blocker should remain.
  rfh->OnMessageReceived(
      FrameHostMsg_MediaPausedNotification(0, kPlayerAudioOnlyId));
  EXPECT_TRUE(contents()->has_video_power_save_blocker_for_testing());
  EXPECT_FALSE(contents()->has_audio_power_save_blocker_for_testing());

  // Destroy the video only player.  No power save blockers should remain.
  rfh->OnMessageReceived(
      FrameHostMsg_MediaPausedNotification(0, kPlayerVideoOnlyId));
  EXPECT_FALSE(contents()->has_video_power_save_blocker_for_testing());
  EXPECT_FALSE(contents()->has_audio_power_save_blocker_for_testing());

  // Destroy the remote player. No power save blockers should remain.
  rfh->OnMessageReceived(
      FrameHostMsg_MediaPausedNotification(0, kPlayerRemoteId));
  EXPECT_FALSE(contents()->has_video_power_save_blocker_for_testing());
  EXPECT_FALSE(contents()->has_audio_power_save_blocker_for_testing());

  // Start a player with both audio and video.  A video power save blocker
  // should be created.  If audio stream monitoring is available, an audio power
  // save blocker should be created too.
  rfh->OnMessageReceived(FrameHostMsg_MediaPlayingNotification(
      0, kPlayerAudioVideoId, true, true, false));
  EXPECT_TRUE(contents()->has_video_power_save_blocker_for_testing());
  EXPECT_EQ(contents()->has_audio_power_save_blocker_for_testing(),
            !AudioStreamMonitor::monitoring_available());

  // Crash the renderer.
  contents()->GetMainFrame()->OnMessageReceived(
      FrameHostMsg_RenderProcessGone(
          0, base::TERMINATION_STATUS_PROCESS_CRASHED, -1));

  // Verify that all the power save blockers have been released.
  EXPECT_FALSE(contents()->has_video_power_save_blocker_for_testing());
  EXPECT_FALSE(contents()->has_audio_power_save_blocker_for_testing());
}

// Test that sudden termination status is properly tracked for a frame.
TEST_F(WebContentsImplTest, SuddenTerminationForFrame) {
  const GURL url("http://www.chromium.org");
  contents()->NavigateAndCommit(url);

  TestRenderFrameHost* frame = contents()->GetMainFrame();
  EXPECT_TRUE(frame->SuddenTerminationAllowed());

  // Register a BeforeUnload handler.
  frame->SendBeforeUnloadHandlersPresent(true);
  EXPECT_FALSE(frame->SuddenTerminationAllowed());

  // Unregister the BeforeUnload handler.
  frame->SendBeforeUnloadHandlersPresent(false);
  EXPECT_TRUE(frame->SuddenTerminationAllowed());

  // Register an Unload handler.
  frame->SendUnloadHandlersPresent(true);
  EXPECT_FALSE(frame->SuddenTerminationAllowed());

  // Unregister the Unload handler.
  frame->SendUnloadHandlersPresent(false);
  EXPECT_TRUE(frame->SuddenTerminationAllowed());

  // Register a BeforeUnload handler and an Unload handler.
  frame->SendBeforeUnloadHandlersPresent(true);
  frame->SendUnloadHandlersPresent(true);
  EXPECT_FALSE(frame->SuddenTerminationAllowed());

  // Override the sudden termination status.
  frame->set_override_sudden_termination_status(true);
  EXPECT_TRUE(frame->SuddenTerminationAllowed());
  frame->set_override_sudden_termination_status(false);
  EXPECT_FALSE(frame->SuddenTerminationAllowed());

  // Sudden termination should not be allowed unless there are no BeforeUnload
  // handlers and no Unload handlers in the RenderFrame.
  frame->SendBeforeUnloadHandlersPresent(false);
  EXPECT_FALSE(frame->SuddenTerminationAllowed());
  frame->SendBeforeUnloadHandlersPresent(true);
  frame->SendUnloadHandlersPresent(false);
  EXPECT_FALSE(frame->SuddenTerminationAllowed());
  frame->SendBeforeUnloadHandlersPresent(false);
  EXPECT_TRUE(frame->SuddenTerminationAllowed());
}

}  // namespace content
