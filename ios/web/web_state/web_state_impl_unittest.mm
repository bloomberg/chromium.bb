// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/web_state_impl.h"

#include <stddef.h>

#include <memory>

#include "base/base64.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "base/values.h"
#include "ios/web/public/load_committed_details.h"
#include "ios/web/public/test/test_browser_state.h"
#include "ios/web/public/web_state/global_web_state_observer.h"
#include "ios/web/public/web_state/web_state_delegate.h"
#include "ios/web/public/web_state/web_state_observer.h"
#include "ios/web/public/web_state/web_state_policy_decider.h"
#import "ios/web/test/web_test.h"
#include "ios/web/web_state/global_web_state_event_tracker.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

using testing::_;
using testing::Assign;
using testing::AtMost;
using testing::DoAll;
using testing::Return;

namespace web {
namespace {

// Test observer to check that the GlobalWebStateObserver methods are called as
// expected.
class TestGlobalWebStateObserver : public GlobalWebStateObserver {
 public:
  TestGlobalWebStateObserver()
      : GlobalWebStateObserver(),
        navigation_items_pruned_called_(false),
        navigation_item_changed_called_(false),
        navigation_item_committed_called_(false),
        did_start_loading_called_(false),
        did_stop_loading_called_(false),
        page_loaded_called_with_success_(false),
        web_state_destroyed_called_(false) {}

  // Methods returning true if the corresponding GlobalWebStateObserver method
  // has been called.
  bool navigation_items_pruned_called() const {
    return navigation_items_pruned_called_;
  }
  bool navigation_item_changed_called() const {
    return navigation_item_changed_called_;
  }
  bool navigation_item_committed_called() const {
    return navigation_item_committed_called_;
  }
  bool did_start_loading_called() const { return did_start_loading_called_; }
  bool did_stop_loading_called() const { return did_stop_loading_called_; }
  bool page_loaded_called_with_success() const {
    return page_loaded_called_with_success_;
  }
  bool web_state_destroyed_called() const {
    return web_state_destroyed_called_;
  }

 private:
  // GlobalWebStateObserver implementation:
  void NavigationItemsPruned(WebState* web_state,
                             size_t pruned_item_count) override {
    navigation_items_pruned_called_ = true;
  }
  void NavigationItemChanged(WebState* web_state) override {
    navigation_item_changed_called_ = true;
  }
  void NavigationItemCommitted(
      WebState* web_state,
      const LoadCommittedDetails& load_details) override {
    navigation_item_committed_called_ = true;
  }
  void WebStateDidStartLoading(WebState* web_state) override {
    did_start_loading_called_ = true;
  }
  void WebStateDidStopLoading(WebState* web_state) override {
    did_stop_loading_called_ = true;
  }
  void PageLoaded(WebState* web_state,
                  PageLoadCompletionStatus load_completion_status) override {
    page_loaded_called_with_success_ =
        load_completion_status == PageLoadCompletionStatus::SUCCESS;
  }
  void WebStateDestroyed(WebState* web_state) override {
    web_state_destroyed_called_ = true;
  }

  bool navigation_items_pruned_called_;
  bool navigation_item_changed_called_;
  bool navigation_item_committed_called_;
  bool did_start_loading_called_;
  bool did_stop_loading_called_;
  bool page_loaded_called_with_success_;
  bool web_state_destroyed_called_;
};

// Test delegate to check that the WebStateDelegate methods are called as
// expected.
class TestWebStateDelegate : public WebStateDelegate {
 public:
  TestWebStateDelegate() : load_progress_changed_called_(false) {}

  // Methods returning true if the corresponding WebStateObserver method has
  // been called.
  bool load_progress_changed_called() const {
    return load_progress_changed_called_;
  }

 private:
  // WebStateObserver implementation:
  void LoadProgressChanged(WebState* source, double progress) override {
    load_progress_changed_called_ = true;
  }

  bool load_progress_changed_called_;
};

// Test observer to check that the WebStateObserver methods are called as
// expected.
class TestWebStateObserver : public WebStateObserver {
 public:
  TestWebStateObserver(WebState* web_state)
      : WebStateObserver(web_state),
        provisional_navigation_started_called_(false),
        navigation_items_pruned_called_(false),
        navigation_item_changed_called_(false),
        navigation_item_committed_called_(false),
        page_loaded_called_with_success_(false),
        url_hash_changed_called_(false),
        history_state_changed_called_(false),
        web_state_destroyed_called_(false) {}

  // Methods returning true if the corresponding WebStateObserver method has
  // been called.
  bool provisional_navigation_started_called() const {
    return provisional_navigation_started_called_;
  };
  bool navigation_items_pruned_called() const {
    return navigation_items_pruned_called_;
  }
  bool navigation_item_changed_called() const {
    return navigation_item_changed_called_;
  }
  bool navigation_item_committed_called() const {
    return navigation_item_committed_called_;
  }
  bool page_loaded_called_with_success() const {
    return page_loaded_called_with_success_;
  }
  bool url_hash_changed_called() const { return url_hash_changed_called_; }
  bool history_state_changed_called() const {
    return history_state_changed_called_;
  }
  bool web_state_destroyed_called() const {
    return web_state_destroyed_called_;
  }

 private:
  // WebStateObserver implementation:
  void ProvisionalNavigationStarted(const GURL& url) override {
    provisional_navigation_started_called_ = true;
  }
  void NavigationItemsPruned(size_t pruned_item_count) override {
    navigation_items_pruned_called_ = true;
  }
  void NavigationItemChanged() override {
    navigation_item_changed_called_ = true;
  }
  void NavigationItemCommitted(
      const LoadCommittedDetails& load_details) override {
    navigation_item_committed_called_ = true;
  }
  void PageLoaded(PageLoadCompletionStatus load_completion_status) override {
    page_loaded_called_with_success_ =
        load_completion_status == PageLoadCompletionStatus::SUCCESS;
  }
  void UrlHashChanged() override { url_hash_changed_called_ = true; }
  void HistoryStateChanged() override { history_state_changed_called_ = true; }
  void WebStateDestroyed() override {
    EXPECT_TRUE(web_state()->IsBeingDestroyed());
    web_state_destroyed_called_ = true;
    Observe(nullptr);
  }

  bool provisional_navigation_started_called_;
  bool navigation_items_pruned_called_;
  bool navigation_item_changed_called_;
  bool navigation_item_committed_called_;
  bool page_loaded_called_with_success_;
  bool url_hash_changed_called_;
  bool history_state_changed_called_;
  bool web_state_destroyed_called_;
};

// Test decider to check that the WebStatePolicyDecider methods are called as
// expected.
class MockWebStatePolicyDecider : public WebStatePolicyDecider {
 public:
  explicit MockWebStatePolicyDecider(WebState* web_state)
      : WebStatePolicyDecider(web_state) {}
  virtual ~MockWebStatePolicyDecider() {}

  MOCK_METHOD1(ShouldAllowRequest, bool(NSURLRequest* request));
  MOCK_METHOD1(ShouldAllowResponse, bool(NSURLResponse* response));
  MOCK_METHOD0(WebStateDestroyed, void());
};

// Creates and returns an HttpResponseHeader using the string representation.
scoped_refptr<net::HttpResponseHeaders> HeadersFromString(const char* string) {
  std::string raw_string(string);
  std::string headers_string = net::HttpUtil::AssembleRawHeaders(
      raw_string.c_str(), raw_string.length());
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(headers_string));
  return headers;
}

// Test callback for script commands.
// Sets |is_called| to true if it is called, and checks that the parameters
// match their expected values.
// |user_is_interacting| is not checked because Bind() has a maximum of 7
// parameters.
bool HandleScriptCommand(bool* is_called,
                         bool should_handle,
                         base::DictionaryValue* expected_value,
                         const GURL& expected_url,
                         const base::DictionaryValue& value,
                         const GURL& url,
                         bool user_is_interacting) {
  *is_called = true;
  EXPECT_TRUE(expected_value->Equals(&value));
  EXPECT_EQ(expected_url, url);
  return should_handle;
}

class WebStateTest : public web::WebTest {
 protected:
  void SetUp() override {
    web_state_.reset(new WebStateImpl(&browser_state_));
  }

  // Loads specified html page into WebState.
  void LoadHtml(std::string html) {
    web_state_->GetNavigationManagerImpl().InitializeSession(nil, nil, NO, 0);

    // Use data: url for loading html page.
    std::string encoded_html;
    base::Base64Encode(html, &encoded_html);
    GURL url("data:text/html;charset=utf8;base64," + encoded_html);
    web::NavigationManager::WebLoadParams params(url);
    web_state_->GetNavigationManager()->LoadURLWithParams(params);

    // Trigger the load.
    web_state_->GetWebController().webUsageEnabled = YES;
    web_state_->GetView();

    // Wait until load is completed.
    EXPECT_TRUE(web_state_->IsLoading());
    base::test::ios::WaitUntilCondition(^bool() {
      return !web_state_->IsLoading();
    });
  }

  web::TestBrowserState browser_state_;
  std::unique_ptr<WebStateImpl> web_state_;
};

TEST_F(WebStateTest, WebUsageEnabled) {
  // Default is false.
  ASSERT_FALSE(web_state_->IsWebUsageEnabled());

  web_state_->SetWebUsageEnabled(true);
  EXPECT_TRUE(web_state_->IsWebUsageEnabled());
  EXPECT_TRUE(web_state_->GetWebController().webUsageEnabled);

  web_state_->SetWebUsageEnabled(false);
  EXPECT_FALSE(web_state_->IsWebUsageEnabled());
  EXPECT_FALSE(web_state_->GetWebController().webUsageEnabled);
}

TEST_F(WebStateTest, ResponseHeaders) {
  GURL real_url("http://foo.com/bar");
  GURL frame_url("http://frames-r-us.com/");
  scoped_refptr<net::HttpResponseHeaders> real_headers(HeadersFromString(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html\r\n"
      "Content-Language: en\r\n"
      "X-Should-Be-Here: yep\r\n"
      "\r\n"));
  scoped_refptr<net::HttpResponseHeaders> frame_headers(HeadersFromString(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/pdf\r\n"
      "Content-Language: fr\r\n"
      "X-Should-Not-Be-Here: oops\r\n"
      "\r\n"));
  // Simulate a load of a page with a frame.
  web_state_->OnHttpResponseHeadersReceived(real_headers.get(), real_url);
  web_state_->OnHttpResponseHeadersReceived(frame_headers.get(), frame_url);
  // Include a hash to be sure it's handled correctly.
  web_state_->OnNavigationCommitted(
      GURL(real_url.spec() + std::string("#baz")));

  // Verify that the right header set was kept.
  EXPECT_TRUE(
      web_state_->GetHttpResponseHeaders()->HasHeader("X-Should-Be-Here"));
  EXPECT_FALSE(
      web_state_->GetHttpResponseHeaders()->HasHeader("X-Should-Not-Be-Here"));

  // And that it was parsed correctly.
  EXPECT_EQ("text/html", web_state_->GetContentsMimeType());
  EXPECT_EQ("en", web_state_->GetContentLanguageHeader());
}

TEST_F(WebStateTest, ResponseHeaderClearing) {
  GURL url("http://foo.com/");
  scoped_refptr<net::HttpResponseHeaders> headers(HeadersFromString(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html\r\n"
      "Content-Language: en\r\n"
      "\r\n"));
  web_state_->OnHttpResponseHeadersReceived(headers.get(), url);

  // There should be no headers before loading.
  EXPECT_EQ(NULL, web_state_->GetHttpResponseHeaders());

  // There should be headers and parsed values after loading.
  web_state_->OnNavigationCommitted(url);
  EXPECT_TRUE(web_state_->GetHttpResponseHeaders()->HasHeader("Content-Type"));
  EXPECT_NE("", web_state_->GetContentsMimeType());
  EXPECT_NE("", web_state_->GetContentLanguageHeader());

  // ... but not after loading another page, nor should there be specific
  // parsed values.
  web_state_->OnNavigationCommitted(GURL("http://elsewhere.com/"));
  EXPECT_EQ(NULL, web_state_->GetHttpResponseHeaders());
  EXPECT_EQ("", web_state_->GetContentsMimeType());
  EXPECT_EQ("", web_state_->GetContentLanguageHeader());
}

TEST_F(WebStateTest, ObserverTest) {
  std::unique_ptr<TestWebStateObserver> observer(
      new TestWebStateObserver(web_state_.get()));
  EXPECT_EQ(web_state_.get(), observer->web_state());

  // Test that ProvisionalNavigationStarted() is called.
  EXPECT_FALSE(observer->provisional_navigation_started_called());
  web_state_->OnProvisionalNavigationStarted(GURL("http://test"));
  EXPECT_TRUE(observer->provisional_navigation_started_called());

  // Test that NavigationItemsPruned() is called.
  EXPECT_FALSE(observer->navigation_items_pruned_called());
  web_state_->OnNavigationItemsPruned(1);
  EXPECT_TRUE(observer->navigation_items_pruned_called());

  // Test that NavigationItemChanged() is called.
  EXPECT_FALSE(observer->navigation_item_changed_called());
  web_state_->OnNavigationItemChanged();
  EXPECT_TRUE(observer->navigation_item_changed_called());

  // Test that NavigationItemCommitted() is called.
  EXPECT_FALSE(observer->navigation_item_committed_called());
  LoadCommittedDetails details;
  web_state_->OnNavigationItemCommitted(details);
  EXPECT_TRUE(observer->navigation_item_committed_called());

  // Test that OnPageLoaded() is called with success when there is no error.
  EXPECT_FALSE(observer->page_loaded_called_with_success());
  web_state_->OnPageLoaded(GURL("http://test"), false);
  EXPECT_FALSE(observer->page_loaded_called_with_success());
  web_state_->OnPageLoaded(GURL("http://test"), true);
  EXPECT_TRUE(observer->page_loaded_called_with_success());

  // Test that UrlHashChanged() is called.
  EXPECT_FALSE(observer->url_hash_changed_called());
  web_state_->OnUrlHashChanged();
  EXPECT_TRUE(observer->url_hash_changed_called());

  // Test that HistoryStateChanged() is called.
  EXPECT_FALSE(observer->history_state_changed_called());
  web_state_->OnHistoryStateChanged();
  EXPECT_TRUE(observer->history_state_changed_called());

  // Test that WebStateDestroyed() is called.
  EXPECT_FALSE(observer->web_state_destroyed_called());
  web_state_.reset();
  EXPECT_TRUE(observer->web_state_destroyed_called());

  EXPECT_EQ(nullptr, observer->web_state());
}

// Tests that WebStateDelegate methods appropriately called.
TEST_F(WebStateTest, DelegateTest) {
  TestWebStateDelegate delegate;
  web_state_->SetDelegate(&delegate);

  // Test that LoadProgressChanged() is called.
  EXPECT_FALSE(delegate.load_progress_changed_called());
  web_state_->SendChangeLoadProgress(0.0);
  EXPECT_TRUE(delegate.load_progress_changed_called());
}

// Verifies that GlobalWebStateObservers are called when expected.
TEST_F(WebStateTest, GlobalObserverTest) {
  std::unique_ptr<TestGlobalWebStateObserver> observer(
      new TestGlobalWebStateObserver());

  // Test that NavigationItemsPruned() is called.
  EXPECT_FALSE(observer->navigation_items_pruned_called());
  web_state_->OnNavigationItemsPruned(1);
  EXPECT_TRUE(observer->navigation_items_pruned_called());

  // Test that NavigationItemChanged() is called.
  EXPECT_FALSE(observer->navigation_item_changed_called());
  web_state_->OnNavigationItemChanged();
  EXPECT_TRUE(observer->navigation_item_changed_called());

  // Test that NavigationItemCommitted() is called.
  EXPECT_FALSE(observer->navigation_item_committed_called());
  LoadCommittedDetails details;
  web_state_->OnNavigationItemCommitted(details);
  EXPECT_TRUE(observer->navigation_item_committed_called());

  // Test that WebStateDidStartLoading() is called.
  EXPECT_FALSE(observer->did_start_loading_called());
  web_state_->SetIsLoading(true);
  EXPECT_TRUE(observer->did_start_loading_called());

  // Test that WebStateDidStopLoading() is called.
  EXPECT_FALSE(observer->did_stop_loading_called());
  web_state_->SetIsLoading(false);
  EXPECT_TRUE(observer->did_stop_loading_called());

  // Test that OnPageLoaded() is called with success when there is no error.
  EXPECT_FALSE(observer->page_loaded_called_with_success());
  web_state_->OnPageLoaded(GURL("http://test"), false);
  EXPECT_FALSE(observer->page_loaded_called_with_success());
  web_state_->OnPageLoaded(GURL("http://test"), true);
  EXPECT_TRUE(observer->page_loaded_called_with_success());

  // Test that WebStateDestroyed() is called.
  EXPECT_FALSE(observer->web_state_destroyed_called());
  web_state_.reset();
  EXPECT_TRUE(observer->web_state_destroyed_called());
}

// Verifies that policy deciders are correctly called by the web state.
TEST_F(WebStateTest, PolicyDeciderTest) {
  MockWebStatePolicyDecider decider(web_state_.get());
  MockWebStatePolicyDecider decider2(web_state_.get());
  EXPECT_EQ(web_state_.get(), decider.web_state());

  // Test that ShouldAllowRequest() is called.
  EXPECT_CALL(decider, ShouldAllowRequest(_)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(decider2, ShouldAllowRequest(_)).Times(1).WillOnce(Return(true));
  EXPECT_TRUE(web_state_->ShouldAllowRequest(nil));

  // Test that ShouldAllowRequest() is stopping on negative answer. Only one
  // one the decider should be called.
  {
    bool decider_called = false;
    bool decider2_called = false;
    EXPECT_CALL(decider, ShouldAllowRequest(_))
        .Times(AtMost(1))
        .WillOnce(DoAll(Assign(&decider_called, true), Return(false)));
    EXPECT_CALL(decider2, ShouldAllowRequest(_))
        .Times(AtMost(1))
        .WillOnce(DoAll(Assign(&decider2_called, true), Return(false)));
    EXPECT_FALSE(web_state_->ShouldAllowRequest(nil));
    EXPECT_FALSE(decider_called && decider2_called);
  }

  // Test that ShouldAllowResponse() is called.
  EXPECT_CALL(decider, ShouldAllowResponse(_)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(decider2, ShouldAllowResponse(_)).Times(1).WillOnce(Return(true));
  EXPECT_TRUE(web_state_->ShouldAllowResponse(nil));

  // Test that ShouldAllowResponse() is stopping on negative answer. Only one
  // one the decider should be called.
  {
    bool decider_called = false;
    bool decider2_called = false;
    EXPECT_CALL(decider, ShouldAllowResponse(_))
        .Times(AtMost(1))
        .WillOnce(DoAll(Assign(&decider_called, true), Return(false)));
    EXPECT_CALL(decider2, ShouldAllowResponse(_))
        .Times(AtMost(1))
        .WillOnce(DoAll(Assign(&decider2_called, true), Return(false)));
    EXPECT_FALSE(web_state_->ShouldAllowResponse(nil));
    EXPECT_FALSE(decider_called && decider2_called);
  }

  // Test that WebStateDestroyed() is called.
  EXPECT_CALL(decider, WebStateDestroyed()).Times(1);
  EXPECT_CALL(decider2, WebStateDestroyed()).Times(1);
  web_state_.reset();
  EXPECT_EQ(nullptr, decider.web_state());
}

// Tests that script command callbacks are called correctly.
TEST_F(WebStateTest, ScriptCommand) {
  // Set up two script command callbacks.
  const std::string kPrefix1("prefix1");
  const std::string kCommand1("prefix1.command1");
  base::DictionaryValue value_1;
  value_1.SetString("a", "b");
  const GURL kUrl1("http://foo");
  bool is_called_1 = false;
  web_state_->AddScriptCommandCallback(
      base::Bind(&HandleScriptCommand, &is_called_1, true, &value_1, kUrl1),
      kPrefix1);

  const std::string kPrefix2("prefix2");
  const std::string kCommand2("prefix2.command2");
  base::DictionaryValue value_2;
  value_2.SetString("c", "d");
  const GURL kUrl2("http://bar");
  bool is_called_2 = false;
  web_state_->AddScriptCommandCallback(
      base::Bind(&HandleScriptCommand, &is_called_2, false, &value_2, kUrl2),
      kPrefix2);

  // Check that a irrelevant or invalid command does not trigger the callbacks.
  EXPECT_FALSE(
      web_state_->OnScriptCommandReceived("wohoo.blah", value_1, kUrl1, false));
  EXPECT_FALSE(is_called_1);
  EXPECT_FALSE(is_called_2);

  EXPECT_FALSE(web_state_->OnScriptCommandReceived(
      "prefix1ButMissingDot", value_1, kUrl1, false));
  EXPECT_FALSE(is_called_1);
  EXPECT_FALSE(is_called_2);

  // Check that only the callback matching the prefix is called, with the
  // expected parameters and return value;
  EXPECT_TRUE(
      web_state_->OnScriptCommandReceived(kCommand1, value_1, kUrl1, false));
  EXPECT_TRUE(is_called_1);
  EXPECT_FALSE(is_called_2);

  // Remove the callback and check it is no longer called.
  is_called_1 = false;
  web_state_->RemoveScriptCommandCallback(kPrefix1);
  EXPECT_FALSE(
      web_state_->OnScriptCommandReceived(kCommand1, value_1, kUrl1, false));
  EXPECT_FALSE(is_called_1);
  EXPECT_FALSE(is_called_2);

  // Check that a false return value is forwarded correctly.
  EXPECT_FALSE(
      web_state_->OnScriptCommandReceived(kCommand2, value_2, kUrl2, false));
  EXPECT_FALSE(is_called_1);
  EXPECT_TRUE(is_called_2);

  web_state_->RemoveScriptCommandCallback(kPrefix2);
}

// Tests script execution with and without callback.
TEST_F(WebStateTest, ScriptExecution) {
  LoadHtml("<html></html>");

  // Execute script without callback.
  web_state_->ExecuteJavaScript(base::UTF8ToUTF16("window.foo = 'bar'"));

  // Execute script with callback.
  __block std::unique_ptr<base::Value> execution_result;
  web_state_->ExecuteJavaScript(base::UTF8ToUTF16("window.foo"),
                                base::BindBlock(^(const base::Value* value) {
                                  ASSERT_TRUE(value);
                                  execution_result = value->CreateDeepCopy();
                                }));
  base::test::ios::WaitUntilCondition(^bool() {
    return execution_result.get();
  });

  std::string string_result;
  execution_result->GetAsString(&string_result);
  EXPECT_EQ("bar", string_result);
}

}  // namespace
}  // namespace web
