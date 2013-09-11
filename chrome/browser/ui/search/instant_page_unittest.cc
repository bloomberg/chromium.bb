// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_page.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class Profile;

namespace {

class FakePageDelegate : public InstantPage::Delegate {
 public:
  virtual ~FakePageDelegate() {
  }

  MOCK_METHOD2(InstantSupportDetermined,
               void(const content::WebContents* contents,
                    bool supports_instant));
  MOCK_METHOD1(InstantPageRenderProcessGone,
               void(const content::WebContents* contents));
  MOCK_METHOD2(InstantPageAboutToNavigateMainFrame,
               void(const content::WebContents* contents,
                    const GURL& url));
  MOCK_METHOD2(FocusOmnibox,
               void(const content::WebContents* contents,
                    OmniboxFocusState state));
  MOCK_METHOD5(NavigateToURL,
               void(const content::WebContents* contents,
                    const GURL& url,
                    content::PageTransition transition,
                    WindowOpenDisposition disposition,
                    bool is_search_type));
  MOCK_METHOD2(PasteIntoOmnibox,
               void(const content::WebContents* contents,
                    const string16& text));
  MOCK_METHOD1(DeleteMostVisitedItem, void(const GURL& url));
  MOCK_METHOD1(UndoMostVisitedDeletion, void(const GURL& url));
  MOCK_METHOD0(UndoAllMostVisitedDeletions, void());
  MOCK_METHOD1(InstantPageLoadFailed, void(content::WebContents* contents));
};

class FakePage : public InstantPage {
 public:
  FakePage(Delegate* delegate, const std::string& instant_url,
           Profile* profile, bool is_incognito);
  virtual ~FakePage();

  // InstantPage overrride.
  virtual bool ShouldProcessDeleteMostVisitedItem() OVERRIDE;
  virtual bool ShouldProcessUndoMostVisitedDeletion() OVERRIDE;
  virtual bool ShouldProcessUndoAllMostVisitedDeletions() OVERRIDE;

  void set_should_handle_messages(bool should_handle_messages);

  using InstantPage::SetContents;

 private:
  // Initialized to true to handle the messages sent by the renderer.
  bool should_handle_messages_;

  DISALLOW_COPY_AND_ASSIGN(FakePage);
};

FakePage::FakePage(Delegate* delegate, const std::string& instant_url,
                   Profile* profile, bool is_incognito)
    : InstantPage(delegate, instant_url, profile, is_incognito),
      should_handle_messages_(true) {
}

FakePage::~FakePage() {
}

bool FakePage::ShouldProcessDeleteMostVisitedItem() {
  return should_handle_messages_;
}

bool FakePage::ShouldProcessUndoMostVisitedDeletion() {
  return should_handle_messages_;
}

bool FakePage::ShouldProcessUndoAllMostVisitedDeletions() {
  return should_handle_messages_;
}

void FakePage::set_should_handle_messages(bool should_handle_messages) {
  should_handle_messages_ = should_handle_messages;
}

}  // namespace

class InstantPageTest : public ChromeRenderViewHostTestHarness {
 public:
  virtual void SetUp() OVERRIDE;

  bool MessageWasSent(uint32 id) {
    return process()->sink().GetFirstMessageMatching(id) != NULL;
  }

  scoped_ptr<FakePage> page;
  FakePageDelegate delegate;
};

void InstantPageTest::SetUp() {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableInstantExtendedAPI);
  ChromeRenderViewHostTestHarness::SetUp();
  SearchTabHelper::CreateForWebContents(web_contents());
}

TEST_F(InstantPageTest, IsLocal) {
  page.reset(new FakePage(&delegate, "", NULL, false));
  EXPECT_FALSE(page->supports_instant());
  EXPECT_FALSE(page->IsLocal());
  page->SetContents(web_contents());
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(page->IsLocal());
  NavigateAndCommit(GURL("http://example.com"));
  EXPECT_FALSE(page->IsLocal());
}

TEST_F(InstantPageTest, DetermineIfPageSupportsInstant_Local) {
  page.reset(new FakePage(&delegate, "", NULL, false));
  EXPECT_FALSE(page->supports_instant());
  page->SetContents(web_contents());
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(page->IsLocal());
  EXPECT_CALL(delegate, InstantSupportDetermined(web_contents(), true))
      .Times(1);
  SearchTabHelper::FromWebContents(web_contents())->
      DetermineIfPageSupportsInstant();
  EXPECT_TRUE(page->supports_instant());
}

TEST_F(InstantPageTest, DetermineIfPageSupportsInstant_NonLocal) {
  page.reset(new FakePage(&delegate, "", NULL, false));
  EXPECT_FALSE(page->supports_instant());
  page->SetContents(web_contents());
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(page->IsLocal());
  process()->sink().ClearMessages();
  SearchTabHelper::FromWebContents(web_contents())->
      DetermineIfPageSupportsInstant();
  const IPC::Message* message = process()->sink().GetFirstMessageMatching(
      ChromeViewMsg_DetermineIfPageSupportsInstant::ID);
  ASSERT_TRUE(message != NULL);
  EXPECT_EQ(web_contents()->GetRoutingID(), message->routing_id());
}

TEST_F(InstantPageTest, DispatchRequestToDeleteMostVisitedItem) {
  page.reset(new FakePage(&delegate, "", NULL, false));
  page->SetContents(web_contents());
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  GURL item_url("www.foo.com");
  int page_id = web_contents()->GetController().GetActiveEntry()->GetPageID();
  EXPECT_CALL(delegate, DeleteMostVisitedItem(item_url)).Times(1);
  EXPECT_TRUE(page->OnMessageReceived(
      ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(rvh()->GetRoutingID(),
                                                       page_id, item_url)));
}

TEST_F(InstantPageTest, DispatchRequestToUndoMostVisitedDeletion) {
  page.reset(new FakePage(&delegate, "", NULL, false));
  page->SetContents(web_contents());
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  GURL item_url("www.foo.com");
  int page_id = web_contents()->GetController().GetActiveEntry()->GetPageID();
  EXPECT_CALL(delegate, UndoMostVisitedDeletion(item_url)).Times(1);
  EXPECT_TRUE(page->OnMessageReceived(
      ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(rvh()->GetRoutingID(),
                                                         page_id, item_url)));
}

TEST_F(InstantPageTest, DispatchRequestToUndoAllMostVisitedDeletions) {
  page.reset(new FakePage(&delegate, "", NULL, false));
  page->SetContents(web_contents());
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  int page_id = web_contents()->GetController().GetActiveEntry()->GetPageID();
  EXPECT_CALL(delegate, UndoAllMostVisitedDeletions()).Times(1);
  EXPECT_TRUE(page->OnMessageReceived(
      ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
          rvh()->GetRoutingID(), page_id)));
}

TEST_F(InstantPageTest, IgnoreMessageReceivedFromIncognitoPage) {
  page.reset(new FakePage(&delegate, "", NULL, true));
  page->SetContents(web_contents());
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  GURL item_url("www.foo.com");
  int page_id = web_contents()->GetController().GetActiveEntry()->GetPageID();

  EXPECT_CALL(delegate, DeleteMostVisitedItem(item_url)).Times(0);
  EXPECT_FALSE(page->OnMessageReceived(
      ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(rvh()->GetRoutingID(),
                                                       page_id,
                                                       item_url)));

  EXPECT_CALL(delegate, UndoMostVisitedDeletion(item_url)).Times(0);
  EXPECT_FALSE(page->OnMessageReceived(
      ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(rvh()->GetRoutingID(),
                                                         page_id,
                                                         item_url)));

  EXPECT_CALL(delegate, UndoAllMostVisitedDeletions()).Times(0);
  EXPECT_FALSE(page->OnMessageReceived(
      ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
          rvh()->GetRoutingID(), page_id)));
}

TEST_F(InstantPageTest, IgnoreMessageIfThePageIsNotActive) {
  page.reset(new FakePage(&delegate, "", NULL, false));
  page->SetContents(web_contents());
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  GURL item_url("www.foo.com");
  int inactive_page_id = 1999;

  EXPECT_CALL(delegate, DeleteMostVisitedItem(item_url)).Times(0);
  EXPECT_TRUE(page->OnMessageReceived(
      ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(rvh()->GetRoutingID(),
                                                       inactive_page_id,
                                                       item_url)));

  EXPECT_CALL(delegate, UndoMostVisitedDeletion(item_url)).Times(0);
  EXPECT_TRUE(page->OnMessageReceived(
      ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(rvh()->GetRoutingID(),
                                                         inactive_page_id,
                                                         item_url)));

  EXPECT_CALL(delegate, UndoAllMostVisitedDeletions()).Times(0);
  EXPECT_TRUE(page->OnMessageReceived(
      ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
          rvh()->GetRoutingID(), inactive_page_id)));
}

TEST_F(InstantPageTest, IgnoreMessageReceivedFromThePage) {
  page.reset(new FakePage(&delegate, "", NULL, false));
  page->SetContents(web_contents());

  // Ignore the messages received from the page.
  page->set_should_handle_messages(false);
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  GURL item_url("www.foo.com");
  int page_id = web_contents()->GetController().GetActiveEntry()->GetPageID();

  EXPECT_CALL(delegate, DeleteMostVisitedItem(item_url)).Times(0);
  EXPECT_TRUE(page->OnMessageReceived(
      ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(rvh()->GetRoutingID(),
                                                       page_id, item_url)));

  EXPECT_CALL(delegate, UndoMostVisitedDeletion(item_url)).Times(0);
  EXPECT_TRUE(page->OnMessageReceived(
      ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(rvh()->GetRoutingID(),
                                                         page_id, item_url)));

  EXPECT_CALL(delegate, UndoAllMostVisitedDeletions()).Times(0);
  EXPECT_TRUE(page->OnMessageReceived(
      ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
          rvh()->GetRoutingID(), page_id)));
}

TEST_F(InstantPageTest, PageURLDoesntBelongToInstantRenderer) {
  page.reset(new FakePage(&delegate, "", NULL, false));
  EXPECT_FALSE(page->supports_instant());
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  page->SetContents(web_contents());

  // Navigate to a page URL that doesn't belong to Instant renderer.
  // SearchTabHelper::DeterminerIfPageSupportsInstant() should return
  // immediately without dispatching any message to the renderer.
  NavigateAndCommit(GURL("http://www.example.com"));
  EXPECT_FALSE(page->IsLocal());
  process()->sink().ClearMessages();
  EXPECT_CALL(delegate, InstantSupportDetermined(web_contents(), false))
      .Times(1);

  SearchTabHelper::FromWebContents(web_contents())->
      DetermineIfPageSupportsInstant();
  const IPC::Message* message = process()->sink().GetFirstMessageMatching(
      ChromeViewMsg_DetermineIfPageSupportsInstant::ID);
  ASSERT_TRUE(message == NULL);
  EXPECT_FALSE(page->supports_instant());
}

// Test to verify that ChromeViewMsg_DetermineIfPageSupportsInstant message
// reply handler updates the instant support state in InstantPage.
TEST_F(InstantPageTest, PageSupportsInstant) {
  page.reset(new FakePage(&delegate, "", NULL, false));
  EXPECT_FALSE(page->supports_instant());
  page->SetContents(web_contents());
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  process()->sink().ClearMessages();
  SearchTabHelper::FromWebContents(web_contents())->
      DetermineIfPageSupportsInstant();
  const IPC::Message* message = process()->sink().GetFirstMessageMatching(
      ChromeViewMsg_DetermineIfPageSupportsInstant::ID);
  ASSERT_TRUE(message != NULL);
  EXPECT_EQ(web_contents()->GetRoutingID(), message->routing_id());

  EXPECT_CALL(delegate, InstantSupportDetermined(web_contents(), true))
      .Times(1);

  // Assume the page supports instant. Invoke the message reply handler to make
  // sure the InstantPage is notified about the instant support state.
  const content::NavigationEntry* entry =
      web_contents()->GetController().GetActiveEntry();
  EXPECT_TRUE(entry);
  SearchTabHelper::FromWebContents(web_contents())->InstantSupportChanged(true);
  EXPECT_TRUE(page->supports_instant());
}

TEST_F(InstantPageTest, AppropriateMessagesSentToIncognitoPages) {
  page.reset(new FakePage(&delegate, "", NULL, true));
  page->SetContents(web_contents());
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  process()->sink().ClearMessages();

  // Incognito pages should get these messages.
  page->sender()->Submit(string16());
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxSubmit::ID));
  page->sender()->SetOmniboxBounds(gfx::Rect());
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxMarginChange::ID));
  page->sender()->ToggleVoiceSearch();
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxToggleVoiceSearch::ID));

  // Incognito pages should not get any others.
  page->sender()->SetFontInformation(string16(), 0);
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxFontInformation::ID));

  page->sender()->SendThemeBackgroundInfo(ThemeBackgroundInfo());
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxThemeChanged::ID));

  page->sender()->FocusChanged(
      OMNIBOX_FOCUS_NONE, OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxFocusChanged::ID));

  page->sender()->SetInputInProgress(false);
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxSetInputInProgress::ID));

  page->sender()->SendMostVisitedItems(std::vector<InstantMostVisitedItem>());
  EXPECT_FALSE(MessageWasSent(
      ChromeViewMsg_SearchBoxMostVisitedItemsChanged::ID));
}
