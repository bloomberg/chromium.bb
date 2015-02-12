// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_AUTH_REQUEST_HANDLER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_AUTH_REQUEST_HANDLER_H_

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/time/time.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class HostPortPair;
class HttpRequestHeaders;
class HttpResponseHeaders;
class ProxyServer;
class URLRequest;
}

namespace data_reduction_proxy {

extern const char kSessionHeaderOption[];
extern const char kCredentialsHeaderOption[];
extern const char kBuildNumberHeaderOption[];
extern const char kPatchNumberHeaderOption[];
extern const char kClientHeaderOption[];
extern const char kLoFiHeaderOption[];

#if defined(OS_ANDROID)
extern const char kAndroidWebViewProtocolVersion[];
#endif

#define CLIENT_ENUMS_LIST \
    CLIENT_ENUM(UNKNOWN, "") \
    CLIENT_ENUM(WEBVIEW_ANDROID, "webview") \
    CLIENT_ENUM(CHROME_ANDROID, "android") \
    CLIENT_ENUM(CHROME_IOS, "ios") \
    CLIENT_ENUM(CHROME_MAC, "mac") \
    CLIENT_ENUM(CHROME_CHROMEOS, "chromeos") \
    CLIENT_ENUM(CHROME_LINUX, "linux") \
    CLIENT_ENUM(CHROME_WINDOWS, "win") \
    CLIENT_ENUM(CHROME_FREEBSD, "freebsd") \
    CLIENT_ENUM(CHROME_OPENBSD, "openbsd") \
    CLIENT_ENUM(CHROME_SOLARIS, "solaris") \
    CLIENT_ENUM(CHROME_QNX, "qnx")

#define CLIENT_ENUM(name, str_value) name,
typedef enum {
  CLIENT_ENUMS_LIST
} Client;
#undef CLIENT_ENUM

class DataReductionProxyParams;

class DataReductionProxyRequestOptions {
 public:
  static bool IsKeySetOnCommandLine();

  // Constructs a DataReductionProxyRequestOptions object with the given
  // client type, params, and network task runner.
  DataReductionProxyRequestOptions(
      Client client,
      DataReductionProxyParams* params,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner);

  virtual ~DataReductionProxyRequestOptions();

  // Sets |key_| to the default key and initializes the credentials, version,
  // client, and lo-fi header values in |header_options_|. Generates the
  // |header_value_| string which is concatenated to the Chrome-proxy header.
  void Init();

  // Adds a 'Chrome-Proxy' header to |request_headers| with the data reduction
  // proxy authentication credentials. Only adds this header if the provided
  // |proxy_server| is a data reduction proxy and not the data reduction proxy's
  // CONNECT server. Must be called on the IO thread.
  void MaybeAddRequestHeader(net::URLRequest* request,
                             const net::ProxyServer& proxy_server,
                             net::HttpRequestHeaders* request_headers);

  // Adds a 'Chrome-Proxy' header to |request_headers| with the data reduction
  // proxy authentication credentials. Only adds this header if the provided
  // |proxy_server| is the data reduction proxy's CONNECT server. Must be called
  // on the IO thread.
  void MaybeAddProxyTunnelRequestHandler(
      const net::HostPortPair& proxy_server,
      net::HttpRequestHeaders* request_headers);

  // Stores the supplied key and sets up credentials suitable for authenticating
  // with the data reduction proxy.
  // This can be called more than once. For example on a platform that does not
  // have a default key defined, this function will be called some time after
  // this class has been constructed. Android WebView is a platform that does
  // this. The caller needs to make sure |this| pointer is valid when
  // SetKeyOnIO is called.
  void SetKeyOnIO(const std::string& key);

 protected:
  void SetHeader(net::HttpRequestHeaders* headers);

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

  // Visible for testing.
  DataReductionProxyRequestOptions(
      Client client,
      const std::string& version,
      DataReductionProxyParams* params,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner);

 private:
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyRequestOptionsTest,
                           AuthorizationOnIOThread);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyRequestOptionsTest,
                           AuthorizationIgnoresEmptyKey);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyRequestOptionsTest,
                           AuthorizationBogusVersion);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyRequestOptionsTest,
                           AuthHashForSalt);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyRequestOptionsTest,
                           AuthorizationLoFi);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyRequestOptionsTest,
                           AuthorizationLoFiOffThenOn);

  // Returns the version of Chromium that is being used.
  std::string ChromiumVersion() const;

  // Returns the build and patch numbers of |version|. If |version| isn't of the
  // form xx.xx.xx.xx build and patch are not modified.
  void GetChromiumBuildAndPatch(const std::string& version,
                                std::string* build,
                                std::string* patch) const;

  // Uses |version_| and |client_| to update values in |header_options_|.
  void UpdateVersion();

  // Updates the value of LoFi in |header_options_| and regenerates the header
  // if necessary.
  void UpdateLoFi();

  // Generates a session ID and credentials suitable for authenticating with
  // the data reduction proxy.
  void ComputeCredentials(const base::Time& now,
                          std::string* session,
                          std::string* credentials);

  // Generates and updates the session ID and credentials in |header_options_|.
  void UpdateCredentials();

  // Adds authentication headers only if |expects_ssl| is true and
  // |proxy_server| is a data reduction proxy used for ssl tunneling via
  // HTTP CONNECT, or |expect_ssl| is false and |proxy_server| is a data
  // reduction proxy for HTTP traffic.
  void MaybeAddRequestHeaderImpl(const net::HostPortPair& proxy_server,
                                 bool expect_ssl,
                                 net::HttpRequestHeaders* request_headers);

  // Regenerates the |header_value_| string which is concatenated to the
  // Chrome-proxy header.
  void RegenerateRequestHeaderValue();

  // Map and string of the request options to be added to the header.
  std::map<std::string, std::string> header_options_;
  std::string header_value_;

  // Authentication state.
  std::string key_;

  // Name of the client and version of the data reduction proxy protocol to use.
  std::string client_;
  std::string version_;

  // The last time the session was updated. Used to ensure that a session is
  // never used for more than twenty-four hours.
  base::Time last_credentials_update_time_;

  DataReductionProxyParams* data_reduction_proxy_params_;

  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyRequestOptions);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_AUTH_REQUEST_HANDLER_H_
