// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IO_DATA_H_
#define CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IO_DATA_H_
#pragma once

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile_io_data.h"

class ChromeURLRequestContext;
class ChromeURLRequestContextGetter;
class Profile;

// OffTheRecordProfile owns a OffTheRecordProfileIOData::Handle, which holds a
// reference to the OffTheRecordProfileIOData. OffTheRecordProfileIOData is
// intended to own all the objects owned by OffTheRecordProfile which live on
// the IO thread, such as, but not limited to, network objects like
// CookieMonster, HttpTransactionFactory, etc. OffTheRecordProfileIOData is
// owned by the OffTheRecordProfile and OffTheRecordProfileIOData's
// ChromeURLRequestContexts. When all of them go away, then ProfileIOData will
// be deleted. Note that the OffTheRecordProfileIOData will typically outlive
// the Profile it is "owned" by, so it's important for OffTheRecordProfileIOData
// not to hold any references to the Profile beyond what's used by LazyParams
// (which should be deleted after lazy initialization).

class OffTheRecordProfileIOData : public ProfileIOData {
 public:
  class Handle {
   public:
    explicit Handle(Profile* profile);
    ~Handle();

    const content::ResourceContext& GetResourceContext() const;
    scoped_refptr<ChromeURLRequestContextGetter>
        GetMainRequestContextGetter() const;
    scoped_refptr<ChromeURLRequestContextGetter>
        GetExtensionsRequestContextGetter() const;
    scoped_refptr<ChromeURLRequestContextGetter>
        GetIsolatedAppRequestContextGetter(
            const std::string& app_id) const;

   private:
    typedef base::hash_map<std::string,
                           scoped_refptr<ChromeURLRequestContextGetter> >
        ChromeURLRequestContextGetterMap;

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
        extensions_request_context_getter_;
    mutable ChromeURLRequestContextGetterMap
        app_request_context_getter_map_;
    const scoped_refptr<OffTheRecordProfileIOData> io_data_;

    Profile* const profile_;

    mutable bool initialized_;

    DISALLOW_COPY_AND_ASSIGN(Handle);
  };

 private:
  friend class base::RefCountedThreadSafe<OffTheRecordProfileIOData>;

  typedef base::hash_map<std::string, net::HttpTransactionFactory* >
      HttpTransactionFactoryMap;

  OffTheRecordProfileIOData();
  ~OffTheRecordProfileIOData();

  virtual void LazyInitializeInternal(ProfileParams* profile_params) const;
  virtual scoped_refptr<RequestContext> InitializeAppRequestContext(
      scoped_refptr<ChromeURLRequestContext> main_context,
      const std::string& app_id) const;
  virtual scoped_refptr<ChromeURLRequestContext>
      AcquireMediaRequestContext() const;
  virtual scoped_refptr<ChromeURLRequestContext>
      AcquireIsolatedAppRequestContext(
          scoped_refptr<ChromeURLRequestContext> main_context,
          const std::string& app_id) const;

  mutable scoped_ptr<net::HttpTransactionFactory> main_http_factory_;

  // One HttpTransactionFactory per isolated app.
  mutable HttpTransactionFactoryMap app_http_factory_map_;

  DISALLOW_COPY_AND_ASSIGN(OffTheRecordProfileIOData);
};

#endif  // CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IO_DATA_H_
