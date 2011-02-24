// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IMPL_IO_DATA_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IMPL_IO_DATA_H_
#pragma once

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/profiles/profile_io_data.h"

class ExtensionIOEventRouter;
namespace net {
class NetworkDelegate;
class DnsCertProvenanceChecker;
class HttpTransactionFactory;
}  // namespace net

class ProfileImplIOData : public ProfileIOData {
 public:
  class Handle {
   public:
    explicit Handle(Profile* profile);
    ~Handle();

    bool HasMainRequestContext() const {
      return main_request_context_getter_ != NULL;
    }

    // Init() must be called before ~Handle(). It records all the necessary
    // parameters needed to construct a ChromeURLRequestContextGetter.
    void Init(const FilePath& cookie_path,
              const FilePath& cache_path,
              int cache_max_size,
              const FilePath& media_cache_path,
              int media_cache_max_size,
              const FilePath& extensions_cookie_path);

    scoped_refptr<ChromeURLRequestContextGetter>
        GetMainRequestContextGetter() const;
    scoped_refptr<ChromeURLRequestContextGetter>
        GetMediaRequestContextGetter() const;
    scoped_refptr<ChromeURLRequestContextGetter>
        GetExtensionsRequestContextGetter() const;

   private:
    // Lazily initialize ProfileParams. We do this on the calls to
    // Get*RequestContextGetter(), so we only initialize ProfileParams right
    // before posting a task to the IO thread to start using them. This prevents
    // objects that are supposed to be deleted on the IO thread, but are created
    // on the UI thread from being unnecessarily initialized.
    void LazyInitialize() const;

    // Ordering is important here. Do not reorder unless you know what you're
    // doing. We need to release |io_data_| *before* the getters, because we
    // want to make sure that the last reference for |io_data_| is on the IO
    // thread. The getters will be deleted on the IO thread, so they will
    // release their refs to their contexts, which will release the last refs to
    // the ProfileIOData on the IO thread.
    mutable scoped_refptr<ChromeURLRequestContextGetter>
        main_request_context_getter_;
    mutable scoped_refptr<ChromeURLRequestContextGetter>
        media_request_context_getter_;
    mutable scoped_refptr<ChromeURLRequestContextGetter>
        extensions_request_context_getter_;
    const scoped_refptr<ProfileImplIOData> io_data_;

    Profile* const profile_;

    mutable bool initialized_;

    DISALLOW_COPY_AND_ASSIGN(Handle);
  };

 private:
  friend class base::RefCountedThreadSafe<ProfileImplIOData>;

  struct LazyParams {
    LazyParams();
    ~LazyParams();

    // All of these parameters are intended to be read on the IO thread.
    FilePath cookie_path;
    FilePath cache_path;
    int cache_max_size;
    FilePath media_cache_path;
    int media_cache_max_size;
    FilePath extensions_cookie_path;
    IOThread* io_thread;

    ProfileParams profile_params;
  };

  ProfileImplIOData();
  virtual ~ProfileImplIOData();

  // Lazily initializes ProfileImplIOData.
  virtual void LazyInitializeInternal() const;
  virtual scoped_refptr<ChromeURLRequestContext>
      AcquireMainRequestContext() const;
  virtual scoped_refptr<ChromeURLRequestContext>
      AcquireMediaRequestContext() const;
  virtual scoped_refptr<ChromeURLRequestContext>
      AcquireExtensionsRequestContext() const;

  // Lazy initialization params.
  mutable scoped_ptr<LazyParams> lazy_params_;

  mutable scoped_refptr<RequestContext> main_request_context_;
  mutable scoped_refptr<RequestContext> media_request_context_;
  mutable scoped_refptr<RequestContext> extensions_request_context_;

  mutable scoped_refptr<ExtensionIOEventRouter> extension_io_event_router_;
  mutable scoped_ptr<net::NetworkDelegate> network_delegate_;
  mutable scoped_ptr<net::DnsCertProvenanceChecker> dns_cert_checker_;
  mutable scoped_ptr<net::HttpTransactionFactory> main_http_factory_;
  mutable scoped_ptr<net::HttpTransactionFactory> media_http_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfileImplIOData);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IMPL_IO_DATA_H_
