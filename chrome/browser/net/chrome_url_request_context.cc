// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_url_request_context.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/content_client.h"
#include "net/base/cookie_store.h"
#include "net/http/http_util.h"

using content::BrowserThread;

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
  const ProfileIOData* const profile_io_data_;
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
  const ProfileIOData* const profile_io_data_;
};

// Factory that creates the ChromeURLRequestContext for a given isolated app.
class FactoryForIsolatedApp : public ChromeURLRequestContextFactory {
 public:
  FactoryForIsolatedApp(const ProfileIOData* profile_io_data,
                        const std::string& app_id,
                        ChromeURLRequestContextGetter* main_context)
      : profile_io_data_(profile_io_data),
        app_id_(app_id),
        main_request_context_getter_(main_context) {}

  virtual scoped_refptr<ChromeURLRequestContext> Create() {
    // We will copy most of the state from the main request context.
    return profile_io_data_->GetIsolatedAppRequestContext(
        main_request_context_getter_->GetIOContext(), app_id_);
  }

 private:
  const ProfileIOData* const profile_io_data_;
  const std::string app_id_;
  scoped_refptr<ChromeURLRequestContextGetter>
      main_request_context_getter_;
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
  const ProfileIOData* const profile_io_data_;
};

}  // namespace

// ----------------------------------------------------------------------------
// ChromeURLRequestContextGetter
// ----------------------------------------------------------------------------

ChromeURLRequestContextGetter::ChromeURLRequestContextGetter(
    Profile* profile,
    ChromeURLRequestContextFactory* factory)
    : io_thread_(g_browser_process->io_thread()),
      factory_(factory) {
  DCHECK(factory);
  DCHECK(profile);
  RegisterPrefsObserver(profile);
}

ChromeURLRequestContextGetter::~ChromeURLRequestContextGetter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DCHECK(registrar_.IsEmpty()) << "Probably didn't call CleanupOnUIThread";
}

// Lazily create a ChromeURLRequestContext using our factory.
net::URLRequestContext* ChromeURLRequestContextGetter::GetURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!url_request_context_) {
    DCHECK(factory_.get());
    url_request_context_ = factory_->Create()->GetWeakPtr();
    factory_.reset();
  }

  // Should not be NULL, unless we're trying to use the URLRequestContextGetter
  // after the Profile has already been deleted.
  CHECK(url_request_context_.get());

  return url_request_context_;
}

net::CookieStore* ChromeURLRequestContextGetter::DONTUSEME_GetCookieStore() {
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
      base::Bind(&ChromeURLRequestContextGetter::GetCookieStoreAsyncHelper,
          this,
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
ChromeURLRequestContextGetter::CreateOriginalForIsolatedApp(
    Profile* profile,
    const ProfileIOData* profile_io_data,
    const std::string& app_id) {
  DCHECK(!profile->IsOffTheRecord());
  ChromeURLRequestContextGetter* main_context =
      static_cast<ChromeURLRequestContextGetter*>(profile->GetRequestContext());
  return new ChromeURLRequestContextGetter(
      profile,
      new FactoryForIsolatedApp(profile_io_data, app_id, main_context));
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

// static
ChromeURLRequestContextGetter*
ChromeURLRequestContextGetter::CreateOffTheRecordForIsolatedApp(
    Profile* profile,
    const ProfileIOData* profile_io_data,
    const std::string& app_id) {
  DCHECK(profile->IsOffTheRecord());
  ChromeURLRequestContextGetter* main_context =
      static_cast<ChromeURLRequestContextGetter*>(profile->GetRequestContext());
  return new ChromeURLRequestContextGetter(
      profile,
      new FactoryForIsolatedApp(profile_io_data, app_id, main_context));
}

void ChromeURLRequestContextGetter::CleanupOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Unregister for pref notifications.
  DCHECK(!registrar_.IsEmpty()) << "Called more than once!";
  registrar_.RemoveAll();
}

// content::NotificationObserver implementation.
void ChromeURLRequestContextGetter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (chrome::NOTIFICATION_PREF_CHANGED == type) {
    std::string* pref_name_in = content::Details<std::string>(details).ptr();
    PrefService* prefs = content::Source<PrefService>(source).ptr();
    DCHECK(pref_name_in && prefs);
    if (*pref_name_in == prefs::kAcceptLanguages) {
      std::string accept_language =
          prefs->GetString(prefs::kAcceptLanguages);
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(
              &ChromeURLRequestContextGetter::OnAcceptLanguageChange,
              this,
              accept_language));
    } else if (*pref_name_in == prefs::kDefaultCharset) {
      std::string default_charset =
          prefs->GetString(prefs::kDefaultCharset);
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(
              &ChromeURLRequestContextGetter::OnDefaultCharsetChange,
              this,
              default_charset));
    } else if (*pref_name_in == prefs::kClearSiteDataOnExit) {
      bool clear_site_data =
          prefs->GetBoolean(prefs::kClearSiteDataOnExit);
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(
              &ChromeURLRequestContextGetter::OnClearSiteDataOnExitChange,
              this,
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
  net::CookieMonster* cookie_monster =
      GetURLRequestContext()->cookie_store()->GetCookieMonster();

  // If there is no cookie monster, this function does nothing. If
  // clear_site_data is true, this is most certainly not the expected behavior.
  DCHECK(!clear_site_data || cookie_monster);

  if (cookie_monster)
    cookie_monster->SetClearPersistentStoreOnExit(clear_site_data);
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
    : is_incognito_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void ChromeURLRequestContext::CopyFrom(ChromeURLRequestContext* other) {
  URLRequestContext::CopyFrom(other);

  // Copy ChromeURLRequestContext parameters.
  // ChromeURLDataManagerBackend is unique per context.
  set_is_incognito(other->is_incognito());
}

ChromeURLDataManagerBackend*
ChromeURLRequestContext::chrome_url_data_manager_backend() const {
    return chrome_url_data_manager_backend_;
}

void ChromeURLRequestContext::set_chrome_url_data_manager_backend(
        ChromeURLDataManagerBackend* backend) {
  DCHECK(backend);
  chrome_url_data_manager_backend_ = backend;
}

ChromeURLRequestContext::~ChromeURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

const std::string& ChromeURLRequestContext::GetUserAgent(
    const GURL& url) const {
  return content::GetUserAgent(url);
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
