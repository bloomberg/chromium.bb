// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_navigation_observer.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"
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

  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE {
    parent_->OnWebContentsDestroyed(this, web_contents);
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
      done_(false),
      running_(false),
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
      done_(false),
      running_(false),
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

void TestNavigationObserver::WaitForObservation(
    const base::Closure& wait_loop_callback,
    const base::Closure& done_callback) {
  if (done_)
    return;

  EXPECT_FALSE(running_);
  running_ = true;
  done_callback_ = done_callback;
  wait_loop_callback.Run();
  EXPECT_TRUE(done_);
}

void TestNavigationObserver::Wait() {
  base::RunLoop run_loop;
  WaitForObservation(
      base::Bind(&base::RunLoop::Run, base::Unretained(&run_loop)),
      GetQuitTaskForRunLoop(&run_loop));
}

void TestNavigationObserver::StartWatchingNewWebContents() {
  WebContents::AddCreatedCallback(web_contents_created_callback_);
}

void TestNavigationObserver::StopWatchingNewWebContents() {
  WebContents::RemoveCreatedCallback(web_contents_created_callback_);
}

void TestNavigationObserver::RegisterAsObserver(
    WebContents* web_contents) {
  web_contents_observers_.insert(
      new TestWebContentsObserver(this, web_contents));

  NotificationSource notification_source =
      Source<NavigationController>(&web_contents->GetController());
  registrar_.Add(this, NOTIFICATION_LOAD_START, notification_source);
  registrar_.Add(this, NOTIFICATION_LOAD_STOP, notification_source);
}

void TestNavigationObserver::Observe(
    int type, const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_LOAD_START:
      navigation_started_ = true;
      break;
    case NOTIFICATION_LOAD_STOP:
      if (navigation_started_ &&
          ++navigations_completed_ == number_of_navigations_) {
        navigation_started_ = false;
        done_ = true;
        if (running_) {
          running_ = false;
          done_callback_.Run();
        }
      }
      break;
    default:
      NOTREACHED();
  }
}

void TestNavigationObserver::OnWebContentsCreated(WebContents* web_contents) {
  RegisterAsObserver(web_contents);
}

void TestNavigationObserver::OnWebContentsDestroyed(
    TestWebContentsObserver* observer,
    WebContents* web_contents) {
  web_contents_observers_.erase(observer);
  delete observer;

  NotificationSource notification_source =
      Source<NavigationController>(&web_contents->GetController());
  registrar_.Remove(this, NOTIFICATION_LOAD_START, notification_source);
  registrar_.Remove(this, NOTIFICATION_LOAD_STOP, notification_source);
}

void TestNavigationObserver::OnNavigationEntryCommitted(
    TestWebContentsObserver* observer,
    WebContents* web_contents,
    const LoadCommittedDetails& load_details) {
  navigation_started_ = true;
}

}  // namespace content
