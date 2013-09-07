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
#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.cc"
#include "chrome/test/base/web_ui_browsertest.h"

namespace local_discovery {

namespace {

const char kSampleServiceName[] = "myService._privet._tcp.local";
const char kSampleDeviceID[] = "MyFakeID";
const char kSampleDeviceHost[] = "myservice.local";

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

class FakePrivetDeviceLister : public PrivetDeviceLister {
 public:
  explicit FakePrivetDeviceLister(const base::Closure& discover_devices_called)
      : discover_devices_called_(discover_devices_called) {
  }

  virtual ~FakePrivetDeviceLister() {
  }

  // PrivetDeviceLister implementation.
  virtual void Start() OVERRIDE {
  }

  virtual void DiscoverNewDevices(bool force_referesh) OVERRIDE {
    discover_devices_called_.Run();
  }

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }
  Delegate* delegate() { return delegate_; }

 private:
  Delegate* delegate_;
  base::Closure discover_devices_called_;

  DISALLOW_COPY_AND_ASSIGN(FakePrivetDeviceLister);
};

class FakeLocalDiscoveryUIFactory : public LocalDiscoveryUIHandler::Factory {
 public:
  explicit FakeLocalDiscoveryUIFactory(
      scoped_ptr<FakePrivetDeviceLister> privet_lister) {
    owned_privet_lister_ = privet_lister.Pass();
    privet_lister_ = owned_privet_lister_.get();
    LocalDiscoveryUIHandler::SetFactory(this);
  }

  virtual ~FakeLocalDiscoveryUIFactory() {
    LocalDiscoveryUIHandler::SetFactory(NULL);
  }

  // LocalDiscoveryUIHandler::Factory implementation.
  virtual LocalDiscoveryUIHandler* CreateLocalDiscoveryUIHandler() OVERRIDE {
    DCHECK(owned_privet_lister_);  // This factory is a one-use factory.
    scoped_ptr<LocalDiscoveryUIHandler> handler(
        new LocalDiscoveryUIHandler(
            owned_privet_lister_.PassAs<PrivetDeviceLister>()));
    privet_lister_->set_delegate(handler.get());
    return handler.release();
  }

  FakePrivetDeviceLister* privet_lister() { return privet_lister_; }

 private:
  // FakePrivetDeviceLister is owned either by the factory or, once it creates a
  // LocalDiscoveryUI, by the LocalDiscoveryUI.  |privet_lister_| points to the
  // FakePrivetDeviceLister whereas |owned_privet_lister_| manages the ownership
  // of the pointer when it is owned by the factory.
  scoped_ptr<FakePrivetDeviceLister> owned_privet_lister_;
  FakePrivetDeviceLister* privet_lister_;

  DISALLOW_COPY_AND_ASSIGN(FakeLocalDiscoveryUIFactory);
};

class LocalDiscoveryUITest : public WebUIBrowserTest {
 public:
  LocalDiscoveryUITest() {
  }
  virtual ~LocalDiscoveryUITest() {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    WebUIBrowserTest::SetUpOnMainThread();

    scoped_ptr<FakePrivetDeviceLister> fake_lister;
    fake_lister.reset(new FakePrivetDeviceLister(
        base::Bind(&TestMessageLoopCondition::Signal,
                   base::Unretained(&condition_devices_listed_))));

    ui_factory_.reset(new FakeLocalDiscoveryUIFactory(
        fake_lister.Pass()));

    AddLibrary(base::FilePath(FILE_PATH_LITERAL("local_discovery_ui_test.js")));
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    WebUIBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableDeviceDiscovery);
  }

  FakeLocalDiscoveryUIFactory* ui_factory() { return ui_factory_.get(); }
  TestMessageLoopCondition& condition_devices_listed() {
    return condition_devices_listed_;
  }

 private:
  scoped_ptr<FakeLocalDiscoveryUIFactory> ui_factory_;
  TestMessageLoopCondition condition_devices_listed_;

  DISALLOW_COPY_AND_ASSIGN(LocalDiscoveryUITest);
};

IN_PROC_BROWSER_TEST_F(LocalDiscoveryUITest, EmptyTest) {
  ui_test_utils::NavigateToURL(browser(), GURL(
      chrome::kChromeUIDevicesFrameURL));
  condition_devices_listed().Wait();
  EXPECT_TRUE(WebUIBrowserTest::RunJavascriptTest("checkNoDevices"));
}

IN_PROC_BROWSER_TEST_F(LocalDiscoveryUITest, AddRowTest) {
  ui_test_utils::NavigateToURL(browser(), GURL(
      chrome::kChromeUIDevicesFrameURL));
  condition_devices_listed().Wait();
  DeviceDescription description;

  description.name = "Sample device";
  description.description = "Sample device description";

  ui_factory()->privet_lister()->delegate()->DeviceChanged(
      true, kSampleServiceName, description);

  EXPECT_TRUE(WebUIBrowserTest::RunJavascriptTest("checkOneDevice"));

  ui_factory()->privet_lister()->delegate()->DeviceRemoved(
      kSampleServiceName);

  EXPECT_TRUE(WebUIBrowserTest::RunJavascriptTest("checkNoDevices"));
}

}  // namespace

}  // namespace local_discovery
