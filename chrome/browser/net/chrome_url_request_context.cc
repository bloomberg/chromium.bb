// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_url_request_context.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/load_time_stats.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/storage_partition_descriptor.h"
#include "content/public/browser/browser_thread.h"
#include "net/cookies/cookie_store.h"

using content::BrowserThread;

class ChromeURLRequestContextFactory {
 public:
  ChromeURLRequestContextFactory() {}
  virtual ~ChromeURLRequestContextFactory() {}

  // Called to create a new instance (will only be called once).
  virtual ChromeURLRequestContext* Create() = 0;

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

  virtual ChromeURLRequestContext* Create() OVERRIDE {
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

  virtual ChromeURLRequestContext* Create() OVERRIDE {
    return profile_io_data_->GetExtensionsRequestContext();
  }

 private:
  const ProfileIOData* const profile_io_data_;
};

// Factory that creates the ChromeURLRequestContext for a given isolated app.
class FactoryForIsolatedApp : public ChromeURLRequestContextFactory {
 public:
  FactoryForIsolatedApp(
      const ProfileIOData* profile_io_data,
      const StoragePartitionDescriptor& partition_descriptor,
      ChromeURLRequestContextGetter* main_context,
      scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
          protocol_handler_interceptor)
      : profile_io_data_(profile_io_data),
        partition_descriptor_(partition_descriptor),
        main_request_context_getter_(main_context),
        protocol_handler_interceptor_(protocol_handler_interceptor.Pass()) {}

  virtual ChromeURLRequestContext* Create() OVERRIDE {
    // We will copy most of the state from the main request context.
    //
    // Note that this factory is one-shot.  After Create() is called once, the
    // factory is actually destroyed. Thus it is safe to destructively pass
    // state onwards.
    return profile_io_data_->GetIsolatedAppRequestContext(
        main_request_context_getter_->GetIOContext(), partition_descriptor_,
        protocol_handler_interceptor_.Pass());
  }

 private:
  const ProfileIOData* const profile_io_data_;
  const StoragePartitionDescriptor partition_descriptor_;
  scoped_refptr<ChromeURLRequestContextGetter>
      main_request_context_getter_;
  scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
      protocol_handler_interceptor_;
};

// Factory that creates the media ChromeURLRequestContext for a given isolated
// app.  The media context is based on the corresponding isolated app's context.
class FactoryForIsolatedMedia : public ChromeURLRequestContextFactory {
 public:
  FactoryForIsolatedMedia(
      const ProfileIOData* profile_io_data,
      const StoragePartitionDescriptor& partition_descriptor,
      ChromeURLRequestContextGetter* app_context)
    : profile_io_data_(profile_io_data),
      partition_descriptor_(partition_descriptor),
      app_context_getter_(app_context) {}

  virtual ChromeURLRequestContext* Create() OVERRIDE {
    // We will copy most of the state from the corresopnding app's
    // request context. We expect to have the same lifetime as
    // the associated |app_context_getter_| so we can just reuse
    // all its backing objects, including the
    // |protocol_handler_interceptor|.  This is why the API
    // looks different from FactoryForIsolatedApp's.
    return profile_io_data_->GetIsolatedMediaRequestContext(
        app_context_getter_->GetIOContext(), partition_descriptor_);
  }

 private:
  const ProfileIOData* const profile_io_data_;
  const StoragePartitionDescriptor partition_descriptor_;
  scoped_refptr<ChromeURLRequestContextGetter> app_context_getter_;
};

// Factory that creates the ChromeURLRequestContext for media.
class FactoryForMedia : public ChromeURLRequestContextFactory {
 public:
  explicit FactoryForMedia(const ProfileIOData* profile_io_data)
      : profile_io_data_(profile_io_data) {
  }

  virtual ChromeURLRequestContext* Create() OVERRIDE {
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
    ChromeURLRequestContextFactory* factory)
    : factory_(factory) {
  DCHECK(factory);
}

ChromeURLRequestContextGetter::~ChromeURLRequestContextGetter() {}

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

scoped_refptr<base::SingleThreadTaskRunner>
ChromeURLRequestContextGetter::GetNetworkTaskRunner() const {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
}

// static
ChromeURLRequestContextGetter* ChromeURLRequestContextGetter::CreateOriginal(
    Profile* profile,
    const ProfileIOData* profile_io_data) {
  DCHECK(!profile->IsOffTheRecord());
  return new ChromeURLRequestContextGetter(
      new FactoryForMain(profile_io_data));
}

// static
ChromeURLRequestContextGetter*
ChromeURLRequestContextGetter::CreateOriginalForMedia(
    Profile* profile, const ProfileIOData* profile_io_data) {
  DCHECK(!profile->IsOffTheRecord());
  return new ChromeURLRequestContextGetter(
      new FactoryForMedia(profile_io_data));
}

// static
ChromeURLRequestContextGetter*
ChromeURLRequestContextGetter::CreateOriginalForExtensions(
    Profile* profile, const ProfileIOData* profile_io_data) {
  DCHECK(!profile->IsOffTheRecord());
  return new ChromeURLRequestContextGetter(
      new FactoryForExtensions(profile_io_data));
}

// static
ChromeURLRequestContextGetter*
ChromeURLRequestContextGetter::CreateOriginalForIsolatedApp(
    Profile* profile,
    const ProfileIOData* profile_io_data,
    const StoragePartitionDescriptor& partition_descriptor,
    scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
        protocol_handler_interceptor) {
  DCHECK(!profile->IsOffTheRecord());
  ChromeURLRequestContextGetter* main_context =
      static_cast<ChromeURLRequestContextGetter*>(profile->GetRequestContext());
  return new ChromeURLRequestContextGetter(
      new FactoryForIsolatedApp(profile_io_data, partition_descriptor,
           main_context, protocol_handler_interceptor.Pass()));
}

// static
ChromeURLRequestContextGetter*
ChromeURLRequestContextGetter::CreateOriginalForIsolatedMedia(
    Profile* profile,
    ChromeURLRequestContextGetter* app_context,
    const ProfileIOData* profile_io_data,
    const StoragePartitionDescriptor& partition_descriptor) {
  DCHECK(!profile->IsOffTheRecord());
  return new ChromeURLRequestContextGetter(
      new FactoryForIsolatedMedia(
          profile_io_data, partition_descriptor, app_context));
}

// static
ChromeURLRequestContextGetter*
ChromeURLRequestContextGetter::CreateOffTheRecord(
    Profile* profile, const ProfileIOData* profile_io_data) {
  DCHECK(profile->IsOffTheRecord());
  return new ChromeURLRequestContextGetter(
      new FactoryForMain(profile_io_data));
}

// static
ChromeURLRequestContextGetter*
ChromeURLRequestContextGetter::CreateOffTheRecordForExtensions(
    Profile* profile, const ProfileIOData* profile_io_data) {
  DCHECK(profile->IsOffTheRecord());
  return new ChromeURLRequestContextGetter(
      new FactoryForExtensions(profile_io_data));
}

// static
ChromeURLRequestContextGetter*
ChromeURLRequestContextGetter::CreateOffTheRecordForIsolatedApp(
    Profile* profile,
    const ProfileIOData* profile_io_data,
    const StoragePartitionDescriptor& partition_descriptor,
    scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
        protocol_handler_interceptor) {
  DCHECK(profile->IsOffTheRecord());
  ChromeURLRequestContextGetter* main_context =
      static_cast<ChromeURLRequestContextGetter*>(profile->GetRequestContext());
  return new ChromeURLRequestContextGetter(
      new FactoryForIsolatedApp(profile_io_data, partition_descriptor,
          main_context, protocol_handler_interceptor.Pass()));
}

// ----------------------------------------------------------------------------
// ChromeURLRequestContext
// ----------------------------------------------------------------------------

ChromeURLRequestContext::ChromeURLRequestContext(
    ContextType type,
    chrome_browser_net::LoadTimeStats* load_time_stats)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      load_time_stats_(load_time_stats) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (load_time_stats_)
    load_time_stats_->RegisterURLRequestContext(this, type);
}

ChromeURLRequestContext::~ChromeURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (load_time_stats_)
    load_time_stats_->UnregisterURLRequestContext(this);
}

void ChromeURLRequestContext::CopyFrom(ChromeURLRequestContext* other) {
  URLRequestContext::CopyFrom(other);

  // Copy ChromeURLRequestContext parameters.
}
