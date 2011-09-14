// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/desktop_notification_service.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPresenter.h"

namespace {

// The HasPermission method of the DesktopNotificationService wants to be called
// on the IO thread. This class routes calls to the cache on the IO thread.
class ThreadProxy : public base::RefCountedThreadSafe<ThreadProxy> {
 public:
  ThreadProxy()
      : io_event_(false, false),
        ui_event_(false, false),
        permission_(WebKit::WebNotificationPresenter::PermissionAllowed) {
    // The current message loop was already initalized by the test superclass.
    ui_thread_.reset(
        new BrowserThread(BrowserThread::UI, MessageLoop::current()));

    // Create IO thread, start its message loop.
    io_thread_.reset(new BrowserThread(BrowserThread::IO));
    io_thread_->Start();

    // Calling PauseIOThread() here isn't safe, because the runnable method
    // could complete before the constructor is done, deleting |this|.
  }

  // Call the HasPermission method of the DesktopNotificationService on the IO
  // thread and returns the permission setting.
  WebKit::WebNotificationPresenter::Permission ServiceHasPermission(
      DesktopNotificationService* service,
      const GURL& url) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this, &ThreadProxy::ServiceHasPermissionIO,
                          service, url));
    io_event_.Signal();
    ui_event_.Wait();  // Wait for IO thread to be done.
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this, &ThreadProxy::PauseIOThreadIO));

    return permission_;
  }

  void PauseIOThread() {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this, &ThreadProxy::PauseIOThreadIO));
  }

  void DrainIOThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    io_event_.Signal();
    io_thread_->Stop();
  }

 private:
  friend class base::RefCountedThreadSafe<ThreadProxy>;
  ~ThreadProxy() {
     DrainIOThread();
  }

  void PauseIOThreadIO() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    io_event_.Wait();
  }

  void ServiceHasPermissionIO(DesktopNotificationService* service,
                              const GURL& url) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    permission_ = service->HasPermission(url);
    ui_event_.Signal();
  }

  base::WaitableEvent io_event_;
  base::WaitableEvent ui_event_;
  scoped_ptr<BrowserThread> ui_thread_;
  scoped_ptr<BrowserThread> io_thread_;

  WebKit::WebNotificationPresenter::Permission permission_;
};

}  // namespace

class DesktopNotificationServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  DesktopNotificationServiceTest() {
  }

  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();
    proxy_ = new ThreadProxy;
    proxy_->PauseIOThread();

    // Creates the destop notification service.
    service_ = DesktopNotificationServiceFactory::GetForProfile(profile());
  }

  virtual void TearDown() {
    // The io thread's waiting on the io_event_ might hold a ref to |proxy_|,
    // preventing its destruction. Clear that ref.
    proxy_->DrainIOThread();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  DesktopNotificationService* service_;
  scoped_refptr<ThreadProxy> proxy_;
};

TEST_F(DesktopNotificationServiceTest, SettingsForSchemes) {
  GURL url("file:///html/test.html");

  EXPECT_EQ(CONTENT_SETTING_ASK,
            service_->GetDefaultContentSetting());
  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionNotAllowed,
            proxy_->ServiceHasPermission(service_, url));

  service_->GrantPermission(url);
  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionAllowed,
            proxy_->ServiceHasPermission(service_, url));

  service_->DenyPermission(url);
  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionDenied,
            proxy_->ServiceHasPermission(service_, url));

  GURL https_url("https://testurl");
  GURL http_url("http://testurl");
  EXPECT_EQ(CONTENT_SETTING_ASK,
            service_->GetDefaultContentSetting());
  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionNotAllowed,
            proxy_->ServiceHasPermission(service_, http_url));
  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionNotAllowed,
            proxy_->ServiceHasPermission(service_, https_url));

  service_->GrantPermission(https_url);
  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionNotAllowed,
            proxy_->ServiceHasPermission(service_, http_url));
  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionAllowed,
            proxy_->ServiceHasPermission(service_, https_url));

  service_->DenyPermission(http_url);
  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionDenied,
            proxy_->ServiceHasPermission(service_, http_url));
  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionAllowed,
            proxy_->ServiceHasPermission(service_, https_url));
}

TEST_F(DesktopNotificationServiceTest, GetNotificationsSettings) {
  service_->GrantPermission(GURL("http://allowed2.com"));
  service_->GrantPermission(GURL("http://allowed.com"));
  service_->DenyPermission(GURL("http://denied2.com"));
  service_->DenyPermission(GURL("http://denied.com"));

  HostContentSettingsMap::SettingsForOneType settings;
  service_->GetNotificationsSettings(&settings);
  ASSERT_EQ(4u, settings.size());

  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(
                GURL("http://allowed.com")),
            settings[0].a);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            settings[0].c);
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(
                GURL("http://allowed2.com")),
            settings[1].a);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            settings[1].c);
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(
                GURL("http://denied.com")),
            settings[2].a);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            settings[2].c);
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(
                GURL("http://denied2.com")),
            settings[3].a);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            settings[3].c);
}
