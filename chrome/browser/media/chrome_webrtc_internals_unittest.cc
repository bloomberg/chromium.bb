// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/media/chrome_webrtc_internals.h"
#include "chrome/browser/media/webrtc_internals_ui_observer.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Value;
using base::DictionaryValue;
using std::string;

static const string kContraints = "c";
static const string kServers = "s";
static const string kUrl = "u";

class MockWebRTCInternalsProxy : public WebRTCInternalsUIObserver {
 public:
  void OnUpdate(const string& command, const Value* value) OVERRIDE {
    command_ = command;
    value_.reset(value->DeepCopy());
  }

  string command() {
    return command_;
  }

  Value* value() {
    return value_.get();
  }

 private:
   string command_;
   scoped_ptr<Value> value_;
};

class WebRTCInternalsTest : public testing::Test {
 public:
  WebRTCInternalsTest()
      : io_thread_(content::BrowserThread::UI, &io_loop_) {}

 protected:
  virtual void SetUp() {
    webrtc_internals_ = ChromeWebRTCInternals::GetInstance();
  }

  string ExpectedInfo(string prefix,
                           string id,
                           string suffix) {
    static const string  kstatic_part1 = string(
        "{\"constraints\":\"c\",");
    static const string kstatic_part2 = string(
        ",\"servers\":\"s\",\"url\":\"u\"}");
    return prefix + kstatic_part1 + id + kstatic_part2 + suffix;
  }

  MessageLoop io_loop_;
  content::TestBrowserThread io_thread_;
  ChromeWebRTCInternals* webrtc_internals_;
};

TEST_F(WebRTCInternalsTest, GetInstance) {
  EXPECT_TRUE(webrtc_internals_);
}

TEST_F(WebRTCInternalsTest, AddRemoveObserver) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  webrtc_internals_->AddObserver(observer.get());
  webrtc_internals_->RemoveObserver(observer.get());
  webrtc_internals_->AddPeerConnection(3, 4, kUrl, kServers, kContraints);
  EXPECT_EQ("", observer->command());

  webrtc_internals_->RemovePeerConnection(3, 4);
}

TEST_F(WebRTCInternalsTest, SendAddPeerConnectionUpdate) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  webrtc_internals_->AddObserver(observer.get());
  webrtc_internals_->AddPeerConnection(1, 2, kUrl, kServers, kContraints);
  EXPECT_EQ("addPeerConnection", observer->command());

  DictionaryValue* dict = NULL;
  EXPECT_TRUE(observer->value()->GetAsDictionary(&dict));

  int int_value;
  EXPECT_TRUE(dict->GetInteger("pid", &int_value));
  EXPECT_EQ(1, int_value);
  EXPECT_TRUE(dict->GetInteger("lid", &int_value));
  EXPECT_EQ(2, int_value);

  string value;
  EXPECT_TRUE(dict->GetString("url", &value));
  EXPECT_EQ(kUrl, value);
  EXPECT_TRUE(dict->GetString("servers", &value));
  EXPECT_EQ(kServers, value);
  EXPECT_TRUE(dict->GetString("constraints", &value));
  EXPECT_EQ(kContraints, value);

  webrtc_internals_->RemoveObserver(observer.get());
  webrtc_internals_->RemovePeerConnection(1, 2);
}

TEST_F(WebRTCInternalsTest, SendRemovePeerConnectionUpdate) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  webrtc_internals_->AddObserver(observer.get());
  webrtc_internals_->AddPeerConnection(1, 2, kUrl, kServers, kContraints);
  webrtc_internals_->RemovePeerConnection(1, 2);
  EXPECT_EQ("removePeerConnection", observer->command());

  DictionaryValue* dict = NULL;
  EXPECT_TRUE(observer->value()->GetAsDictionary(&dict));

  int int_value;
  EXPECT_TRUE(dict->GetInteger("pid", &int_value));
  EXPECT_EQ(1, int_value);
  EXPECT_TRUE(dict->GetInteger("lid", &int_value));
  EXPECT_EQ(2, int_value);

  webrtc_internals_->RemoveObserver(observer.get());
}

TEST_F(WebRTCInternalsTest, SendUpdatePeerConnectionUpdate) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  webrtc_internals_->AddObserver(observer.get());
  webrtc_internals_->AddPeerConnection(1, 2, kUrl, kServers, kContraints);

  const string update_type = "fakeType";
  const string update_value = "fakeValue";
  webrtc_internals_->UpdatePeerConnection(1, 2, update_type, update_value);

  EXPECT_EQ("updatePeerConnection", observer->command());

  DictionaryValue* dict = NULL;
  EXPECT_TRUE(observer->value()->GetAsDictionary(&dict));

  int int_value;
  EXPECT_TRUE(dict->GetInteger("pid", &int_value));
  EXPECT_EQ(1, int_value);
  EXPECT_TRUE(dict->GetInteger("lid", &int_value));
  EXPECT_EQ(2, int_value);

  string value;
  EXPECT_TRUE(dict->GetString("type", &value));
  EXPECT_EQ(update_type, value);
  EXPECT_TRUE(dict->GetString("value", &value));
  EXPECT_EQ(update_value, value);

  webrtc_internals_->RemovePeerConnection(1, 2);
  webrtc_internals_->RemoveObserver(observer.get());
}
