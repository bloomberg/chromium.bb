// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_AUTH_REQUEST_HANDLER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_AUTH_REQUEST_HANDLER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace net {
class HttpRequestHeaders;
class HttpResponseHeaders;
class ProxyServer;
class URLRequest;
}

namespace data_reduction_proxy {

#if defined(OS_ANDROID)
extern const char kAndroidWebViewProtocolVersion[];
#endif

extern const char kClientAndroidWebview[];
extern const char kClientChromeAndroid[];
extern const char kClientChromeIOS[];

class DataReductionProxyParams;

class DataReductionProxyAuthRequestHandler {
 public:
  static bool IsKeySetOnCommandLine();

  // Constructs an authentication request handler. Client is the canonical name
  // for the client. Client names should be defined in this file as one of
  // |kClient...|. Version is the authentication protocol version that the
  // client uses, which should be |kProtocolVersion| unless the client expects
  // to be handled differently from the standard behavior.
  DataReductionProxyAuthRequestHandler(
      const std::string& client,
      const std::string& version,
      DataReductionProxyParams* params);

  virtual ~DataReductionProxyAuthRequestHandler();

  // Adds a 'Chrome-Proxy' header to |request_headers| with the data reduction
  // proxy authentication credentials. Only adds this header if the provided
  // |proxy_server| is a data reduction proxy.
  void MaybeAddRequestHeader(net::URLRequest* request,
                             const net::ProxyServer& proxy_server,
                             net::HttpRequestHeaders* request_headers);

  // Sets a new authentication key. This must be called for platforms that do
  // not have a default key defined. See the constructor implementation for
  // those platforms.
  void SetKey(const std::string& key);

 protected:
  void Init();
  void InitAuthentication(const std::string& key);

  void AddAuthorizationHeader(net::HttpRequestHeaders* headers);

  // Returns a UTF16 string that's the hash of the configured authentication
  // |key| and |salt|. Returns an empty UTF16 string if no key is configured or
  // the data reduction proxy feature isn't available.
  static base::string16 AuthHashForSalt(int64 salt,
                                        const std::string& key);
  // Visible for testing.
  virtual base::Time Now() const;
  virtual void RandBytes(void* output, size_t length);

  // Visible for testing.
  virtual std::string GetDefaultKey() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyAuthRequestHandlerTest,
                           Authorization);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyAuthRequestHandlerTest,
                           AuthHashForSalt);

  // Authentication state.
  std::string key_;
  std::string session_;
  std::string credentials_;

  // Name of the client and version of the data reduction proxy protocol to use.
  std::string client_;
  std::string version_;

  DataReductionProxyParams* data_reduction_proxy_params_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyAuthRequestHandler);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_AUTH_REQUEST_HANDLER_H_
