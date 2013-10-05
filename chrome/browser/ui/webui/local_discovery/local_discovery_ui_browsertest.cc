// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/local_discovery/test_service_discovery_client.h"
#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.cc"
#include "chrome/test/base/web_ui_browsertest.h"

using testing::InvokeWithoutArgs;
using testing::Return;
using testing::AtLeast;

namespace local_discovery {

namespace {

const uint8 kQueryData[] = {
  // Header
  0x00, 0x00,
  0x00, 0x00,               // Flags not set.
  0x00, 0x01,               // Set QDCOUNT (question count) to 1, all the
  // rest are 0 for a query.
  0x00, 0x00,
  0x00, 0x00,
  0x00, 0x00,

  // Question
  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,

  0x00, 0x0c,               // QTYPE: A query.
  0x00, 0x01,               // QCLASS: IN class. Unicast bit not set.
};

const uint8 kAnnouncePacket[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x80, 0x00,               // Standard query response, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x03,               // 1 RR (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x0c,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x00,        // TTL (4 bytes) is 32768 second.
  0x10, 0x00,
  0x00, 0x0c,        // RDLENGTH is 12 bytes.
  0x09, 'm', 'y', 'S', 'e', 'r', 'v', 'i', 'c', 'e',
  0xc0, 0x0c,

  0x09, 'm', 'y', 'S', 'e', 'r', 'v', 'i', 'c', 'e',
  0xc0, 0x0c,
  0x00, 0x10,        // TYPE is TXT.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x00,        // TTL (4 bytes) is 32768 seconds.
  0x01, 0x00,
  0x00, 0x34,        // RDLENGTH is 69 bytes.
  0x03, 'i', 'd', '=',
  0x10, 't', 'y', '=', 'S', 'a', 'm', 'p', 'l', 'e', ' ',
        'd', 'e', 'v', 'i', 'c', 'e',
  0x1e, 'n', 'o', 't', 'e', '=',
        'S', 'a', 'm', 'p', 'l', 'e', ' ', 'd', 'e', 'v', 'i', 'c', 'e', ' ',
        'd', 'e', 's', 'c', 'r', 'i', 'p', 't', 'i', 'o', 'n',

  0x09, 'm', 'y', 'S', 'e', 'r', 'v', 'i', 'c', 'e',
  0xc0, 0x0c,
  0x00, 0x21,        // Type is SRV
  0x00, 0x01,        // CLASS is IN
  0x00, 0x00,        // TTL (4 bytes) is 32768 second.
  0x10, 0x00,
  0x00, 0x17,        // RDLENGTH is 23
  0x00, 0x00,
  0x00, 0x00,
  0x22, 0xb8,        // port 8888
  0x09, 'm', 'y', 'S', 'e', 'r', 'v', 'i', 'c', 'e',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00
};


const uint8 kGoodbyePacket[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x80, 0x00,               // Standard query response, RA, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x01,               // 1 RR (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x0c,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x00,        // TTL (4 bytes) is 0 seconds.
  0x00, 0x00,
  0x00, 0x0c,        // RDLENGTH is 12 bytes.
  0x09, 'm', 'y', 'S', 'e', 'r', 'v', 'i', 'c', 'e',
  0xc0, 0x0c,
};

const char kSampleServiceName[] = "myService._privet._tcp.local";

class TestMessageLoopCondition {
 public:
  TestMessageLoopCondition() : signaled_(false),
                               waiting_(false) {
  }

  ~TestMessageLoopCondition() {
  }

  // Signal a waiting method that it can continue executing.
  void Signal() {
    signaled_ = true;
    if (waiting_)
      base::MessageLoop::current()->Quit();
  }

  // Pause execution and recursively run the message loop until |Signal()| is
  // called. Do not pause if |Signal()| has already been called.
  void Wait() {
    if (!signaled_) {
      waiting_ = true;
      base::MessageLoop::current()->Run();
      waiting_ = false;
    }
  }

 private:
  bool signaled_;
  bool waiting_;

  DISALLOW_COPY_AND_ASSIGN(TestMessageLoopCondition);
};

class LocalDiscoveryUITest : public WebUIBrowserTest {
 public:
  LocalDiscoveryUITest() {
  }
  virtual ~LocalDiscoveryUITest() {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    WebUIBrowserTest::SetUpOnMainThread();

    test_service_discovery_client_ = new TestServiceDiscoveryClient();
    test_service_discovery_client_->Start();
    EXPECT_CALL(*test_service_discovery_client_, OnSendTo(
        std::string((const char*)kQueryData,
                    sizeof(kQueryData))))
        .Times(AtLeast(2))
        .WillOnce(InvokeWithoutArgs(&condition_devices_listed_,
                                    &TestMessageLoopCondition::Signal))
        .WillRepeatedly(Return());

    AddLibrary(base::FilePath(FILE_PATH_LITERAL("local_discovery_ui_test.js")));
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    WebUIBrowserTest::SetUpCommandLine(command_line);
  }

  void RunFor(base::TimeDelta time_period) {
    base::CancelableCallback<void()> callback(base::Bind(
        &base::MessageLoop::Quit, base::Unretained(
            base::MessageLoop::current())));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, callback.callback(), time_period);

    base::MessageLoop::current()->Run();
    callback.Cancel();
  }

  TestServiceDiscoveryClient* test_service_discovery_client() {
    return test_service_discovery_client_.get();
  }

  TestMessageLoopCondition& condition_devices_listed() {
    return condition_devices_listed_;
  }

 private:
  scoped_refptr<TestServiceDiscoveryClient> test_service_discovery_client_;
  TestMessageLoopCondition condition_devices_listed_;

  DISALLOW_COPY_AND_ASSIGN(LocalDiscoveryUITest);
};

IN_PROC_BROWSER_TEST_F(LocalDiscoveryUITest, EmptyTest) {
  ui_test_utils::NavigateToURL(browser(), GURL(
      chrome::kChromeUIDevicesURL));
  condition_devices_listed().Wait();
  EXPECT_TRUE(WebUIBrowserTest::RunJavascriptTest("checkNoDevices"));
}

IN_PROC_BROWSER_TEST_F(LocalDiscoveryUITest, AddRowTest) {
  ui_test_utils::NavigateToURL(browser(), GURL(
      chrome::kChromeUIDevicesURL));
  condition_devices_listed().Wait();

  test_service_discovery_client()->SimulateReceive(
      kAnnouncePacket, sizeof(kAnnouncePacket));

  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_TRUE(WebUIBrowserTest::RunJavascriptTest("checkOneDevice"));

  test_service_discovery_client()->SimulateReceive(
      kGoodbyePacket, sizeof(kGoodbyePacket));

  RunFor(base::TimeDelta::FromMilliseconds(1100));

  EXPECT_TRUE(WebUIBrowserTest::RunJavascriptTest("checkNoDevices"));
}

}  // namespace

}  // namespace local_discovery
