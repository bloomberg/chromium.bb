// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CRNET_CRNET_ENVIRONMENT_H_
#define IOS_CRNET_CRNET_ENVIRONMENT_H_

#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#import "ios/crnet/CrNet.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

class JsonPrefStore;

namespace net {
class HttpCache;
class NetworkChangeNotifier;
class NetLog;
class ProxyConfigService;
class SdchManager;
class SdchOwner;
class URLRequestContextGetter;
class WriteToFileNetLogObserver;
}

class CrNetHttpProtocolHandlerDelegate;

// CrNetEnvironment contains all the network stack configuration
// and initialization.
class CrNetEnvironment {
 public:
  // Must be called on the main thread.
  static void Initialize();

  // |user_agent_product_name| will be used to generate the user-agent.
  CrNetEnvironment(const std::string& user_agent_product_name);
  ~CrNetEnvironment();

  // Installs this CrNet environment so requests are intercepted.
  // Can only be called once; to enable/disable CrNet at runtime, use
  // SetHTTPProtocolHandlerRegistered.
  void Install();

  // Installs this CrNet environment into the supplied
  // NSURLSessionConfiguration. Settings are inherited from the shared
  // NSURLSessionConfiguration, which Install() affects.
  void InstallIntoSessionConfiguration(NSURLSessionConfiguration* config);

  // The full user-agent.
  std::string user_agent();

  // Returns the global request context getter for use in the network stack.
  //
  // The request context gathers all the settings needed to do an actual network
  // request (cache type and path, cookies store, proxy setting ...).
  // Chrome network stacks supports multiple active contexts, and this is used
  // for example to separate Incognito data from the main profile data.
  // CrNetEnvironment only implement one request context for now, but it can be
  // extended in the future.
  net::URLRequestContextGetter* GetMainContextGetter();

  // Enables or disables the HTTP protocol handler.
  //
  // When the HTTP protocol handler is registered, it will be used for all the
  // network requests the application does (including requests from UIWebView).
  void SetHTTPProtocolHandlerRegistered(bool registered);

  // Creates a new net log (overwrites existing file with this name). If
  // actively logging, this call is ignored.
  void StartNetLog(base::FilePath::StringType file_name, bool log_bytes);
  // Stops logging and flushes file. If not currently logging this call is
  // ignored.
  void StopNetLog();
  // Closes all existing SPDY sessions with ERR_ABORTED.
  void CloseAllSpdySessions();

  // Sets the block used to determine whether or not CrNet should handle the
  // request. If this is not set, CrNet will handle all requests.
  // Must not be called while requests are in progress.
  void SetRequestFilterBlock(RequestFilterBlock block);

  // Setters and getters for |spdy_enabled_|, |quic_enabled_|, and
  // |forced_quic_origin_|. These only have any effect before Install() is
  // called.
  void set_spdy_enabled(bool enabled) { spdy_enabled_ = enabled; }
  void set_quic_enabled(bool enabled) { quic_enabled_ = enabled; }
  void set_sdch_enabled(bool enabled) { sdch_enabled_ = enabled; }
  void set_sdch_pref_store_filename(const std::string& pref_store) {
    sdch_pref_store_filename_ = pref_store;
  }
  void set_alternate_protocol_threshold(double threshold) {
    alternate_protocol_threshold_ = threshold;
  }

  bool spdy_enabled() const { return spdy_enabled_; }
  bool quic_enabled() const { return quic_enabled_; }
  bool sdch_enabled() const { return sdch_enabled_; }
  double alternate_protocol_threshold() const {
    return alternate_protocol_threshold_;
  }

  // Clears the network stack's disk cache.
  void ClearCache(ClearCacheCallback callback);

 private:
  // Runs a closure on the network thread.
  void PostToNetworkThread(const tracked_objects::Location& from_here,
                           const base::Closure& task);

  // Configures SDCH on the network thread. If SDCH is enabled, sets up
  // SdchManager, and configures persistence as needed.
  void ConfigureSdchOnNetworkThread();

  // Performs initialization tasks that must happen on the network thread.
  void InitializeOnNetworkThread();

  // Runs a closure on the file user blocking thread.
  void PostToFileUserBlockingThread(const tracked_objects::Location& from_here,
                                    const base::Closure& task);

  // Helper methods that start/stop net-internals logging on the file
  // user blocking thread.
  void StartNetLogInternal(base::FilePath::StringType file_name,
      bool log_bytes);
  void StopNetLogInternal();

  // Returns the HttpNeteworkSession object from the passed in
  // URLRequestContext or NULL if none exists.
  net::HttpNetworkSession* GetHttpNetworkSession(
      net::URLRequestContext* context);

  // Helper method that closes all current SPDY sessions on the network IO
  // thread.
  void CloseAllSpdySessionsInternal();

  bool spdy_enabled_;
  bool quic_enabled_;
  bool sdch_enabled_;
  std::string sdch_pref_store_filename_;
  double alternate_protocol_threshold_;

  static CrNetEnvironment* chrome_net_;
  scoped_ptr<base::Thread> network_io_thread_;
  scoped_ptr<base::Thread> network_cache_thread_;
  scoped_ptr<base::Thread> file_thread_;
  scoped_ptr<base::Thread> file_user_blocking_thread_;
  scoped_ptr<net::SdchManager> sdch_manager_;
  scoped_ptr<net::SdchOwner> sdch_owner_;
  scoped_refptr<base::SequencedTaskRunner> pref_store_worker_pool_;
  scoped_refptr<JsonPrefStore> net_pref_store_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_ptr<net::ProxyConfigService> proxy_config_service_;
  scoped_ptr<net::HttpServerProperties> http_server_properties_;
  scoped_refptr<net::URLRequestContextGetter> main_context_getter_;
  scoped_ptr<net::URLRequestContext> main_context_;
  scoped_ptr<CrNetHttpProtocolHandlerDelegate> http_protocol_handler_delegate_;
  std::string user_agent_product_name_;
  scoped_ptr<net::NetLog> net_log_;
  scoped_ptr<net::WriteToFileNetLogObserver> net_log_observer_;

  DISALLOW_COPY_AND_ASSIGN(CrNetEnvironment);
};

#endif  // IOS_CRNET_CRNET_ENVIRONMENT_H_
