// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
#pragma once

#include <set>
#include "base/basictypes.h"
#include "base/debug/stack_trace.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/resource_context.h"
#include "net/base/cookie_monster.h"

class CommandLine;
class ChromeAppCacheService;
class ChromeBlobStorageContext;
class ExtensionInfoMap;
namespace fileapi {
class FileSystemContext;
}  // namespace fileapi
class HostContentSettingsMap;
class HostZoomMap;
class IOThread;
namespace net {
class DnsCertProvenanceChecker;
class NetLog;
class ProxyConfigService;
class ProxyService;
class SSLConfigService;
class TransportSecurityState;
}  // namespace net
namespace prerender {
class PrerenderManager;
};  // namespace prerender
class ProtocolHandlerRegistry;
namespace webkit_database {
class DatabaseTracker;
}  // webkit_database

// Conceptually speaking, the ProfileIOData represents data that lives on the IO
// thread that is owned by a Profile, such as, but not limited to, network
// objects like CookieMonster, HttpTransactionFactory, etc. The Profile
// implementation will maintain a reference to the ProfileIOData. The
// ProfileIOData will originally own a reference to the ChromeURLRequestContexts
// that reference its members. When an accessor for a ChromeURLRequestContext is
// invoked, then ProfileIOData will release its reference to the
// ChromeURLRequestContext and the ChromeURLRequestContext will acquire a
// reference to the ProfileIOData, so they exchange ownership. This is done
// because it's possible for a context's accessor never to be invoked, so this
// ownership reversal prevents shutdown leaks. ProfileIOData will lazily
// initialize its members on the first invocation of a ChromeURLRequestContext
// accessor.
class ProfileIOData : public base::RefCountedThreadSafe<ProfileIOData> {
 public:
  // These should only be called at most once each. Ownership is reversed when
  // they get called, from ProfileIOData owning ChromeURLRequestContext to vice
  // versa.
  scoped_refptr<ChromeURLRequestContext> GetMainRequestContext() const;
  scoped_refptr<ChromeURLRequestContext> GetMediaRequestContext() const;
  scoped_refptr<ChromeURLRequestContext> GetExtensionsRequestContext() const;
  scoped_refptr<ChromeURLRequestContext> GetIsolatedAppRequestContext(
      scoped_refptr<ChromeURLRequestContext> main_context,
      const std::string& app_id) const;
  const content::ResourceContext& GetResourceContext() const;

 protected:
  friend class base::RefCountedThreadSafe<ProfileIOData>;

  class RequestContext : public ChromeURLRequestContext {
   public:
    RequestContext();
    ~RequestContext();

    // Setter is used to transfer ownership of the ProfileIOData to the context.
    void set_profile_io_data(const ProfileIOData* profile_io_data) {
      profile_io_data_ = profile_io_data;
    }

   private:
    scoped_refptr<const ProfileIOData> profile_io_data_;
  };

  // Created on the UI thread, read on the IO thread during ProfileIOData lazy
  // initialization.
  struct ProfileParams {
    ProfileParams();
    ~ProfileParams();

    bool is_incognito;
    bool clear_local_state_on_exit;
    std::string accept_language;
    std::string accept_charset;
    std::string referrer_charset;
    FilePath user_script_dir_path;
    IOThread* io_thread;
    scoped_refptr<HostContentSettingsMap> host_content_settings_map;
    scoped_refptr<HostZoomMap> host_zoom_map;
    scoped_refptr<net::TransportSecurityState> transport_security_state;
    scoped_refptr<net::SSLConfigService> ssl_config_service;
    scoped_refptr<net::CookieMonster::Delegate> cookie_monster_delegate;
    scoped_refptr<webkit_database::DatabaseTracker> database_tracker;
    scoped_refptr<ChromeAppCacheService> appcache_service;
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context;
    scoped_refptr<fileapi::FileSystemContext> file_system_context;
    scoped_refptr<ExtensionInfoMap> extension_info_map;
    scoped_refptr<prerender::PrerenderManager> prerender_manager;
    scoped_refptr<ProtocolHandlerRegistry> protocol_handler_registry;
    // We need to initialize the ProxyConfigService from the UI thread
    // because on linux it relies on initializing things through gconf,
    // and needs to be on the main thread.
    scoped_ptr<net::ProxyConfigService> proxy_config_service;
    // The profile this struct was populated from.
    ProfileId profile_id;
  };

  explicit ProfileIOData(bool is_incognito);
  virtual ~ProfileIOData();

  void InitializeProfileParams(Profile* profile);
  void ApplyProfileParamsToContext(ChromeURLRequestContext* context) const;

  // Lazy initializes the ProfileIOData object the first time a request context
  // is requested. The lazy logic is implemented here. The actual initialization
  // is done in LazyInitializeInternal(), implemented by subtypes. Static helper
  // functions have been provided to assist in common operations.
  void LazyInitialize() const;

  // Called when the profile is destroyed.
  void ShutdownOnUIThread();

  BooleanPrefMember* enable_referrers() const {
    return &enable_referrers_;
  }

  net::NetworkDelegate* network_delegate() const {
    return network_delegate_.get();
  }

  net::DnsCertProvenanceChecker* dns_cert_checker() const {
    return dns_cert_checker_.get();
  }

  net::ProxyService* proxy_service() const {
    return proxy_service_.get();
  }

  net::CookiePolicy* cookie_policy() const {
    return cookie_policy_.get();
  }

  ChromeURLRequestContext* main_request_context() const {
    return main_request_context_;
  }

  ChromeURLRequestContext* extensions_request_context() const {
    return extensions_request_context_;
  }

 private:
  class ResourceContext : public content::ResourceContext {
   public:
    explicit ResourceContext(const ProfileIOData* io_data);
    virtual ~ResourceContext();

   private:
    virtual void EnsureInitialized() const;

    const ProfileIOData* const io_data_;
  };

  // --------------------------------------------
  // Virtual interface for subtypes to implement:
  // --------------------------------------------

  // Does the actual initialization of the ProfileIOData subtype. Subtypes
  // should use the static helper functions above to implement this.
  virtual void LazyInitializeInternal(ProfileParams* profile_params) const = 0;

  // Does an on-demand initialization of a RequestContext for the given
  // isolated app.
  virtual scoped_refptr<RequestContext> InitializeAppRequestContext(
      scoped_refptr<ChromeURLRequestContext> main_context,
      const std::string& app_id) const = 0;

  // These functions are used to transfer ownership of the lazily initialized
  // context from ProfileIOData to the URLRequestContextGetter.
  virtual scoped_refptr<ChromeURLRequestContext>
      AcquireMediaRequestContext() const = 0;
  virtual scoped_refptr<ChromeURLRequestContext>
      AcquireIsolatedAppRequestContext(
          scoped_refptr<ChromeURLRequestContext> main_context,
          const std::string& app_id) const = 0;

  // Tracks whether or not we've been lazily initialized.
  mutable bool initialized_;

  // Data from the UI thread from the Profile, used to initialize ProfileIOData.
  // Deleted after lazy initialization.
  mutable scoped_ptr<ProfileParams> profile_params_;

  // Member variables which are pointed to by the various context objects.
  mutable BooleanPrefMember enable_referrers_;

  // Pointed to by URLRequestContext.
  mutable scoped_ptr<net::NetworkDelegate> network_delegate_;
  mutable scoped_ptr<net::DnsCertProvenanceChecker> dns_cert_checker_;
  mutable scoped_refptr<net::ProxyService> proxy_service_;
  mutable scoped_ptr<net::CookiePolicy> cookie_policy_;

  // Pointed to by ResourceContext.
  mutable scoped_refptr<webkit_database::DatabaseTracker> database_tracker_;
  mutable scoped_refptr<ChromeAppCacheService> appcache_service_;
  mutable scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  mutable scoped_refptr<fileapi::FileSystemContext> file_system_context_;

  mutable ResourceContext resource_context_;

  // These are only valid in between LazyInitialize() and their accessor being
  // called.
  mutable scoped_refptr<RequestContext> main_request_context_;
  mutable scoped_refptr<RequestContext> extensions_request_context_;

  DISALLOW_COPY_AND_ASSIGN(ProfileIOData);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
