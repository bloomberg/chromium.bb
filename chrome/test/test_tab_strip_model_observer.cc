// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/test_tab_strip_model_observer.h"

#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

TestTabStripModelObserver::LoadStartObserver::LoadStartObserver() {
}

TestTabStripModelObserver::LoadStartObserver::~LoadStartObserver() {
}

TestTabStripModelObserver::TestTabStripModelObserver(
    TabStripModel* tab_strip_model,
    TestTabStripModelObserver::LoadStartObserver* load_start_observer)
    : navigation_started_(false),
      navigations_completed_(0),
      number_of_navigations_(1),
      tab_strip_model_(tab_strip_model),
      load_start_observer_(load_start_observer),
      done_(false),
      running_(false) {
  tab_strip_model_->AddObserver(this);
}

TestTabStripModelObserver::~TestTabStripModelObserver() {
  tab_strip_model_->RemoveObserver(this);
}

void TestTabStripModelObserver::WaitForObservation() {
  if (!done_) {
    EXPECT_FALSE(running_);
    running_ = true;
    ui_test_utils::RunMessageLoop();
  }
}

void TestTabStripModelObserver::TabInsertedAt(
    TabContentsWrapper* contents, int index, bool foreground) {
  NavigationController* controller = &contents->controller();
  registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(controller));
  registrar_.Add(this, NotificationType::LOAD_START,
                 Source<NavigationController>(controller));
  registrar_.Add(this, NotificationType::LOAD_STOP,
                 Source<NavigationController>(controller));
}

void TestTabStripModelObserver::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  if (type == NotificationType::NAV_ENTRY_COMMITTED ||
      type == NotificationType::LOAD_START) {
    if (!navigation_started_) {
      load_start_observer_->OnLoadStart();
      navigation_started_ = true;
    }
  } else if (type == NotificationType::LOAD_STOP) {
    if (navigation_started_ &&
        ++navigations_completed_ == number_of_navigations_) {
      navigation_started_ = false;
      done_ = true;
      if (running_)
        MessageLoopForUI::current()->Quit();
    }
  }
}
