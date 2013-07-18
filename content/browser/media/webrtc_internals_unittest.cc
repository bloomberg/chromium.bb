// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "content/browser/media/webrtc_internals.h"
#include "content/browser/media/webrtc_internals_ui_observer.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

static const std::string kContraints = "c";
static const std::string kServers = "s";
static const std::string kUrl = "u";

class MockWebRTCInternalsProxy : public WebRTCInternalsUIObserver {
 public:
  virtual void OnUpdate(const std::string& command,
                        const base::Value* value) OVERRIDE {
    command_ = command;
    value_.reset(value->DeepCopy());
  }

  std::string command() {
    return command_;
  }

  base::Value* value() {
    return value_.get();
  }

 private:
   std::string command_;
   scoped_ptr<base::Value> value_;
};

class WebRTCInternalsTest : public testing::Test {
 public:
  WebRTCInternalsTest() : io_thread_(BrowserThread::UI, &io_loop_) {}

 protected:
  std::string ExpectedInfo(std::string prefix,
                           std::string id,
                           std::string suffix) {
    static const std::string  kstatic_part1 = std::string(
        "{\"constraints\":\"c\",");
    static const std::string kstatic_part2 = std::string(
        ",\"servers\":\"s\",\"url\":\"u\"}");
    return prefix + kstatic_part1 + id + kstatic_part2 + suffix;
  }

  base::MessageLoop io_loop_;
  TestBrowserThread io_thread_;
};

}  // namespace

TEST_F(WebRTCInternalsTest, AddRemoveObserver) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  WebRTCInternals::GetInstance()->AddObserver(observer.get());
  WebRTCInternals::GetInstance()->RemoveObserver(observer.get());
  WebRTCInternals::GetInstance()->OnAddPeerConnection(
      0, 3, 4, kUrl, kServers, kContraints);
  EXPECT_EQ("", observer->command());

  WebRTCInternals::GetInstance()->OnRemovePeerConnection(3, 4);
}

TEST_F(WebRTCInternalsTest, SendAddPeerConnectionUpdate) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  WebRTCInternals::GetInstance()->AddObserver(observer.get());
  WebRTCInternals::GetInstance()->OnAddPeerConnection(
      0, 1, 2, kUrl, kServers, kContraints);
  EXPECT_EQ("addPeerConnection", observer->command());

  base::DictionaryValue* dict = NULL;
  EXPECT_TRUE(observer->value()->GetAsDictionary(&dict));

  int int_value;
  EXPECT_TRUE(dict->GetInteger("pid", &int_value));
  EXPECT_EQ(1, int_value);
  EXPECT_TRUE(dict->GetInteger("lid", &int_value));
  EXPECT_EQ(2, int_value);

  std::string value;
  EXPECT_TRUE(dict->GetString("url", &value));
  EXPECT_EQ(kUrl, value);
  EXPECT_TRUE(dict->GetString("servers", &value));
  EXPECT_EQ(kServers, value);
  EXPECT_TRUE(dict->GetString("constraints", &value));
  EXPECT_EQ(kContraints, value);

  WebRTCInternals::GetInstance()->RemoveObserver(observer.get());
  WebRTCInternals::GetInstance()->OnRemovePeerConnection(1, 2);
}

TEST_F(WebRTCInternalsTest, SendRemovePeerConnectionUpdate) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  WebRTCInternals::GetInstance()->AddObserver(observer.get());
  WebRTCInternals::GetInstance()->OnAddPeerConnection(
      0, 1, 2, kUrl, kServers, kContraints);
  WebRTCInternals::GetInstance()->OnRemovePeerConnection(1, 2);
  EXPECT_EQ("removePeerConnection", observer->command());

  base::DictionaryValue* dict = NULL;
  EXPECT_TRUE(observer->value()->GetAsDictionary(&dict));

  int int_value;
  EXPECT_TRUE(dict->GetInteger("pid", &int_value));
  EXPECT_EQ(1, int_value);
  EXPECT_TRUE(dict->GetInteger("lid", &int_value));
  EXPECT_EQ(2, int_value);

  WebRTCInternals::GetInstance()->RemoveObserver(observer.get());
}

TEST_F(WebRTCInternalsTest, SendUpdatePeerConnectionUpdate) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  WebRTCInternals::GetInstance()->AddObserver(observer.get());
  WebRTCInternals::GetInstance()->OnAddPeerConnection(
      0, 1, 2, kUrl, kServers, kContraints);

  const std::string update_type = "fakeType";
  const std::string update_value = "fakeValue";
  WebRTCInternals::GetInstance()->OnUpdatePeerConnection(
      1, 2, update_type, update_value);

  EXPECT_EQ("updatePeerConnection", observer->command());

  base::DictionaryValue* dict = NULL;
  EXPECT_TRUE(observer->value()->GetAsDictionary(&dict));

  int int_value;
  EXPECT_TRUE(dict->GetInteger("pid", &int_value));
  EXPECT_EQ(1, int_value);
  EXPECT_TRUE(dict->GetInteger("lid", &int_value));
  EXPECT_EQ(2, int_value);

  std::string value;
  EXPECT_TRUE(dict->GetString("type", &value));
  EXPECT_EQ(update_type, value);
  EXPECT_TRUE(dict->GetString("value", &value));
  EXPECT_EQ(update_value, value);

  WebRTCInternals::GetInstance()->OnRemovePeerConnection(1, 2);
  WebRTCInternals::GetInstance()->RemoveObserver(observer.get());
}

}  // namespace content
