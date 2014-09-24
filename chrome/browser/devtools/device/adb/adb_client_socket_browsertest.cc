// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/adb/adb_device_provider.h"
#include "chrome/browser/devtools/device/adb/mock_adb_server.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/devtools/devtools_target_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"

using content::BrowserThread;

static scoped_refptr<DevToolsAndroidBridge::RemoteBrowser>
FindBrowserByDisplayName(DevToolsAndroidBridge::RemoteBrowsers browsers,
                         const std::string& name) {
  for (DevToolsAndroidBridge::RemoteBrowsers::iterator it = browsers.begin();
      it != browsers.end(); ++it)
    if ((*it)->display_name() == name)
      return *it;
  return NULL;
}

class AdbClientSocketTest : public InProcessBrowserTest,
                            public DevToolsAndroidBridge::DeviceListListener {

 public:
  void StartTest() {
    Profile* profile = browser()->profile();
    android_bridge_ = DevToolsAndroidBridge::Factory::GetForProfile(profile);
    AndroidDeviceManager::DeviceProviders device_providers;
    device_providers.push_back(new AdbDeviceProvider());
    android_bridge_->set_device_providers_for_test(device_providers);
    android_bridge_->AddDeviceListListener(this);
    content::RunMessageLoop();
  }

  virtual void DeviceListChanged(
      const DevToolsAndroidBridge::RemoteDevices& devices) OVERRIDE {
    devices_ = devices;
    android_bridge_->RemoveDeviceListListener(this);
    base::MessageLoop::current()->Quit();
  }

  void CheckDevices() {
    ASSERT_EQ(2U, devices_.size());

    scoped_refptr<DevToolsAndroidBridge::RemoteDevice> connected =
        devices_[0]->is_connected() ? devices_[0] : devices_[1];

    scoped_refptr<DevToolsAndroidBridge::RemoteDevice> not_connected =
        devices_[0]->is_connected() ? devices_[1] : devices_[0];

    ASSERT_TRUE(connected->is_connected());
    ASSERT_FALSE(not_connected->is_connected());

    ASSERT_EQ(720, connected->screen_size().width());
    ASSERT_EQ(1184, connected->screen_size().height());

    ASSERT_EQ("01498B321301A00A", connected->serial());
    ASSERT_EQ("Nexus 6", connected->model());

    ASSERT_EQ("01498B2B0D01300E", not_connected->serial());
    ASSERT_EQ("Offline", not_connected->model());

    const DevToolsAndroidBridge::RemoteBrowsers& browsers =
        connected->browsers();
    ASSERT_EQ(4U, browsers.size());

    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> chrome =
        FindBrowserByDisplayName(browsers, "Chrome");
    ASSERT_TRUE(chrome.get());

    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> chrome_beta =
        FindBrowserByDisplayName(browsers, "Chrome Beta");
    ASSERT_TRUE(chrome_beta.get());

    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> chromium =
        FindBrowserByDisplayName(browsers, "Chromium");
    ASSERT_FALSE(chromium.get());

    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> webview =
        FindBrowserByDisplayName(browsers, "WebView in com.sample.feed");
    ASSERT_TRUE(webview.get());

    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> noprocess =
        FindBrowserByDisplayName(browsers, "Noprocess");
    ASSERT_TRUE(noprocess.get());

    ASSERT_EQ("32.0.1679.0", chrome->version());
    ASSERT_EQ("31.0.1599.0", chrome_beta->version());
    ASSERT_EQ("4.0", webview->version());

    std::vector<DevToolsAndroidBridge::RemotePage*> chrome_pages =
        chrome->CreatePages();
    std::vector<DevToolsAndroidBridge::RemotePage*> chrome_beta_pages =
        chrome_beta->CreatePages();
    std::vector<DevToolsAndroidBridge::RemotePage*> webview_pages =
        webview->CreatePages();

    ASSERT_EQ(1U, chrome_pages.size());
    ASSERT_EQ(1U, chrome_beta_pages.size());
    ASSERT_EQ(2U, webview_pages.size());

    // Check that we have non-empty description for webview pages.
    ASSERT_EQ(0U, chrome_pages[0]->GetTarget()->GetDescription().size());
    ASSERT_EQ(0U, chrome_beta_pages[0]->GetTarget()->GetDescription().size());
    ASSERT_NE(0U, webview_pages[0]->GetTarget()->GetDescription().size());
    ASSERT_NE(0U, webview_pages[1]->GetTarget()->GetDescription().size());

    ASSERT_EQ(GURL("http://www.chromium.org/"),
                   chrome_pages[0]->GetTarget()->GetURL());
    ASSERT_EQ("The Chromium Projects",
              chrome_pages[0]->GetTarget()->GetTitle());

    STLDeleteElements(&chrome_pages);
    STLDeleteElements(&chrome_beta_pages);
    STLDeleteElements(&webview_pages);
  }

 private:
  scoped_refptr<DevToolsAndroidBridge> android_bridge_;
  DevToolsAndroidBridge::RemoteDevices devices_;
};

IN_PROC_BROWSER_TEST_F(AdbClientSocketTest, TestAdbClientSocket) {
  StartMockAdbServer();
  StartTest();
  CheckDevices();
  StopMockAdbServer();
}
