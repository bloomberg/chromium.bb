// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_MAC_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_MAC_H_

#include "chrome/browser/local_discovery/privet_http.h"
#include "chrome/browser/local_discovery/privet_http_asynchronous_factory.h"

namespace local_discovery {

class PrivetHTTPAsynchronousFactoryMac : public PrivetHTTPAsynchronousFactory {
 public:
  explicit PrivetHTTPAsynchronousFactoryMac(
      net::URLRequestContextGetter* request_context);
  virtual ~PrivetHTTPAsynchronousFactoryMac();

  virtual scoped_ptr<PrivetHTTPResolution> CreatePrivetHTTP(
      const std::string& name,
      const net::HostPortPair& address,
      const ResultCallback& callback) OVERRIDE;

 private:
  class ResolutionMac : public PrivetHTTPResolution {
   public:
    ResolutionMac(net::URLRequestContextGetter* request_context,
                  const std::string& name,
                  const net::HostPortPair& host_port,
                  const ResultCallback& callback);
    virtual ~ResolutionMac();

    virtual void Start() OVERRIDE;
    virtual const std::string& GetName() OVERRIDE;

   private:
    net::URLRequestContextGetter* request_context_;
    std::string name_;
    net::HostPortPair host_port_;
    ResultCallback callback_;
  };

  net::URLRequestContextGetter* request_context_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_MAC_H_
