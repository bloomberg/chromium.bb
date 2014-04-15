// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/android_device.h"
#include "chrome/browser/devtools/devtools_adb_bridge.h"
#include "chrome/browser/devtools/devtools_target_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"

const char kDeviceModelCommand[] = "shell:getprop ro.product.model";
const char kOpenedUnixSocketsCommand[] = "shell:cat /proc/net/unix";
const char kListProcessesCommand[] = "shell:ps";
const char kListPackagesCommand[] = "shell:pm list packages";
const char kDumpsysCommand[] = "shell:dumpsys window policy";

const char kPageListRequest[] = "GET /json HTTP/1.1\r\n\r\n";
const char kVersionRequest[] = "GET /json/version HTTP/1.1\r\n\r\n";

const char kSampleOpenedUnixSockets[] =
    "Num       RefCount Protocol Flags    Type St Inode Path\n"
    "00000000: 00000004 00000000"
    " 00000000 0002 01  3328 /dev/socket/wpa_wlan0\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01  5394 /dev/socket/vold\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01 11810 @webview_devtools_remote_2425\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01 20893 @chrome_devtools_remote\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01 20894 @chrome_devtools_remote_1002\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01 20895 @noprocess_devtools_remote\n";

const char kSampleListProcesses[] =
    "USER   PID  PPID VSIZE  RSS    WCHAN    PC         NAME\n"
    "root   1    0    688    508    ffffffff 00000000 S /init\r\n"
    "u0_a75 2425 123  933736 193024 ffffffff 00000000 S com.sample.feed\r\n"
    "nfc    741  123  706448 26316  ffffffff 00000000 S com.android.nfc\r\n"
    "u0_a76 1001 124  111111 222222 ffffffff 00000000 S com.android.chrome\r\n"
    "u0_a77 1002 125  111111 222222 ffffffff 00000000 S com.chrome.beta\r\n"
    "u0_a78 1003 126  111111 222222 ffffffff 00000000 S com.noprocess.app\r\n";

const char kSampleListPackages[] =
    "package:com.sample.feed\r\n"
    "package:com.android.nfc\r\n"
    "package:com.android.chrome\r\n"
    "package:com.chrome.beta\r\n"
    "package:com.google.android.apps.chrome\r\n";

const char kSampleDumpsysCommand[] =
    "WINDOW MANAGER POLICY STATE (dumpsys window policy)\r\n"
    "    mSafeMode=false mSystemReady=true mSystemBooted=true\r\n"
    "    mStable=(0,50)-(720,1184)\r\n" // Only mStable parameter is parsed
    "    mForceStatusBar=false mForceStatusBarFromKeyguard=false\r\n";

char kSampleChromeVersion[] = "{\n"
    "   \"Browser\": \"Chrome/32.0.1679.0\",\n"
    "   \"Protocol-Version\": \"1.0\",\n"
    "   \"User-Agent\": \"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/32.0.1679.0 Safari/537.36\",\n"
    "   \"WebKit-Version\": \"537.36 (@160162)\"\n"
    "}";

char kSampleChromeBetaVersion[] = "{\n"
    "   \"Browser\": \"Chrome/31.0.1599.0\",\n"
    "   \"Protocol-Version\": \"1.0\",\n"
    "   \"User-Agent\": \"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/32.0.1679.0 Safari/537.36\",\n"
    "   \"WebKit-Version\": \"537.36 (@160162)\"\n"
    "}";

char kSampleWebViewVersion[] = "{\n"
    "   \"Browser\": \"Version/4.0\",\n"
    "   \"Protocol-Version\": \"1.0\",\n"
    "   \"User-Agent\": \"Mozilla/5.0 (Linux; Android 4.3; Build/KRS74B) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Safari/537.36\",\n"
    "   \"WebKit-Version\": \"537.36 (@157588)\"\n"
    "}";

char kSampleChromePages[] = "[ {\n"
    "   \"description\": \"\",\n"
    "   \"devtoolsFrontendUrl\": \"/devtools/devtools.html?"
    "ws=/devtools/page/755DE5C9-D49F-811D-0693-51B8E15C80D2\",\n"
    "   \"id\": \"755DE5C9-D49F-811D-0693-51B8E15C80D2\",\n"
    "   \"title\": \"The Chromium Projects\",\n"
    "   \"type\": \"page\",\n"
    "   \"url\": \"http://www.chromium.org/\",\n"
    "   \"webSocketDebuggerUrl\": \""
    "ws:///devtools/page/755DE5C9-D49F-811D-0693-51B8E15C80D2\"\n"
    "} ]";

char kSampleChromeBetaPages[] = "[]";

char kSampleWebViewPages[] = "[ {\n"
    "   \"description\": \"{\\\"attached\\\":false,\\\"empty\\\":false,"
    "\\\"height\\\":1173,\\\"screenX\\\":0,\\\"screenY\\\":0,"
    "\\\"visible\\\":true,\\\"width\\\":800}\",\n"
    "   \"devtoolsFrontendUrl\": \"http://chrome-devtools-frontend.appspot.com/"
    "serve_rev/@157588/devtools.html?ws="
    "/devtools/page/3E962D4D-B676-182D-3BE8-FAE7CE224DE7\",\n"
    "   \"faviconUrl\": \"http://chromium.org/favicon.ico\",\n"
    "   \"id\": \"3E962D4D-B676-182D-3BE8-FAE7CE224DE7\",\n"
    "   \"thumbnailUrl\": \"/thumb/3E962D4D-B676-182D-3BE8-FAE7CE224DE7\",\n"
    "   \"title\": \"Blink - The Chromium Projects\",\n"
    "   \"type\": \"page\",\n"
    "   \"url\": \"http://www.chromium.org/blink\",\n"
    "   \"webSocketDebuggerUrl\": \"ws:///devtools/"
    "page/3E962D4D-B676-182D-3BE8-FAE7CE224DE7\"\n"
    "}, {\n"
    "   \"description\": \"{\\\"attached\\\":true,\\\"empty\\\":true,"
    "\\\"screenX\\\":0,\\\"screenY\\\":33,\\\"visible\\\":false}\",\n"
    "   \"devtoolsFrontendUrl\": \"http://chrome-devtools-frontend.appspot.com/"
    "serve_rev/@157588/devtools.html?ws="
    "/devtools/page/44681551-ADFD-2411-076B-3AB14C1C60E2\",\n"
    "   \"faviconUrl\": \"\",\n"
    "   \"id\": \"44681551-ADFD-2411-076B-3AB14C1C60E2\",\n"
    "   \"thumbnailUrl\": \"/thumb/44681551-ADFD-2411-076B-3AB14C1C60E2\",\n"
    "   \"title\": \"More Activity\",\n"
    "   \"type\": \"page\",\n"
    "   \"url\": \"about:blank\",\n"
    "   \"webSocketDebuggerUrl\": \"ws:///devtools/page/"
    "44681551-ADFD-2411-076B-3AB14C1C60E2\"\n"
    "}]";

class MockDeviceImpl : public AndroidDeviceManager::Device {
 public:
  MockDeviceImpl(const std::string& serial, int index,
                 bool connected, const char* device_model)
      : Device(serial, connected),
        device_model_(device_model)
  {}

  virtual void RunCommand(const std::string& command,
                            const CommandCallback& callback) OVERRIDE {
    const char* response;

    if (command == kDeviceModelCommand) {
      response = device_model_;
    } else if (command == kOpenedUnixSocketsCommand) {
      response = kSampleOpenedUnixSockets;
    } else if (command == kListProcessesCommand) {
      response = kSampleListProcesses;
    } else if (command == kListPackagesCommand) {
      response = kSampleListPackages;
    } else if (command == kDumpsysCommand) {
      response = kSampleDumpsysCommand;
    } else {
      NOTREACHED();
      return;
    }

    base::MessageLoop::current()->PostTask( FROM_HERE,
              base::Bind(&MockDeviceImpl::RunCommandCallback,
                         this, callback, 0, response));
  }

  void RunCommandCallback(const CommandCallback& callback, int result,
                          const std::string& response) {
    callback.Run(result, response);
  }

  virtual void OpenSocket(const std::string& name,
                          const SocketCallback& callback) OVERRIDE {
    NOTREACHED();
  }

  virtual void HttpQuery(const std::string& la_name,
                     const std::string& request,
                     const CommandCallback& callback) OVERRIDE {
    const char* response;

    if (la_name == "chrome_devtools_remote") {
      if (request == kVersionRequest) {
        response = kSampleChromeVersion;
      } else if (request == kPageListRequest) {
        response = kSampleChromePages;
      } else {
        NOTREACHED();
        return;
      }
    } else if (la_name == "chrome_devtools_remote_1002") {
      if (request == kVersionRequest) {
        response = kSampleChromeBetaVersion;
      } else if (request == kPageListRequest) {
        response = kSampleChromeBetaPages;
      } else {
        NOTREACHED();
        return;
      }
    } else if (la_name.find("noprocess_devtools_remote") == 0) {
      if (request == kVersionRequest) {
        response = "{}";
      } else if (request == kPageListRequest) {
        response = "[]";
      } else {
        NOTREACHED();
        return;
      }
    } else if (la_name == "webview_devtools_remote_2425") {
      if (request == kVersionRequest) {
        response = kSampleWebViewVersion;
      } else if (request == kPageListRequest) {
        response = kSampleWebViewPages;
      } else {
        NOTREACHED();
        return;
      }
    } else {
      NOTREACHED();
      return;
    }

    base::MessageLoop::current()->PostTask( FROM_HERE,
              base::Bind(&MockDeviceImpl::RunCommandCallback,
                         this, callback, 0, response));
  }

  virtual void HttpUpgrade(const std::string& la_name,
                       const std::string& request,
                       const SocketCallback& callback) {
    NOTREACHED();
  }

  virtual void HttpQueryCallback(const CommandCallback& next, int code,
                                 const std::string& result) {
    NOTREACHED();
  }

 private:
  virtual ~MockDeviceImpl()
  {}

  const char* device_model_;
};

class MockDeviceProvider : public AndroidDeviceManager::DeviceProvider {
  virtual ~MockDeviceProvider()
  {}

  virtual void QueryDevices(const QueryDevicesCallback& callback) OVERRIDE {
    AndroidDeviceManager::Devices devices;
    devices.push_back(new MockDeviceImpl("FirstDevice", 0, true, "Nexus 6"));
    devices.push_back(new MockDeviceImpl("SecondDevice", 1, false, "Nexus 8"));
    callback.Run(devices);
  }
};

// static
scoped_refptr<AndroidDeviceManager::DeviceProvider>
AndroidDeviceManager::GetMockDeviceProviderForTest() {
  return new MockDeviceProvider();
}

static scoped_refptr<DevToolsAdbBridge::RemoteBrowser>
FindBrowserByDisplayName(DevToolsAdbBridge::RemoteBrowsers browsers,
                         const std::string& name) {
  for (DevToolsAdbBridge::RemoteBrowsers::iterator it = browsers.begin();
      it != browsers.end(); ++it)
    if ((*it)->display_name() == name)
      return *it;
  return NULL;
}

class DevToolsAdbBridgeTest : public InProcessBrowserTest,
                              public DevToolsAdbBridge::DeviceListListener {
  typedef DevToolsAdbBridge::RemoteDevices::const_iterator rdci;
  typedef DevToolsAdbBridge::RemoteBrowsers::const_iterator rbci;
public:
  virtual void DeviceListChanged(
      const DevToolsAdbBridge::RemoteDevices& devices) OVERRIDE {
    devices_ = devices;
    runner_->Quit();
  }

  void CheckDevices() {
    ASSERT_EQ(2U, devices_.size());

    scoped_refptr<DevToolsAdbBridge::RemoteDevice> connected =
        devices_[0]->is_connected() ? devices_[0] : devices_[1];

    scoped_refptr<DevToolsAdbBridge::RemoteDevice> not_connected =
        devices_[0]->is_connected() ? devices_[1] : devices_[0];

    ASSERT_TRUE(connected->is_connected());
    ASSERT_FALSE(not_connected->is_connected());

    ASSERT_EQ(720, connected->screen_size().width());
    ASSERT_EQ(1184, connected->screen_size().height());

    ASSERT_EQ("FirstDevice", connected->serial());
    ASSERT_EQ("Nexus 6", connected->model());

    ASSERT_EQ("SecondDevice", not_connected->serial());
    ASSERT_EQ("Offline", not_connected->model());

    const DevToolsAdbBridge::RemoteBrowsers& browsers = connected->browsers();
    ASSERT_EQ(4U, browsers.size());

    scoped_refptr<DevToolsAdbBridge::RemoteBrowser> chrome =
        FindBrowserByDisplayName(browsers, "Chrome");
    ASSERT_TRUE(chrome);

    scoped_refptr<DevToolsAdbBridge::RemoteBrowser> chrome_beta =
        FindBrowserByDisplayName(browsers, "Chrome Beta");
    ASSERT_TRUE(chrome_beta);

    scoped_refptr<DevToolsAdbBridge::RemoteBrowser> chromium =
        FindBrowserByDisplayName(browsers, "Chromium");
    ASSERT_FALSE(chromium);

    scoped_refptr<DevToolsAdbBridge::RemoteBrowser> webview =
        FindBrowserByDisplayName(browsers, "WebView in com.sample.feed");
    ASSERT_TRUE(webview);

    scoped_refptr<DevToolsAdbBridge::RemoteBrowser> noprocess =
        FindBrowserByDisplayName(browsers, "Noprocess");
    ASSERT_TRUE(noprocess);

    ASSERT_EQ("32.0.1679.0", chrome->version());
    ASSERT_EQ("31.0.1599.0", chrome_beta->version());
    ASSERT_EQ("4.0", webview->version());

    std::vector<DevToolsTargetImpl*> chrome_pages =
        chrome->CreatePageTargets();
    std::vector<DevToolsTargetImpl*> chrome_beta_pages =
        chrome_beta->CreatePageTargets();
    std::vector<DevToolsTargetImpl*> webview_pages =
        webview->CreatePageTargets();

    ASSERT_EQ(1U, chrome_pages.size());
    ASSERT_EQ(0U, chrome_beta_pages.size());
    ASSERT_EQ(2U, webview_pages.size());

    // Check that we have non-empty description for webview pages.
    ASSERT_EQ(0U, chrome_pages[0]->GetDescription().size());
    ASSERT_NE(0U, webview_pages[0]->GetDescription().size());
    ASSERT_NE(0U, webview_pages[1]->GetDescription().size());

    ASSERT_EQ(GURL("http://www.chromium.org/"), chrome_pages[0]->GetUrl());
    ASSERT_EQ("The Chromium Projects", chrome_pages[0]->GetTitle());

    STLDeleteElements(&chrome_pages);
    STLDeleteElements(&webview_pages);
  }

  void init() {
    runner_ = new content::MessageLoopRunner;
  }

protected:
  scoped_refptr<content::MessageLoopRunner> runner_;
  DevToolsAdbBridge::RemoteDevices devices_;
};

IN_PROC_BROWSER_TEST_F(DevToolsAdbBridgeTest, DiscoverAndroidBrowsers) {
  init();

  scoped_refptr<DevToolsAdbBridge> adb_bridge =
      DevToolsAdbBridge::Factory::GetForProfile(browser()->profile());

  AndroidDeviceManager::DeviceProviders providers;
  providers.push_back(AndroidDeviceManager::GetMockDeviceProviderForTest());

  adb_bridge->set_device_providers_for_test(providers);

  if (!adb_bridge) {
    FAIL() << "Failed to get DevToolsAdbBridge.";
  }

  adb_bridge->AddDeviceListListener(this);

  runner_->Run();

  CheckDevices();
}
