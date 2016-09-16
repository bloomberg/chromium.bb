// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_navigation_observer.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class TestNavigationObserver::TestWebContentsObserver
    : public WebContentsObserver {
 public:
  TestWebContentsObserver(TestNavigationObserver* parent,
                          WebContents* web_contents)
      : WebContentsObserver(web_contents),
        parent_(parent) {
  }

 private:
  // WebContentsObserver:
  void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override {
    parent_->OnNavigationEntryCommitted(this, web_contents(), load_details);
  }

  void DidAttachInterstitialPage() override {
    parent_->OnDidAttachInterstitialPage(web_contents());
  }

  void WebContentsDestroyed() override {
    parent_->OnWebContentsDestroyed(this, web_contents());
  }

  void DidStartLoading() override {
    parent_->OnDidStartLoading(web_contents());
  }

  void DidStopLoading() override {
    parent_->OnDidStopLoading(web_contents());
  }

  void DidStartProvisionalLoadForFrame(RenderFrameHost* render_frame_host,
                                       const GURL& validated_url,
                                       bool is_error_page,
                                       bool is_iframe_srcdoc) override {
    parent_->OnDidStartProvisionalLoad(render_frame_host, validated_url,
                                       is_error_page, is_iframe_srcdoc);
  }

  void DidFailProvisionalLoad(
      RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description,
      bool was_ignored_by_handler) override {
    parent_->OnDidFailProvisionalLoad(render_frame_host, validated_url,
                                      error_code, error_description);
  }

  void DidCommitProvisionalLoadForFrame(
      RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) override {
    parent_->OnDidCommitProvisionalLoadForFrame(
        render_frame_host, url, transition_type);
  }

  TestNavigationObserver* parent_;

  DISALLOW_COPY_AND_ASSIGN(TestWebContentsObserver);
};

TestNavigationObserver::TestNavigationObserver(
    WebContents* web_contents,
    int number_of_navigations)
    : navigation_started_(false),
      navigations_completed_(0),
      number_of_navigations_(number_of_navigations),
      message_loop_runner_(new MessageLoopRunner),
      web_contents_created_callback_(
          base::Bind(
              &TestNavigationObserver::OnWebContentsCreated,
              base::Unretained(this))) {
  if (web_contents)
    RegisterAsObserver(web_contents);
}

TestNavigationObserver::TestNavigationObserver(
    WebContents* web_contents)
    : navigation_started_(false),
      navigations_completed_(0),
      number_of_navigations_(1),
      message_loop_runner_(new MessageLoopRunner),
      web_contents_created_callback_(
          base::Bind(
              &TestNavigationObserver::OnWebContentsCreated,
              base::Unretained(this))) {
  if (web_contents)
    RegisterAsObserver(web_contents);
}

TestNavigationObserver::~TestNavigationObserver() {
  StopWatchingNewWebContents();
}

void TestNavigationObserver::Wait() {
  message_loop_runner_->Run();
}

void TestNavigationObserver::StartWatchingNewWebContents() {
  WebContentsImpl::FriendZone::AddCreatedCallbackForTesting(
      web_contents_created_callback_);
}

void TestNavigationObserver::StopWatchingNewWebContents() {
  WebContentsImpl::FriendZone::RemoveCreatedCallbackForTesting(
      web_contents_created_callback_);
}

void TestNavigationObserver::RegisterAsObserver(WebContents* web_contents) {
  web_contents_observers_.insert(
      base::MakeUnique<TestWebContentsObserver>(this, web_contents));
}

void TestNavigationObserver::OnWebContentsCreated(WebContents* web_contents) {
  RegisterAsObserver(web_contents);
}

void TestNavigationObserver::OnWebContentsDestroyed(
    TestWebContentsObserver* observer,
    WebContents* web_contents) {
  web_contents_observers_.erase(std::find_if(
      web_contents_observers_.begin(), web_contents_observers_.end(),
      [observer](const std::unique_ptr<TestWebContentsObserver>& ptr) {
        return ptr.get() == observer;
      }));
}

void TestNavigationObserver::OnNavigationEntryCommitted(
    TestWebContentsObserver* observer,
    WebContents* web_contents,
    const LoadCommittedDetails& load_details) {
  navigation_started_ = true;
}

void TestNavigationObserver::OnDidAttachInterstitialPage(
    WebContents* web_contents) {
  // Going to an interstitial page does not trigger NavigationEntryCommitted,
  // but has the same meaning for us here.
  navigation_started_ = true;
}

void TestNavigationObserver::OnDidStartLoading(WebContents* web_contents) {
  navigation_started_ = true;
}

void TestNavigationObserver::OnDidStopLoading(WebContents* web_contents) {
  if (!navigation_started_)
    return;

  ++navigations_completed_;
  if (navigations_completed_ == number_of_navigations_) {
    navigation_started_ = false;
    message_loop_runner_->Quit();
  }
}

void TestNavigationObserver::OnDidStartProvisionalLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  last_navigation_succeeded_ = false;
}

void TestNavigationObserver::OnDidFailProvisionalLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  last_navigation_url_ = validated_url;
  last_navigation_succeeded_ = false;
}

void TestNavigationObserver::OnDidCommitProvisionalLoadForFrame(
    RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition transition_type) {
  last_navigation_url_ = url;
  last_navigation_succeeded_ = true;
}

}  // namespace content
