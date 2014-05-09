// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_navigation_observer.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
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
  virtual void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) OVERRIDE {
    parent_->OnNavigationEntryCommitted(this, web_contents(), load_details);
  }

  virtual void DidAttachInterstitialPage() OVERRIDE {
    parent_->OnDidAttachInterstitialPage(web_contents());
  }

  virtual void WebContentsDestroyed() OVERRIDE {
    parent_->OnWebContentsDestroyed(this, web_contents());
  }

  virtual void DidStartLoading(RenderViewHost* render_view_host) OVERRIDE {
    parent_->OnDidStartLoading(web_contents());
  }

  virtual void DidStopLoading(RenderViewHost* render_view_host) OVERRIDE {
    parent_->OnDidStopLoading(web_contents());
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

  STLDeleteContainerPointers(web_contents_observers_.begin(),
                             web_contents_observers_.end());
}

void TestNavigationObserver::Wait() {
  message_loop_runner_->Run();
}

void TestNavigationObserver::StartWatchingNewWebContents() {
  WebContentsImpl::AddCreatedCallback(web_contents_created_callback_);
}

void TestNavigationObserver::StopWatchingNewWebContents() {
  WebContentsImpl::RemoveCreatedCallback(web_contents_created_callback_);
}

void TestNavigationObserver::RegisterAsObserver(WebContents* web_contents) {
  web_contents_observers_.insert(
      new TestWebContentsObserver(this, web_contents));
}

void TestNavigationObserver::OnWebContentsCreated(WebContents* web_contents) {
  RegisterAsObserver(web_contents);
}

void TestNavigationObserver::OnWebContentsDestroyed(
    TestWebContentsObserver* observer,
    WebContents* web_contents) {
  web_contents_observers_.erase(observer);
  delete observer;
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

}  // namespace content
