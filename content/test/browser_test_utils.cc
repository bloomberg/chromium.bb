// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test_utils.h"

#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TitleWatcher::TitleWatcher(WebContents* web_contents,
                           const string16& expected_title)
    : web_contents_(web_contents),
      expected_title_observed_(false),
      quit_loop_on_observation_(false) {
  EXPECT_TRUE(web_contents != NULL);
  expected_titles_.push_back(expected_title);
  notification_registrar_.Add(this,
                              NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED,
                              Source<WebContents>(web_contents));

  // When navigating through the history, the restored NavigationEntry's title
  // will be used. If the entry ends up having the same title after we return
  // to it, as will usually be the case, the
  // NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED will then be suppressed, since the
  // NavigationEntry's title hasn't changed.
  notification_registrar_.Add(
      this,
      NOTIFICATION_LOAD_STOP,
      Source<NavigationController>(&web_contents->GetController()));
}

void TitleWatcher::AlsoWaitForTitle(const string16& expected_title) {
  expected_titles_.push_back(expected_title);
}

TitleWatcher::~TitleWatcher() {
}

const string16& TitleWatcher::WaitAndGetTitle() {
  if (expected_title_observed_)
    return observed_title_;
  quit_loop_on_observation_ = true;
  message_loop_runner_ = new MessageLoopRunner;
  message_loop_runner_->Run();
  return observed_title_;
}

void TitleWatcher::Observe(int type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  if (type == NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED) {
    WebContents* source_contents = Source<WebContents>(source).ptr();
    ASSERT_EQ(web_contents_, source_contents);
  } else if (type == NOTIFICATION_LOAD_STOP) {
    NavigationController* controller =
        Source<NavigationController>(source).ptr();
    ASSERT_EQ(&web_contents_->GetController(), controller);
  } else {
    FAIL() << "Unexpected notification received.";
  }

  std::vector<string16>::const_iterator it =
      std::find(expected_titles_.begin(),
                expected_titles_.end(),
                web_contents_->GetTitle());
  if (it == expected_titles_.end())
    return;
  observed_title_ = *it;
  expected_title_observed_ = true;
  if (quit_loop_on_observation_) {
    // Only call Quit once, on first Observe:
    quit_loop_on_observation_ = false;
    message_loop_runner_->Quit();
  }
}

}  // namespace content
