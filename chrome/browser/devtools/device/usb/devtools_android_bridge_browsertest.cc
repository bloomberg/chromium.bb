// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/message_loop/message_loop.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/usb_service/usb_device.h"
#include "components/usb_service/usb_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using content::BrowserThread;
using usb_service::UsbService;
using usb_service::UsbDevice;

class DevToolsAndroidBridgeWarmUp
    : public DevToolsAndroidBridge::DeviceCountListener {
 public:
  DevToolsAndroidBridgeWarmUp(base::Closure closure,
                              scoped_refptr<DevToolsAndroidBridge> adb_bridge)
      : closure_(closure), adb_bridge_(adb_bridge) {}

  virtual void DeviceCountChanged(int count) OVERRIDE {
    adb_bridge_->RemoveDeviceCountListener(this);
    closure_.Run();
  }

  base::Closure closure_;
  scoped_refptr<DevToolsAndroidBridge> adb_bridge_;
};

class MockCountListener : public DevToolsAndroidBridge::DeviceCountListener {
 public:
  explicit MockCountListener(scoped_refptr<DevToolsAndroidBridge> adb_bridge)
      : adb_bridge_(adb_bridge),
        reposts_left_(10),
        invoked_(0) {
  }

  virtual void DeviceCountChanged(int count) OVERRIDE {
    ++invoked_;
    adb_bridge_->RemoveDeviceCountListener(this);
    Shutdown();
  }

  void Shutdown() {
    ShutdownOnUIThread();
  };

  void ShutdownOnUIThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (reposts_left_-- == 0) {
      base::MessageLoop::current()->Quit();
    } else {
      BrowserThread::PostTask(
          BrowserThread::FILE,
          FROM_HERE,
          base::Bind(&MockCountListener::ShutdownOnFileThread,
                     base::Unretained(this)));
    }
  }

  void ShutdownOnFileThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&MockCountListener::ShutdownOnUIThread,
                                       base::Unretained(this)));
  }

  scoped_refptr<DevToolsAndroidBridge> adb_bridge_;
  int reposts_left_;
  int invoked_;
};

class MockCountListenerWithReAdd : public MockCountListener {
 public:
  explicit MockCountListenerWithReAdd(
      scoped_refptr<DevToolsAndroidBridge> adb_bridge)
      : MockCountListener(adb_bridge),
        readd_count_(2) {
  }

  virtual void DeviceCountChanged(int count) OVERRIDE {
    ++invoked_;
    adb_bridge_->RemoveDeviceCountListener(this);
    if (readd_count_ > 0) {
      readd_count_--;
      adb_bridge_->AddDeviceCountListener(this);
      adb_bridge_->RemoveDeviceCountListener(this);
      adb_bridge_->AddDeviceCountListener(this);
    } else {
      Shutdown();
    }
  }

  int readd_count_;
};

class MockCountListenerWithReAddWhileQueued : public MockCountListener {
 public:
  MockCountListenerWithReAddWhileQueued(
      scoped_refptr<DevToolsAndroidBridge> adb_bridge)
      : MockCountListener(adb_bridge),
        readded_(false) {
  }

  virtual void DeviceCountChanged(int count) OVERRIDE {
    ++invoked_;
    if (!readded_) {
      readded_ = true;
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&MockCountListenerWithReAddWhileQueued::ReAdd,
                     base::Unretained(this)));
    } else {
      adb_bridge_->RemoveDeviceCountListener(this);
      Shutdown();
    }
  }

  void ReAdd() {
    adb_bridge_->RemoveDeviceCountListener(this);
    adb_bridge_->AddDeviceCountListener(this);
  }

  bool readded_;
};

class MockUsbService : public UsbService {
 public:
  MOCK_METHOD1(GetDeviceById, scoped_refptr<UsbDevice>(uint32 unique_id));
  virtual void GetDevices(
      std::vector<scoped_refptr<UsbDevice> >* devices) OVERRIDE {}
};

class DevtoolsAndroidBridgeBrowserTest : public InProcessBrowserTest {
 protected:
  DevtoolsAndroidBridgeBrowserTest()
      : scheduler_invoked_(0) {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;

    BrowserThread::PostTaskAndReply(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&DevtoolsAndroidBridgeBrowserTest::SetUpService, this),
        runner->QuitClosure());
    runner->Run();

    adb_bridge_ =
        DevToolsAndroidBridge::Factory::GetForProfile(browser()->profile());
    DCHECK(adb_bridge_);
    adb_bridge_->set_task_scheduler_for_test(base::Bind(
        &DevtoolsAndroidBridgeBrowserTest::ScheduleDeviceCountRequest, this));

    runner = new content::MessageLoopRunner;

    DevToolsAndroidBridgeWarmUp warmup(runner->QuitClosure(), adb_bridge_);
    adb_bridge_->AddDeviceCountListener(&warmup);
    runner->Run();

    runner_ = new content::MessageLoopRunner;
  }

  void ScheduleDeviceCountRequest(const base::Closure& request) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    scheduler_invoked_++;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, request);
  }

  void SetUpService() {
    service_ = new MockUsbService();
    UsbService::SetInstanceForTest(service_);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    UsbService* service = NULL;
    BrowserThread::PostTaskAndReply(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&UsbService::SetInstanceForTest, service),
        runner->QuitClosure());
    runner->Run();
  }

  MockUsbService* service_;
  int scheduler_invoked_;
  scoped_refptr<content::MessageLoopRunner> runner_;
  scoped_refptr<DevToolsAndroidBridge> adb_bridge_;
};

IN_PROC_BROWSER_TEST_F(DevtoolsAndroidBridgeBrowserTest,
                       TestNoMultipleCallsRemoveInCallback) {
  scoped_ptr<MockCountListener> listener(new MockCountListener(adb_bridge_));

  adb_bridge_->AddDeviceCountListener(listener.get());

  runner_->Run();
  EXPECT_EQ(1, listener->invoked_);
  EXPECT_EQ(listener->invoked_ - 1, scheduler_invoked_);
}

IN_PROC_BROWSER_TEST_F(DevtoolsAndroidBridgeBrowserTest,
                       TestNoMultipleCallsRemoveAddInCallback) {
  scoped_ptr<MockCountListener> listener(
      new MockCountListenerWithReAdd(adb_bridge_));

  adb_bridge_->AddDeviceCountListener(listener.get());

  runner_->Run();
  EXPECT_EQ(3, listener->invoked_);
  EXPECT_EQ(listener->invoked_ - 1, scheduler_invoked_);
}

IN_PROC_BROWSER_TEST_F(DevtoolsAndroidBridgeBrowserTest,
                       TestNoMultipleCallsRemoveAddOnStart) {
  scoped_ptr<MockCountListener> listener(new MockCountListener(adb_bridge_));

  adb_bridge_->AddDeviceCountListener(listener.get());
  adb_bridge_->RemoveDeviceCountListener(listener.get());
  adb_bridge_->AddDeviceCountListener(listener.get());

  runner_->Run();
  EXPECT_EQ(1, listener->invoked_);
  EXPECT_EQ(listener->invoked_ - 1, scheduler_invoked_);
}

IN_PROC_BROWSER_TEST_F(DevtoolsAndroidBridgeBrowserTest,
                       TestNoMultipleCallsRemoveAddWhileQueued) {
  scoped_ptr<MockCountListener> listener(
      new MockCountListenerWithReAddWhileQueued(adb_bridge_));

  adb_bridge_->AddDeviceCountListener(listener.get());

  runner_->Run();
  EXPECT_EQ(2, listener->invoked_);
  EXPECT_EQ(listener->invoked_ - 1, scheduler_invoked_);
}
