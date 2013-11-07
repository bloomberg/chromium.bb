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

  virtual void OnNeedPrivetToken(
      PrivetURLFetcher* fetcher,
      const PrivetURLFetcher::TokenCallback& callback) OVERRIDE;

  virtual void OnPrivetInfoDone(PrivetInfoOperation* operation,
                                int http_code,
                                const base::DictionaryValue* value) OVERRIDE;

  virtual PrivetHTTPClient* GetHTTPClient() OVERRIDE;
 private:
  class Cancelation : public PrivetURLFetcher::Delegate {
   public:
    Cancelation(PrivetHTTPClientImpl* privet_client,
                const std::string& user);
    virtual ~Cancelation();

    virtual void OnError(PrivetURLFetcher* fetcher,
                         PrivetURLFetcher::ErrorType error) OVERRIDE;

    virtual void OnParsedJson(PrivetURLFetcher* fetcher,
                              const base::DictionaryValue* value,
                              bool has_error) OVERRIDE;

    void Cleanup();

   private:
    scoped_ptr<PrivetURLFetcher> url_fetcher_;
  };

  // Arguments is JSON value from request.
  typedef base::Callback<void(const base::DictionaryValue&)>
      ResponseHandler;

  void StartInfoOperation();
  void StartResponse(const base::DictionaryValue& value);
  void GetClaimTokenResponse(const base::DictionaryValue& value);
  void CompleteResponse(const base::DictionaryValue& value);

  void SendRequest(const std::string& action);

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
  std::string expected_id_;
};

// TODO(noamsml): Factor out some of this code into a PrivetBaseOperation
class PrivetCapabilitiesOperationImpl : public PrivetCapabilitiesOperation,
                                        public PrivetURLFetcher::Delegate {
 public:
  PrivetCapabilitiesOperationImpl(
      PrivetHTTPClientImpl* privet_client,
      PrivetCapabilitiesOperation::Delegate* delegate);
  virtual ~PrivetCapabilitiesOperationImpl();
  virtual void Start() OVERRIDE;

  virtual PrivetHTTPClient* GetHTTPClient() OVERRIDE;

  virtual void OnError(PrivetURLFetcher* fetcher,
                       PrivetURLFetcher::ErrorType error) OVERRIDE;
  virtual void OnParsedJson(PrivetURLFetcher* fetcher,
                            const base::DictionaryValue* value,
                            bool has_error) OVERRIDE;
  virtual void OnNeedPrivetToken(
      PrivetURLFetcher* fetcher,
      const PrivetURLFetcher::TokenCallback& callback) OVERRIDE;

 private:
  PrivetHTTPClientImpl* privet_client_;
  PrivetCapabilitiesOperation::Delegate* delegate_;

  scoped_ptr<PrivetURLFetcher> url_fetcher_;
  scoped_ptr<PrivetInfoOperation> info_operation_;
};

class PrivetLocalPrintOperationImpl
    : public PrivetLocalPrintOperation,
      public PrivetURLFetcher::Delegate,
      public PrivetInfoOperation::Delegate {
 public:
  PrivetLocalPrintOperationImpl(
      PrivetHTTPClientImpl* privet_client,
      PrivetLocalPrintOperation::Delegate* delegate);

  virtual ~PrivetLocalPrintOperationImpl();
  virtual void Start() OVERRIDE;

  virtual void SendData(const std::string& data) OVERRIDE;

  virtual void SetTicket(const std::string& ticket) OVERRIDE;

  virtual void SetUsername(const std::string& user) OVERRIDE;

  virtual void SetJobname(const std::string& jobname) OVERRIDE;

  virtual  void SetOffline(bool offline) OVERRIDE;

  virtual PrivetHTTPClient* GetHTTPClient() OVERRIDE;

  virtual void OnError(PrivetURLFetcher* fetcher,
                       PrivetURLFetcher::ErrorType error) OVERRIDE;
  virtual void OnParsedJson(PrivetURLFetcher* fetcher,
                            const base::DictionaryValue* value,
                            bool has_error) OVERRIDE;
  virtual void OnNeedPrivetToken(
      PrivetURLFetcher* fetcher,
      const PrivetURLFetcher::TokenCallback& callback) OVERRIDE;

  virtual void OnPrivetInfoDone(PrivetInfoOperation* operation,
                                int http_code,
                                const base::DictionaryValue* value) OVERRIDE;
 private:
  typedef base::Callback<void(const base::DictionaryValue* value)>
      ResponseCallback;

  void StartInitialRequest();
  void GetCapabilities();
  void DoCreatejob();
  void DoSubmitdoc();

  void OnCapabilitiesResponse(const base::DictionaryValue* value);
  void OnSubmitdocResponse(const base::DictionaryValue* value);
  void OnCreatejobResponse(const base::DictionaryValue* value);

  PrivetHTTPClientImpl* privet_client_;
  PrivetLocalPrintOperation::Delegate* delegate_;

  ResponseCallback current_response_;

  std::string ticket_;
  std::string data_;

  bool use_pdf_;
  bool has_capabilities_;
  bool has_extended_workflow_;
  bool started_;
  bool offline_;

  std::string user_;
  std::string jobname_;

  std::string jobid_;

  scoped_ptr<PrivetURLFetcher> url_fetcher_;
  scoped_ptr<PrivetInfoOperation> info_operation_;
};

class PrivetHTTPClientImpl : public PrivetHTTPClient,
                             public PrivetInfoOperation::Delegate {
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

  virtual scoped_ptr<PrivetCapabilitiesOperation> CreateCapabilitiesOperation(
      PrivetCapabilitiesOperation::Delegate* delegate) OVERRIDE;

  virtual scoped_ptr<PrivetLocalPrintOperation> CreateLocalPrintOperation(
      PrivetLocalPrintOperation::Delegate* delegate) OVERRIDE;

  virtual const std::string& GetName() OVERRIDE;

  scoped_ptr<PrivetURLFetcher> CreateURLFetcher(
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      PrivetURLFetcher::Delegate* delegate) const;

  void CacheInfo(const base::DictionaryValue* cached_info);

  bool HasToken() const;

  void RefreshPrivetToken(
      const PrivetURLFetcher::TokenCallback& token_callback);

  virtual void OnPrivetInfoDone(PrivetInfoOperation* operation,
                                int http_code,
                                const base::DictionaryValue* value) OVERRIDE;

 private:
  typedef std::vector<PrivetURLFetcher::TokenCallback> TokenCallbackVector;
  std::string name_;
  PrivetURLFetcherFactory fetcher_factory_;
  net::HostPortPair host_port_;
  scoped_ptr<base::DictionaryValue> cached_info_;

  scoped_ptr<PrivetInfoOperation> info_operation_;
  TokenCallbackVector token_callbacks_;
};

}  // namespace local_discovery
#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_IMPL_H_
