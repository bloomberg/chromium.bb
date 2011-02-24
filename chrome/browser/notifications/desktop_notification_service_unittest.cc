// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/desktop_notification_service.h"

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/notifications/notifications_prefs_cache.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_pref_update.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPresenter.h"

namespace {

// NotificationsPrefsCache wants to be called on the IO thread. This class
// routes calls to the cache on the IO thread.
class ThreadProxy : public base::RefCountedThreadSafe<ThreadProxy> {
 public:
  ThreadProxy()
      : io_event_(false, false),
        ui_event_(false, false),
        permission_(0) {
    // The current message loop was already initalized by the test superclass.
    ui_thread_.reset(
        new BrowserThread(BrowserThread::UI, MessageLoop::current()));

    // Create IO thread, start its message loop.
    io_thread_.reset(new BrowserThread(BrowserThread::IO));
    io_thread_->Start();

    // Calling PauseIOThread() here isn't safe, because the runnable method
    // could complete before the constructor is done, deleting |this|.
  }

  int CacheHasPermission(NotificationsPrefsCache* cache, const GURL& url) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this, &ThreadProxy::CacheHasPermissionIO,
                          make_scoped_refptr(cache), url));
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

  void CacheHasPermissionIO(NotificationsPrefsCache* cache, const GURL& url) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    permission_ = cache->HasPermission(url);
    ui_event_.Signal();
  }

  base::WaitableEvent io_event_;
  base::WaitableEvent ui_event_;
  scoped_ptr<BrowserThread> ui_thread_;
  scoped_ptr<BrowserThread> io_thread_;

  int permission_;
};


class DesktopNotificationServiceTest : public RenderViewHostTestHarness {
 public:
  DesktopNotificationServiceTest() {
  }

  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    proxy_ = new ThreadProxy;
    proxy_->PauseIOThread();

    // Creates the service, calls InitPrefs() on it which loads data from the
    // profile into the cache and then puts the cache in io thread mode.
    service_ = profile()->GetDesktopNotificationService();
    cache_ = service_->prefs_cache();
  }

  virtual void TearDown() {
    // The io thread's waiting on the io_event_ might hold a ref to |proxy_|,
    // preventing its destruction. Clear that ref.
    proxy_->DrainIOThread();
    RenderViewHostTestHarness::TearDown();
  }

  DesktopNotificationService* service_;
  NotificationsPrefsCache* cache_;
  scoped_refptr<ThreadProxy> proxy_;
};

TEST_F(DesktopNotificationServiceTest, DefaultContentSettingSentToCache) {
  // The default pref registered in DesktopNotificationService is "ask",
  // and that's what sent to the cache.
  EXPECT_EQ(CONTENT_SETTING_ASK, cache_->CachedDefaultContentSetting());

  // Change the default content setting. This will post a task on the IO thread
  // to update the cache.
  service_->SetDefaultContentSetting(CONTENT_SETTING_BLOCK);

  // The updated pref shouldn't be sent to the cache immediately.
  EXPECT_EQ(CONTENT_SETTING_ASK, cache_->CachedDefaultContentSetting());

  // Run IO thread tasks.
  proxy_->DrainIOThread();

  // Now that IO thread events have been processed, it should be there.
  EXPECT_EQ(CONTENT_SETTING_BLOCK, cache_->CachedDefaultContentSetting());
}

TEST_F(DesktopNotificationServiceTest, GrantPermissionSentToCache) {
  GURL url("http://allowed.com");
  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionNotAllowed,
            proxy_->CacheHasPermission(cache_, url));

  service_->GrantPermission(url);

  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionAllowed,
            proxy_->CacheHasPermission(cache_, url));
}

TEST_F(DesktopNotificationServiceTest, DenyPermissionSentToCache) {
  GURL url("http://denied.com");
  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionNotAllowed,
            proxy_->CacheHasPermission(cache_, url));

  service_->DenyPermission(url);

  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionDenied,
            proxy_->CacheHasPermission(cache_, url));
}

TEST_F(DesktopNotificationServiceTest, PrefChangesSentToCache) {
  PrefService* prefs = profile()->GetPrefs();

  ListValue* allowed_sites =
      prefs->GetMutableList(prefs::kDesktopNotificationAllowedOrigins);
  {
    allowed_sites->Append(new StringValue(GURL("http://allowed.com").spec()));
    ScopedPrefUpdate updateAllowed(
        prefs, prefs::kDesktopNotificationAllowedOrigins);
  }

  ListValue* denied_sites =
      prefs->GetMutableList(prefs::kDesktopNotificationDeniedOrigins);
  {
    denied_sites->Append(new StringValue(GURL("http://denied.com").spec()));
    ScopedPrefUpdate updateDenied(
        prefs, prefs::kDesktopNotificationDeniedOrigins);
  }

  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionAllowed,
            proxy_->CacheHasPermission(cache_, GURL("http://allowed.com")));
  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionDenied,
            proxy_->CacheHasPermission(cache_, GURL("http://denied.com")));
}

TEST_F(DesktopNotificationServiceTest, GetAllowedOrigins) {
  service_->GrantPermission(GURL("http://allowed2.com"));
  service_->GrantPermission(GURL("http://allowed.com"));

  std::vector<GURL> allowed_origins(service_->GetAllowedOrigins());
  ASSERT_EQ(2u, allowed_origins.size());
  EXPECT_EQ(GURL("http://allowed2.com"), allowed_origins[0]);
  EXPECT_EQ(GURL("http://allowed.com"), allowed_origins[1]);
}

TEST_F(DesktopNotificationServiceTest, GetBlockedOrigins) {
  service_->DenyPermission(GURL("http://denied2.com"));
  service_->DenyPermission(GURL("http://denied.com"));

  std::vector<GURL> denied_origins(service_->GetBlockedOrigins());
  ASSERT_EQ(2u, denied_origins.size());
  EXPECT_EQ(GURL("http://denied2.com"), denied_origins[0]);
  EXPECT_EQ(GURL("http://denied.com"), denied_origins[1]);
}

TEST_F(DesktopNotificationServiceTest, ResetAllSentToCache) {
  GURL allowed_url("http://allowed.com");
  service_->GrantPermission(allowed_url);
  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionAllowed,
            proxy_->CacheHasPermission(cache_, allowed_url));
  GURL denied_url("http://denied.com");
  service_->DenyPermission(denied_url);
  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionDenied,
            proxy_->CacheHasPermission(cache_, denied_url));

  service_->ResetAllOrigins();

  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionNotAllowed,
            proxy_->CacheHasPermission(cache_, allowed_url));
  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionNotAllowed,
            proxy_->CacheHasPermission(cache_, denied_url));
}

TEST_F(DesktopNotificationServiceTest, ResetAllowedSentToCache) {
  GURL allowed_url("http://allowed.com");
  service_->GrantPermission(allowed_url);
  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionAllowed,
            proxy_->CacheHasPermission(cache_, allowed_url));

  service_->ResetAllowedOrigin(allowed_url);

  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionNotAllowed,
            proxy_->CacheHasPermission(cache_, allowed_url));
}

TEST_F(DesktopNotificationServiceTest, ResetBlockedSentToCache) {
  GURL denied_url("http://denied.com");
  service_->DenyPermission(denied_url);
  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionDenied,
            proxy_->CacheHasPermission(cache_, denied_url));

  service_->ResetBlockedOrigin(denied_url);

  EXPECT_EQ(WebKit::WebNotificationPresenter::PermissionNotAllowed,
            proxy_->CacheHasPermission(cache_, denied_url));
}

}  // namespace
