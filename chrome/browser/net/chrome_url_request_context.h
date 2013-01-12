// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_URL_REQUEST_CONTEXT_H_
#define CHROME_BROWSER_NET_CHROME_URL_REQUEST_CONTEXT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory.h"

class ChromeURLRequestContextFactory;
class IOThread;
class Profile;
class ProfileIOData;
struct StoragePartitionDescriptor;

namespace chrome_browser_net {
class LoadTimeStats;
}

// Subclass of net::URLRequestContext which can be used to store extra
// information for requests.
//
// All methods of this class must be called from the IO thread,
// including the constructor and destructor.
class ChromeURLRequestContext : public net::URLRequestContext {
 public:
  enum ContextType {
    CONTEXT_TYPE_MAIN,
    CONTEXT_TYPE_MEDIA,
    CONTEXT_TYPE_EXTENSIONS,
    CONTEXT_TYPE_APP
  };
  ChromeURLRequestContext(ContextType type,
                          chrome_browser_net::LoadTimeStats* load_time_stats);
  virtual ~ChromeURLRequestContext();

  base::WeakPtr<ChromeURLRequestContext> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Copies the state from |other| into this context.
  void CopyFrom(ChromeURLRequestContext* other);

  bool is_incognito() const {
    return is_incognito_;
  }

  void set_is_incognito(bool is_incognito) {
    is_incognito_ = is_incognito;
  }

 private:
  base::WeakPtrFactory<ChromeURLRequestContext> weak_factory_;

  // ---------------------------------------------------------------------------
  // Important: When adding any new members below, consider whether they need to
  // be added to CopyFrom.
  // ---------------------------------------------------------------------------

  bool is_incognito_;
  chrome_browser_net::LoadTimeStats* load_time_stats_;

  // ---------------------------------------------------------------------------
  // Important: When adding any new members above, consider whether they need to
  // be added to CopyFrom.
  // ---------------------------------------------------------------------------

  DISALLOW_COPY_AND_ASSIGN(ChromeURLRequestContext);
};

// A net::URLRequestContextGetter subclass used by the browser. This returns a
// subclass of net::URLRequestContext which can be used to store extra
// information about requests.
//
// Most methods are expected to be called on the UI thread, except for
// the destructor and GetURLRequestContext().
class ChromeURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  // Constructs a ChromeURLRequestContextGetter that will use |factory| to
  // create the ChromeURLRequestContext.
  explicit ChromeURLRequestContextGetter(
      ChromeURLRequestContextFactory* factory);

  // Note that GetURLRequestContext() can only be called from the IO
  // thread (it will assert otherwise).
  // GetIOMessageLoopProxy however can be called from any thread.
  //
  // net::URLRequestContextGetter implementation.
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE;

  // Convenience overload of GetURLRequestContext() that returns a
  // ChromeURLRequestContext* rather than a net::URLRequestContext*.
  ChromeURLRequestContext* GetIOContext() {
    return reinterpret_cast<ChromeURLRequestContext*>(GetURLRequestContext());
  }

  // Create an instance for use with an 'original' (non-OTR) profile. This is
  // expected to get called on the UI thread.
  static ChromeURLRequestContextGetter* CreateOriginal(
      Profile* profile, const ProfileIOData* profile_io_data);

  // Create an instance for an original profile for media. This is expected to
  // get called on UI thread. This method takes a profile and reuses the
  // 'original' net::URLRequestContext for common files.
  static ChromeURLRequestContextGetter* CreateOriginalForMedia(
      Profile* profile, const ProfileIOData* profile_io_data);

  // Create an instance for an original profile for extensions. This is expected
  // to get called on UI thread.
  static ChromeURLRequestContextGetter* CreateOriginalForExtensions(
      Profile* profile, const ProfileIOData* profile_io_data);

  // Create an instance for an original profile for an app with isolated
  // storage. This is expected to get called on UI thread.
  static ChromeURLRequestContextGetter* CreateOriginalForIsolatedApp(
      Profile* profile,
      const ProfileIOData* profile_io_data,
      const StoragePartitionDescriptor& partition_descriptor,
      scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
          protocol_handler_interceptor);

  // Create an instance for an original profile for media with isolated
  // storage. This is expected to get called on UI thread.
  static ChromeURLRequestContextGetter* CreateOriginalForIsolatedMedia(
      Profile* profile,
      ChromeURLRequestContextGetter* app_context,
      const ProfileIOData* profile_io_data,
      const StoragePartitionDescriptor& partition_descriptor);

  // Create an instance for use with an OTR profile. This is expected to get
  // called on the UI thread.
  static ChromeURLRequestContextGetter* CreateOffTheRecord(
      Profile* profile, const ProfileIOData* profile_io_data);

  // Create an instance for an OTR profile for extensions. This is expected
  // to get called on UI thread.
  static ChromeURLRequestContextGetter* CreateOffTheRecordForExtensions(
      Profile* profile, const ProfileIOData* profile_io_data);

  // Create an instance for an OTR profile for an app with isolated storage.
  // This is expected to get called on UI thread.
  static ChromeURLRequestContextGetter* CreateOffTheRecordForIsolatedApp(
      Profile* profile,
      const ProfileIOData* profile_io_data,
      const StoragePartitionDescriptor& partition_descriptor,
      scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
          protocol_handler_interceptor);

 private:
  virtual ~ChromeURLRequestContextGetter();

  // Deferred logic for creating a ChromeURLRequestContext.
  // Access only from the IO thread.
  scoped_ptr<ChromeURLRequestContextFactory> factory_;

  // NULL if not yet initialized. Otherwise, it is the ChromeURLRequestContext
  // instance that was lazily created by GetURLRequestContext().
  // Access only from the IO thread.
  base::WeakPtr<ChromeURLRequestContext> url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(ChromeURLRequestContextGetter);
};

#endif  // CHROME_BROWSER_NET_CHROME_URL_REQUEST_CONTEXT_H_
