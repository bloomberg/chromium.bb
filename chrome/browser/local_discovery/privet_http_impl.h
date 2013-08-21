// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_IMPL_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/local_discovery/privet_http.h"

namespace local_discovery {

class PrivetHTTPClientImpl;

class PrivetInfoOperationImpl : public PrivetInfoOperation,
                                public PrivetURLFetcher::Delegate {
 public:
  PrivetInfoOperationImpl(PrivetHTTPClientImpl* privet_client,
                          PrivetInfoOperation::Delegate* delegate);
  virtual ~PrivetInfoOperationImpl();

  virtual void Start() OVERRIDE;

  virtual PrivetHTTPClient* GetHTTPClient() OVERRIDE;

  virtual void OnError(PrivetURLFetcher* fetcher,
                       PrivetURLFetcher::ErrorType error) OVERRIDE;
  virtual void OnParsedJson(PrivetURLFetcher* fetcher,
                            const base::DictionaryValue* value,
                            bool has_error) OVERRIDE;

 private:
  PrivetHTTPClientImpl* privet_client_;
  PrivetInfoOperation::Delegate* delegate_;
  scoped_ptr<PrivetURLFetcher> url_fetcher_;
};

class PrivetRegisterOperationImpl
    : public PrivetRegisterOperation,
      public PrivetURLFetcher::Delegate,
      public PrivetInfoOperation::Delegate,
      public base::SupportsWeakPtr<PrivetRegisterOperationImpl> {
 public:
  PrivetRegisterOperationImpl(PrivetHTTPClientImpl* privet_client,
                              const std::string& user,
                              PrivetRegisterOperation::Delegate* delegate);
  virtual ~PrivetRegisterOperationImpl();

  virtual void Start() OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual void CompleteRegistration() OVERRIDE;

  virtual void OnError(PrivetURLFetcher* fetcher,
                       PrivetURLFetcher::ErrorType error) OVERRIDE;

  virtual void OnParsedJson(PrivetURLFetcher* fetcher,
                            const base::DictionaryValue* value,
                            bool has_error) OVERRIDE;

  virtual void OnPrivetInfoDone(PrivetInfoOperation* operation,
                                int http_code,
                                const base::DictionaryValue* value) OVERRIDE;

  virtual PrivetHTTPClient* GetHTTPClient() OVERRIDE;
 private:
  // Arguments is JSON value from request.
  typedef base::Callback<void(const base::DictionaryValue&)>
      ResponseHandler;

  void StartResponse(const base::DictionaryValue& value);
  void GetClaimTokenResponse(const base::DictionaryValue& value);
  void CompleteResponse(const base::DictionaryValue& value);

  void StartInfoOperation();

  void SendRequest(const std::string& action);

  bool PrivetErrorTransient(const std::string& error);

  std::string user_;
  std::string current_action_;
  scoped_ptr<PrivetURLFetcher> url_fetcher_;
  PrivetRegisterOperation::Delegate* delegate_;
  PrivetHTTPClientImpl* privet_client_;
  ResponseHandler next_response_handler_;
  // Required to ensure destroying completed register operations doesn't cause
  // extraneous cancelations.
  bool ongoing_;
  scoped_ptr<PrivetInfoOperation> info_operation_;
};

class PrivetHTTPClientImpl : public PrivetHTTPClient {
 public:
  PrivetHTTPClientImpl(
      const std::string& name,
      const net::HostPortPair& host_port,
      net::URLRequestContextGetter* request_context);
  virtual ~PrivetHTTPClientImpl();

  virtual const base::DictionaryValue* GetCachedInfo() const OVERRIDE;

  virtual scoped_ptr<PrivetRegisterOperation> CreateRegisterOperation(
      const std::string& user,
      PrivetRegisterOperation::Delegate* delegate) OVERRIDE;

  virtual scoped_ptr<PrivetInfoOperation> CreateInfoOperation(
      PrivetInfoOperation::Delegate* delegate) OVERRIDE;

  virtual const std::string& GetName() OVERRIDE;

  const PrivetURLFetcherFactory& fetcher_factory() const {
    return fetcher_factory_;
  }
  const net::HostPortPair& host_port() const { return host_port_; }

  void CacheInfo(const base::DictionaryValue* cached_info);

 private:
  std::string name_;
  PrivetURLFetcherFactory fetcher_factory_;
  net::HostPortPair host_port_;
  scoped_ptr<base::DictionaryValue> cached_info_;
};

}  // namespace local_discovery
#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_IMPL_H_
