// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_chrome.h"
#include "chrome/test/chromedriver/chrome/stub_web_view.h"
#include "chrome/test/chromedriver/session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockChrome : public StubChrome {
 public:
  MockChrome() : web_view_("1") {}
  virtual ~MockChrome() {}

  virtual Status GetWebViewById(const std::string& id,
                                WebView** web_view) OVERRIDE {
    if (id == web_view_.GetId()) {
      *web_view = &web_view_;
      return Status(kOk);
    }
    return Status(kUnknownError);
  }

 private:
  StubWebView web_view_;
};

}  // namespace

TEST(Session, GetTargetWindowNoChrome) {
  Session session("1");
  WebView* web_view;
  ASSERT_EQ(kNoSuchWindow, session.GetTargetWindow(&web_view).code());
}

TEST(Session, GetTargetWindowTargetWindowClosed) {
  scoped_ptr<Chrome> chrome(new MockChrome());
  Session session("1", chrome.Pass());
  session.window = "2";
  WebView* web_view;
  ASSERT_EQ(kNoSuchWindow, session.GetTargetWindow(&web_view).code());
}

TEST(Session, GetTargetWindowTargetWindowStillOpen) {
  scoped_ptr<Chrome> chrome(new MockChrome());
  Session session("1", chrome.Pass());
  session.window = "1";
  WebView* web_view = NULL;
  ASSERT_EQ(kOk, session.GetTargetWindow(&web_view).code());
  ASSERT_TRUE(web_view);
}

TEST(Session, SwitchToParentFrame) {
  scoped_ptr<Chrome> chrome(new MockChrome());
  Session session("1", chrome.Pass());

  // Initial frame should be top frame.
  ASSERT_EQ(std::string(), session.GetCurrentFrameId());

  // Switching to parent frame should be a no-op.
  session.SwitchToParentFrame();
  ASSERT_EQ(std::string(), session.GetCurrentFrameId());

  session.SwitchToSubFrame("1.1", std::string());
  ASSERT_EQ("1.1", session.GetCurrentFrameId());
  session.SwitchToParentFrame();
  ASSERT_EQ(std::string(), session.GetCurrentFrameId());

  session.SwitchToSubFrame("2.1", std::string());
  ASSERT_EQ("2.1", session.GetCurrentFrameId());
  session.SwitchToSubFrame("2.2", std::string());
  ASSERT_EQ("2.2", session.GetCurrentFrameId());
  session.SwitchToParentFrame();
  ASSERT_EQ("2.1", session.GetCurrentFrameId());
  session.SwitchToParentFrame();
  ASSERT_EQ(std::string(), session.GetCurrentFrameId());
}

TEST(Session, SwitchToTopFrame) {
  scoped_ptr<Chrome> chrome(new MockChrome());
  Session session("1", chrome.Pass());

  // Initial frame should be top frame.
  ASSERT_EQ(std::string(), session.GetCurrentFrameId());

  // Switching to top frame should be a no-op.
  session.SwitchToTopFrame();
  ASSERT_EQ(std::string(), session.GetCurrentFrameId());

  session.SwitchToSubFrame("3.1", std::string());
  ASSERT_EQ("3.1", session.GetCurrentFrameId());
  session.SwitchToSubFrame("3.2", std::string());
  ASSERT_EQ("3.2", session.GetCurrentFrameId());
  session.SwitchToTopFrame();
  ASSERT_EQ(std::string(), session.GetCurrentFrameId());
}
