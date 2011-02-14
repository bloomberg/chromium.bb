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
#include "chrome/browser/net/chrome_url_request_context.h"
#include "net/base/cookie_monster.h"

class CommandLine;
class ChromeAppCacheService;
class ChromeBlobStorageContext;
class ChromeURLRequestContext;
class ChromeURLRequestContextGetter;
class ExtensionInfoMap;
class ExtensionIOEventRouter;
namespace fileapi {
class FileSystemContext;
}
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
class PrerenderManager;
class Profile;
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
  // These should only be called at most once each. Ownership is reversed they
  // get called, from ProfileIOData owning ChromeURLRequestContext to vice
  // versa.
  scoped_refptr<ChromeURLRequestContext> GetMainRequestContext() const;
  scoped_refptr<ChromeURLRequestContext> GetMediaRequestContext() const;
  scoped_refptr<ChromeURLRequestContext> GetExtensionsRequestContext() const;

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

    bool is_off_the_record;
    bool clear_local_state_on_exit;
    std::string accept_language;
    std::string accept_charset;
    std::string referrer_charset;
    FilePath user_script_dir_path;
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
    scoped_refptr<ExtensionIOEventRouter> extension_io_event_router;
    scoped_refptr<PrerenderManager> prerender_manager;
    // We need to initialize the ProxyConfigService from the UI thread
    // because on linux it relies on initializing things through gconf,
    // and needs to be on the main thread.
    scoped_ptr<net::ProxyConfigService> proxy_config_service;
  };

  explicit ProfileIOData(bool is_off_the_record);
  virtual ~ProfileIOData();

  // Static helper functions to assist in common operations executed by
  // subtypes.

  static void InitializeProfileParams(Profile* profile, ProfileParams* params);
  static void ApplyProfileParamsToContext(const ProfileParams& profile_params,
                                          ChromeURLRequestContext* context);
  static net::ProxyConfigService* CreateProxyConfigService(Profile* profile);
  static net::ProxyService* CreateProxyService(
    net::NetLog* net_log,
    net::URLRequestContext* context,
    net::ProxyConfigService* proxy_config_service,
    const CommandLine& command_line);

  // Lazy initializes the ProfileIOData object the first time a request context
  // is requested. The lazy logic is implemented here. The actual initialization
  // is done in LazyInitializeInternal(), implemented by subtypes. Static helper
  // functions have been provided to assist in common operations.
  void LazyInitialize() const;

  // --------------------------------------------
  // Virtual interface for subtypes to implement:
  // --------------------------------------------

  // Does that actual initialization of the ProfileIOData subtype. Subtypes
  // should use the static helper functions above to implement this.
  virtual void LazyInitializeInternal() const = 0;

  // These functions are used to transfer ownership of the lazily initialized
  // context from ProfileIOData to the URLRequestContextGetter.
  virtual scoped_refptr<ChromeURLRequestContext>
      AcquireMainRequestContext() const = 0;
  virtual scoped_refptr<ChromeURLRequestContext>
      AcquireMediaRequestContext() const = 0;
  virtual scoped_refptr<ChromeURLRequestContext>
      AcquireExtensionsRequestContext() const = 0;

  mutable bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(ProfileIOData);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
