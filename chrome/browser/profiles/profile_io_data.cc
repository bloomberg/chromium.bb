// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_io_data.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_url_request_context.h"

ProfileIOData::Handle::Handle() : io_data_(new ProfileIOData) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ProfileIOData::Handle::~Handle() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (main_request_context_getter_)
    main_request_context_getter_->CleanupOnUIThread();
  if (media_request_context_getter_)
    media_request_context_getter_->CleanupOnUIThread();
  if (extensions_request_context_getter_)
    extensions_request_context_getter_->CleanupOnUIThread();
}

void ProfileIOData::Handle::Init(const FilePath& cookie_path,
                                 const FilePath& cache_path,
                                 int cache_max_size,
                                 const FilePath& media_cache_path,
                                 int media_cache_max_size,
                                 const FilePath& extensions_cookie_path,
                                 Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!io_data_->lazy_params_.get());
  LazyParams* lazy_params = new LazyParams;
  lazy_params->cookie_path = cookie_path;
  lazy_params->cache_path = cache_path;
  lazy_params->cache_max_size = cache_max_size;
  lazy_params->media_cache_path = media_cache_path;
  lazy_params->media_cache_max_size = media_cache_max_size;
  lazy_params->extensions_cookie_path = extensions_cookie_path;
  lazy_params->profile = profile;
  lazy_params->io_thread = g_browser_process->io_thread();
  io_data_->lazy_params_.reset(lazy_params);
}

scoped_refptr<ChromeURLRequestContextGetter>
ProfileIOData::Handle::GetMainRequestContextGetter() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!main_request_context_getter_) {
    main_request_context_getter_ =
        ChromeURLRequestContextGetter::CreateOriginal(
            io_data_->lazy_params_->profile, io_data_);
  }
  return main_request_context_getter_;
}

scoped_refptr<ChromeURLRequestContextGetter>
ProfileIOData::Handle::GetMediaRequestContextGetter() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!media_request_context_getter_) {
    media_request_context_getter_ =
        ChromeURLRequestContextGetter::CreateOriginalForMedia(
            io_data_->lazy_params_->profile, io_data_);
  }
  return media_request_context_getter_;
}

scoped_refptr<ChromeURLRequestContextGetter>
ProfileIOData::Handle::GetExtensionsRequestContextGetter() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!extensions_request_context_getter_) {
    extensions_request_context_getter_ =
        ChromeURLRequestContextGetter::CreateOriginalForExtensions(
            io_data_->lazy_params_->profile, io_data_);
  }
  return extensions_request_context_getter_;
}

class ProfileIOData::RequestContext : public ChromeURLRequestContext {
 public:
  RequestContext();
  ~RequestContext();

  void set_profile_io_data(const ProfileIOData* profile_io_data) {
    profile_io_data_ = profile_io_data;
  }

 private:
  scoped_refptr<const ProfileIOData> profile_io_data_;
};

ProfileIOData::RequestContext::RequestContext() {}
ProfileIOData::RequestContext::~RequestContext() {}

ProfileIOData::ProfileIOData() : initialized_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ProfileIOData::~ProfileIOData() {
  // If we have never initialized ProfileIOData, then Handle may hold the only
  // reference to it. The important thing is to make sure it hasn't been
  // initialized yet, because the lazily initialized variables are supposed to
  // live on the IO thread.
  if (BrowserThread::CurrentlyOn(BrowserThread::UI))
    DCHECK(!initialized_);
  else
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

scoped_refptr<ChromeURLRequestContext>
ProfileIOData::GetMainRequestContext() const {
  LazyInitialize();
  DCHECK(main_request_context_);
  scoped_refptr<ChromeURLRequestContext> context = main_request_context_;
  main_request_context_->set_profile_io_data(this);
  main_request_context_ = NULL;
  return context;
}

scoped_refptr<ChromeURLRequestContext>
ProfileIOData::GetMediaRequestContext() const {
  LazyInitialize();
  DCHECK(media_request_context_);
  scoped_refptr<ChromeURLRequestContext> context = media_request_context_;
  media_request_context_->set_profile_io_data(this);
  media_request_context_ = NULL;
  return context;
}

scoped_refptr<ChromeURLRequestContext>
ProfileIOData::GetExtensionsRequestContext() const {
  LazyInitialize();
  DCHECK(extensions_request_context_);
  scoped_refptr<ChromeURLRequestContext> context = extensions_request_context_;
  extensions_request_context_->set_profile_io_data(this);
  extensions_request_context_ = NULL;
  return context;
}

const ProfileIOData::LazyParams& ProfileIOData::lazy_params() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(lazy_params_.get());
  return *lazy_params_;
}

void ProfileIOData::LazyInitialize() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (initialized_)
    return;

  main_request_context_ = new RequestContext;
  media_request_context_ = new RequestContext;
  extensions_request_context_ = new RequestContext;

  // Initialize context members.
  IOThread::Globals* io_thread_globals = lazy_params_->io_thread->globals();

  main_request_context_->set_net_log(lazy_params_->io_thread->net_log());
  media_request_context_->set_net_log(lazy_params_->io_thread->net_log());

  main_request_context_->set_host_resolver(
      io_thread_globals->host_resolver.get());
  main_request_context_->set_cert_verifier(
      io_thread_globals->cert_verifier.get());
  main_request_context_->set_dnsrr_resolver(
      io_thread_globals->dnsrr_resolver.get());
  main_request_context_->set_network_delegate(
      &io_thread_globals->network_delegate);

  main_request_context_->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());
  media_request_context_->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());
  // TODO(cbentzel): How should extensions handle HTTP Authentication?
  extensions_request_context_->set_http_auth_handler_factory(
      io_thread_globals->http_auth_handler_factory.get());

  // TODO(willchan): Initialize more of the contexts!

  // TODO(willchan): Enable this when LazyInitialize() is able to fully
  // initialize all the ChromeURLRequestContexts.
#if 0
  params_.reset();
#endif

  initialized_ = true;
}
