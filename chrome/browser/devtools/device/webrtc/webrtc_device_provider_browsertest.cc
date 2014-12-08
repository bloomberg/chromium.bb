// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/devtools/device/webrtc/webrtc_device_provider.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"

using content::BrowserThread;
using content::MessageLoopRunner;

class WebRTCDeviceProviderTest : public InProcessBrowserTest {
 protected:
  scoped_refptr<WebRTCDeviceProvider> CreateProvider();
  static void Unreference(
      scoped_refptr<WebRTCDeviceProvider> provider);

  scoped_refptr<WebRTCDeviceProvider> provider_;
};

scoped_refptr<WebRTCDeviceProvider>
WebRTCDeviceProviderTest::CreateProvider() {
  return new WebRTCDeviceProvider(
      browser()->profile(),
      SigninManagerFactory::GetForProfile(browser()->profile()),
      ProfileOAuth2TokenServiceFactory::GetForProfile(browser()->profile()));
}

// static
void WebRTCDeviceProviderTest::Unreference(
    scoped_refptr<WebRTCDeviceProvider> provider) {
}

IN_PROC_BROWSER_TEST_F(WebRTCDeviceProviderTest, TestDeleteSelf) {
  scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&Unreference, CreateProvider()),
      runner->QuitClosure());
  runner->Run();
}

IN_PROC_BROWSER_TEST_F(WebRTCDeviceProviderTest, OutliveProfile) {
  provider_ = CreateProvider();
}
