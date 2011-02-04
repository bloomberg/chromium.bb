// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
#pragma once

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

class ChromeURLRequestContext;
class ChromeURLRequestContextGetter;
class IOThread;
class Profile;

// ProfileImpl owns a ProfileIOData::Handle, which holds a reference to the
// ProfileIOData. ProfileIOData is intended to own all the objects owned by
// ProfileImpl which live on the IO thread, such as, but not limited to, network
// objects like CookieMonster, HttpTransactionFactory, etc. ProfileIOData is
// owned by the ProfileImpl and ProfileIOData's ChromeURLRequestContexts. When
// all of them go away, then ProfileIOData will be deleted. Note that the
// ProfileIOData will typically outlive the Profile it is "owned" by, so it's
// important for ProfileIOData not to hold any references to the Profile beyond
// what's used by LazyParams (which should be deleted after lazy
// initialization).
class ProfileIOData : public base::RefCountedThreadSafe<ProfileIOData> {
 public:
  class Handle {
   public:
    Handle();
    ~Handle();

    // Init() must be called before ~Handle(). It records all the necessary
    // parameters needed to construct a ChromeURLRequestContextGetter.
    void Init(const FilePath& cookie_path,
              const FilePath& cache_path,
              int cache_max_size,
              const FilePath& media_cache_path,
              int media_cache_max_size,
              const FilePath& extensions_cookie_path,
              Profile* profile);

    bool HasMainRequestContext() const {
      return main_request_context_getter_ != NULL;
    }
    scoped_refptr<ChromeURLRequestContextGetter>
        GetMainRequestContextGetter() const;
    scoped_refptr<ChromeURLRequestContextGetter>
        GetMediaRequestContextGetter() const;
    scoped_refptr<ChromeURLRequestContextGetter>
        GetExtensionsRequestContextGetter() const;

   private:
    // Ordering is important here. Do not reorder unless you know what you're
    // doing. |io_data_| must be released before the getters to ensure
    // that ProfileIOData is deleted on the IO thread.
    mutable scoped_refptr<ChromeURLRequestContextGetter>
        main_request_context_getter_;
    mutable scoped_refptr<ChromeURLRequestContextGetter>
        media_request_context_getter_;
    mutable scoped_refptr<ChromeURLRequestContextGetter>
        extensions_request_context_getter_;
    const scoped_refptr<ProfileIOData> io_data_;

    DISALLOW_COPY_AND_ASSIGN(Handle);
  };

  // TODO(willchan): Move this to the private section when
  // ChromeURLRequestContextFactory subclasses don't need it anymore.
  struct LazyParams {
    // All of these parameters are intended to be read on the IO thread.
    FilePath cookie_path;
    FilePath cache_path;
    int cache_max_size;
    FilePath media_cache_path;
    int media_cache_max_size;
    FilePath extensions_cookie_path;
    IOThread* io_thread;

    // TODO(willchan): Kill this, since the IO thread shouldn't be reading from
    // the Profile. Instead, replace this with the parameters we want to copy
    // from the UI thread to the IO thread.
    Profile* profile;
  };

  // These should only be called at most once each. Ownership is reversed they
  // get called, from ProfileIOData owning ChromeURLRequestContext to vice
  // versa.
  scoped_refptr<ChromeURLRequestContext> GetMainRequestContext() const;
  scoped_refptr<ChromeURLRequestContext> GetMediaRequestContext() const;
  scoped_refptr<ChromeURLRequestContext> GetExtensionsRequestContext() const;

  // TODO(willchan): Delete this when ChromeURLRequestContextFactory subclasses
  // don't need it anymore.
  const LazyParams& lazy_params() const;

 private:
  friend class base::RefCountedThreadSafe<ProfileIOData>;

  class RequestContext;

  ProfileIOData();
  ~ProfileIOData();

  // Lazily initializes ProfileIOData.
  void LazyInitialize() const;

  // Lazy initialization params.
  // TODO(willchan): Delete after Initialize() finishes initializing all the
  // contexts.
  scoped_ptr<const LazyParams> lazy_params_;

  mutable bool initialized_;
  mutable scoped_refptr<RequestContext> main_request_context_;
  mutable scoped_refptr<RequestContext> media_request_context_;
  mutable scoped_refptr<RequestContext> extensions_request_context_;

  DISALLOW_COPY_AND_ASSIGN(ProfileIOData);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
