// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_URL_REQUEST_CONTEXT_PEER_H_
#define COMPONENTS_CRONET_ANDROID_URL_REQUEST_CONTEXT_PEER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "net/base/net_log.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace net {
class NetLogLogger;
}  // namespace net

namespace cronet {

struct URLRequestContextConfig;

// Implementation of the Chromium NetLog observer interface.
class NetLogObserver : public net::NetLog::ThreadSafeObserver {
 public:
  explicit NetLogObserver() {}

  virtual ~NetLogObserver() {}

  virtual void OnAddEntry(const net::NetLog::Entry& entry) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetLogObserver);
};

// Fully configured |URLRequestContext|.
class URLRequestContextPeer : public net::URLRequestContextGetter {
 public:
  class URLRequestContextPeerDelegate
      : public base::RefCountedThreadSafe<URLRequestContextPeerDelegate> {
   public:
    virtual void OnContextInitialized(URLRequestContextPeer* context) = 0;

   protected:
    friend class base::RefCountedThreadSafe<URLRequestContextPeerDelegate>;

    virtual ~URLRequestContextPeerDelegate() {}
  };

  URLRequestContextPeer(URLRequestContextPeerDelegate* delegate,
                        std::string user_agent);
  void Initialize(scoped_ptr<URLRequestContextConfig> config);

  const std::string& GetUserAgent(const GURL& url) const;

  // net::URLRequestContextGetter implementation:
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const OVERRIDE;

  void StartNetLogToFile(const std::string& file_name);
  void StopNetLog();

 private:
  scoped_refptr<URLRequestContextPeerDelegate> delegate_;
  scoped_ptr<net::URLRequestContext> context_;
  std::string user_agent_;
  base::Thread* network_thread_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_ptr<NetLogObserver> net_log_observer_;
  scoped_ptr<net::NetLogLogger> net_log_logger_;

  virtual ~URLRequestContextPeer();

  // Initializes |context_| on the IO thread.
  void InitializeURLRequestContext(scoped_ptr<URLRequestContextConfig> config);

  DISALLOW_COPY_AND_ASSIGN(URLRequestContextPeer);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_URL_REQUEST_CONTEXT_PEER_H_
