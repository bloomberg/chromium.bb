// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_url_request_context.h"

#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_cookie_policy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/webui/chrome_url_data_manager_backend.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "net/base/cookie_store.h"
#include "net/ftp/ftp_transaction_factory.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "webkit/glue/webkit_glue.h"

#if defined(USE_NSS)
#include "net/ocsp/nss_ocsp.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/libcros_service_library.h"
#include "chrome/browser/chromeos/proxy_config_service.h"
#endif  // defined(OS_CHROMEOS)

class ChromeURLRequestContextFactory {
 public:
  ChromeURLRequestContextFactory() {}
  virtual ~ChromeURLRequestContextFactory() {}

  // Called to create a new instance (will only be called once).
  virtual scoped_refptr<ChromeURLRequestContext> Create() = 0;

 protected:
  DISALLOW_COPY_AND_ASSIGN(ChromeURLRequestContextFactory);
};

namespace {

// ----------------------------------------------------------------------------
// Helper factories
// ----------------------------------------------------------------------------

// Factory that creates the main ChromeURLRequestContext.
class FactoryForMain : public ChromeURLRequestContextFactory {
 public:
  explicit FactoryForMain(const ProfileIOData* profile_io_data)
      : profile_io_data_(profile_io_data) {}

  virtual scoped_refptr<ChromeURLRequestContext> Create() {
    return profile_io_data_->GetMainRequestContext();
  }

 private:
  const scoped_refptr<const ProfileIOData> profile_io_data_;
};

// Factory that creates the ChromeURLRequestContext for extensions.
class FactoryForExtensions : public ChromeURLRequestContextFactory {
 public:
  explicit FactoryForExtensions(const ProfileIOData* profile_io_data)
      : profile_io_data_(profile_io_data) {}

  virtual scoped_refptr<ChromeURLRequestContext> Create() {
    return profile_io_data_->GetExtensionsRequestContext();
  }

 private:
  const scoped_refptr<const ProfileIOData> profile_io_data_;
};

// Factory that creates the ChromeURLRequestContext for media.
class FactoryForMedia : public ChromeURLRequestContextFactory {
 public:
  explicit FactoryForMedia(const ProfileIOData* profile_io_data)
      : profile_io_data_(profile_io_data) {
  }

  virtual scoped_refptr<ChromeURLRequestContext> Create() {
    return profile_io_data_->GetMediaRequestContext();
  }

 private:
  const scoped_refptr<const ProfileIOData> profile_io_data_;
};

}  // namespace

// ----------------------------------------------------------------------------
// ChromeURLRequestContextGetter
// ----------------------------------------------------------------------------

ChromeURLRequestContextGetter::ChromeURLRequestContextGetter(
    Profile* profile,
    ChromeURLRequestContextFactory* factory)
    : io_thread_(g_browser_process->io_thread()),
      factory_(factory),
      url_request_context_(NULL) {
  DCHECK(factory);
  DCHECK(profile);
  RegisterPrefsObserver(profile);
}

ChromeURLRequestContextGetter::~ChromeURLRequestContextGetter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DCHECK(registrar_.IsEmpty()) << "Probably didn't call CleanupOnUIThread";

  // Either we already transformed the factory into a net::URLRequestContext, or
  // we still have a pending factory.
  DCHECK((factory_.get() && !url_request_context_.get()) ||
         (!factory_.get() && url_request_context_.get()));

  if (url_request_context_)
    io_thread_->UnregisterURLRequestContextGetter(this);

  // The scoped_refptr / scoped_ptr destructors take care of releasing
  // |factory_| and |url_request_context_| now.
}

// Lazily create a ChromeURLRequestContext using our factory.
net::URLRequestContext* ChromeURLRequestContextGetter::GetURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!url_request_context_) {
    DCHECK(factory_.get());
    url_request_context_ = factory_->Create();
    if (is_main()) {
      url_request_context_->set_is_main(true);
#if defined(USE_NSS)
      // TODO(ukai): find a better way to set the net::URLRequestContext for
      // OCSP.
      net::SetURLRequestContextForOCSP(url_request_context_);
#endif
    }

    factory_.reset();
    io_thread_->RegisterURLRequestContextGetter(this);
  }

  return url_request_context_;
}

void ChromeURLRequestContextGetter::ReleaseURLRequestContext() {
  DCHECK(url_request_context_);
  url_request_context_ = NULL;
}

net::CookieStore* ChromeURLRequestContextGetter::GetCookieStore() {
  // If we are running on the IO thread this is real easy.
  if (BrowserThread::CurrentlyOn(BrowserThread::IO))
    return GetURLRequestContext()->cookie_store();

  // If we aren't running on the IO thread, we cannot call
  // GetURLRequestContext(). Instead we will post a task to the IO loop
  // and wait for it to complete.

  base::WaitableEvent completion(false, false);
  net::CookieStore* result = NULL;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this,
          &ChromeURLRequestContextGetter::GetCookieStoreAsyncHelper,
          &completion,
          &result));

  completion.Wait();
  DCHECK(result);
  return result;
}

scoped_refptr<base::MessageLoopProxy>
ChromeURLRequestContextGetter::GetIOMessageLoopProxy() const {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
}

// static
ChromeURLRequestContextGetter* ChromeURLRequestContextGetter::CreateOriginal(
    Profile* profile,
    const ProfileIOData* profile_io_data) {
  DCHECK(!profile->IsOffTheRecord());
  return new ChromeURLRequestContextGetter(
      profile,
      new FactoryForMain(profile_io_data));
}

// static
ChromeURLRequestContextGetter*
ChromeURLRequestContextGetter::CreateOriginalForMedia(
    Profile* profile, const ProfileIOData* profile_io_data) {
  DCHECK(!profile->IsOffTheRecord());
  return new ChromeURLRequestContextGetter(
      profile,
      new FactoryForMedia(profile_io_data));
}

// static
ChromeURLRequestContextGetter*
ChromeURLRequestContextGetter::CreateOriginalForExtensions(
    Profile* profile, const ProfileIOData* profile_io_data) {
  DCHECK(!profile->IsOffTheRecord());
  return new ChromeURLRequestContextGetter(
      profile,
      new FactoryForExtensions(profile_io_data));
}

// static
ChromeURLRequestContextGetter*
ChromeURLRequestContextGetter::CreateOffTheRecord(
    Profile* profile, const ProfileIOData* profile_io_data) {
  DCHECK(profile->IsOffTheRecord());
  return new ChromeURLRequestContextGetter(
      profile, new FactoryForMain(profile_io_data));
}

// static
ChromeURLRequestContextGetter*
ChromeURLRequestContextGetter::CreateOffTheRecordForExtensions(
    Profile* profile, const ProfileIOData* profile_io_data) {
  DCHECK(profile->IsOffTheRecord());
  return new ChromeURLRequestContextGetter(
      profile, new FactoryForExtensions(profile_io_data));
}

void ChromeURLRequestContextGetter::CleanupOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Unregister for pref notifications.
  DCHECK(!registrar_.IsEmpty()) << "Called more than once!";
  registrar_.RemoveAll();
}

// NotificationObserver implementation.
void ChromeURLRequestContextGetter::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (NotificationType::PREF_CHANGED == type) {
    std::string* pref_name_in = Details<std::string>(details).ptr();
    PrefService* prefs = Source<PrefService>(source).ptr();
    DCHECK(pref_name_in && prefs);
    if (*pref_name_in == prefs::kAcceptLanguages) {
      std::string accept_language =
          prefs->GetString(prefs::kAcceptLanguages);
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(
              this,
              &ChromeURLRequestContextGetter::OnAcceptLanguageChange,
              accept_language));
    } else if (*pref_name_in == prefs::kDefaultCharset) {
      std::string default_charset =
          prefs->GetString(prefs::kDefaultCharset);
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(
              this,
              &ChromeURLRequestContextGetter::OnDefaultCharsetChange,
              default_charset));
    } else if (*pref_name_in == prefs::kClearSiteDataOnExit) {
      bool clear_site_data =
          prefs->GetBoolean(prefs::kClearSiteDataOnExit);
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(
              this,
              &ChromeURLRequestContextGetter::OnClearSiteDataOnExitChange,
              clear_site_data));
    }
  } else {
    NOTREACHED();
  }
}

void ChromeURLRequestContextGetter::RegisterPrefsObserver(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  registrar_.Init(profile->GetPrefs());
  registrar_.Add(prefs::kAcceptLanguages, this);
  registrar_.Add(prefs::kDefaultCharset, this);
  registrar_.Add(prefs::kClearSiteDataOnExit, this);
}

void ChromeURLRequestContextGetter::OnAcceptLanguageChange(
    const std::string& accept_language) {
  GetIOContext()->OnAcceptLanguageChange(accept_language);
}

void ChromeURLRequestContextGetter::OnDefaultCharsetChange(
    const std::string& default_charset) {
  GetIOContext()->OnDefaultCharsetChange(default_charset);
}

void ChromeURLRequestContextGetter::OnClearSiteDataOnExitChange(
    bool clear_site_data) {
  GetCookieStore()->GetCookieMonster()->
      SetClearPersistentStoreOnExit(clear_site_data);
}

void ChromeURLRequestContextGetter::GetCookieStoreAsyncHelper(
    base::WaitableEvent* completion,
    net::CookieStore** result) {
  // Note that CookieStore is refcounted, yet we do not add a reference.
  *result = GetURLRequestContext()->cookie_store();
  completion->Signal();
}

// ----------------------------------------------------------------------------
// ChromeURLRequestContext
// ----------------------------------------------------------------------------

ChromeURLRequestContext::ChromeURLRequestContext()
    : is_off_the_record_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void ChromeURLRequestContext::set_chrome_cookie_policy(
    ChromeCookiePolicy* cookie_policy) {
  chrome_cookie_policy_ = cookie_policy;  // Take a strong reference.
  set_cookie_policy(cookie_policy);
}

ChromeURLDataManagerBackend*
    ChromeURLRequestContext::GetChromeURLDataManagerBackend() {
  if (!chrome_url_data_manager_backend_.get())
    chrome_url_data_manager_backend_.reset(new ChromeURLDataManagerBackend());
  return chrome_url_data_manager_backend_.get();
}

ChromeURLRequestContext::~ChromeURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (appcache_service_.get() && appcache_service_->request_context() == this)
    appcache_service_->set_request_context(NULL);

#if defined(USE_NSS)
  if (is_main()) {
    net::URLRequestContext* ocsp_context = net::GetURLRequestContextForOCSP();
    if (ocsp_context) {
      DCHECK_EQ(this, ocsp_context);
      // We are releasing the net::URLRequestContext used by OCSP handlers.
      net::SetURLRequestContextForOCSP(NULL);
    }
  }
#endif

  NotificationService::current()->Notify(
      NotificationType::URL_REQUEST_CONTEXT_RELEASED,
      Source<net::URLRequestContext>(this),
      NotificationService::NoDetails());

  // cookie_policy_'s lifetime is auto-managed by chrome_cookie_policy_.  We
  // null this out here to avoid a dangling reference to chrome_cookie_policy_
  // when ~net::URLRequestContext runs.
  set_cookie_policy(NULL);
}

const std::string& ChromeURLRequestContext::GetUserAgent(
    const GURL& url) const {
  return webkit_glue::GetUserAgent(url);
}

void ChromeURLRequestContext::OnAcceptLanguageChange(
    const std::string& accept_language) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  set_accept_language(
      net::HttpUtil::GenerateAcceptLanguageHeader(accept_language));
}

void ChromeURLRequestContext::OnDefaultCharsetChange(
    const std::string& default_charset) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  set_referrer_charset(default_charset);
  set_accept_charset(
      net::HttpUtil::GenerateAcceptCharsetHeader(default_charset));
}
