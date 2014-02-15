// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_IMPL_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/local_discovery/privet_http.h"
#include "printing/pdf_render_settings.h"

namespace local_discovery {

class PrivetHTTPClientImpl;

class PrivetInfoOperationImpl : public PrivetJSONOperation,
                                public PrivetURLFetcher::Delegate {
 public:
  PrivetInfoOperationImpl(PrivetHTTPClientImpl* privet_client,
                          const PrivetJSONOperation::ResultCallback& callback);
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
  PrivetJSONOperation::ResultCallback callback_;
  scoped_ptr<PrivetURLFetcher> url_fetcher_;
};

class PrivetRegisterOperationImpl
    : public PrivetRegisterOperation,
      public PrivetURLFetcher::Delegate,
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
  void OnPrivetInfoDone(const base::DictionaryValue* value);


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

  scoped_ptr<PrivetJSONOperation> info_operation_;
  std::string expected_id_;
};

class PrivetJSONOperationImpl : public PrivetJSONOperation,
                                public PrivetURLFetcher::Delegate {
 public:
  PrivetJSONOperationImpl(
      PrivetHTTPClientImpl* privet_client,
      const std::string& path,
      const std::string& query_params,
      const PrivetJSONOperation::ResultCallback& callback);
  virtual ~PrivetJSONOperationImpl();
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
  std::string path_;
  std::string query_params_;
  PrivetJSONOperation::ResultCallback callback_;

  scoped_ptr<PrivetURLFetcher> url_fetcher_;
};

class PrivetDataReadOperationImpl : public PrivetDataReadOperation,
                                    public PrivetURLFetcher::Delegate {
 public:
  PrivetDataReadOperationImpl(
      PrivetHTTPClientImpl* privet_client,
      const std::string& path,
      const std::string& query_params,
      const PrivetDataReadOperation::ResultCallback& callback);
  virtual ~PrivetDataReadOperationImpl();

  virtual void Start() OVERRIDE;

  virtual void SetDataRange(int range_start, int range_end) OVERRIDE;

  virtual void SaveDataToFile() OVERRIDE;

  virtual PrivetHTTPClient* GetHTTPClient() OVERRIDE;

  virtual void OnError(PrivetURLFetcher* fetcher,
                       PrivetURLFetcher::ErrorType error) OVERRIDE;
  virtual void OnParsedJson(PrivetURLFetcher* fetcher,
                            const base::DictionaryValue* value,
                            bool has_error) OVERRIDE;
  virtual void OnNeedPrivetToken(
      PrivetURLFetcher* fetcher,
      const PrivetURLFetcher::TokenCallback& callback) OVERRIDE;
  virtual bool OnRawData(PrivetURLFetcher* fetcher,
                         bool is_file,
                         const std::string& data_str,
                         const base::FilePath& file_path) OVERRIDE;

 private:
  PrivetHTTPClientImpl* privet_client_;
  std::string path_;
  std::string query_params_;
  int range_start_;
  int range_end_;
  PrivetDataReadOperation::ResultCallback callback_;

  bool has_range_;
  bool save_to_file_;

  scoped_ptr<PrivetURLFetcher> url_fetcher_;
};

class PrivetLocalPrintOperationImpl
    : public PrivetLocalPrintOperation,
      public PrivetURLFetcher::Delegate {
 public:
  PrivetLocalPrintOperationImpl(
      PrivetHTTPClientImpl* privet_client,
      PrivetLocalPrintOperation::Delegate* delegate);

  virtual ~PrivetLocalPrintOperationImpl();
  virtual void Start() OVERRIDE;

  virtual void SetData(base::RefCountedBytes* data) OVERRIDE;

  virtual void SetTicket(const std::string& ticket) OVERRIDE;

  virtual void SetUsername(const std::string& user) OVERRIDE;

  virtual void SetJobname(const std::string& jobname) OVERRIDE;

  virtual void SetOffline(bool offline) OVERRIDE;

  virtual void SetPageSize(const gfx::Size& page_size) OVERRIDE;

  virtual void SetPWGRasterConverterForTesting(
      scoped_ptr<PWGRasterConverter> pwg_raster_converter) OVERRIDE;

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
  typedef base::Callback<void(bool, const base::DictionaryValue* value)>
      ResponseCallback;

  void StartInitialRequest();
  void GetCapabilities();
  void DoCreatejob();
  void DoSubmitdoc();

  void StartConvertToPWG();
  void StartPrinting();

  void OnPrivetInfoDone(const base::DictionaryValue* value);
  void OnCapabilitiesResponse(bool has_error,
                              const base::DictionaryValue* value);
  void OnSubmitdocResponse(bool has_error,
                           const base::DictionaryValue* value);
  void OnCreatejobResponse(bool has_error,
                           const base::DictionaryValue* value);
  void OnPWGRasterConverted(bool success, const base::FilePath& pwg_file_path);

  PrivetHTTPClientImpl* privet_client_;
  PrivetLocalPrintOperation::Delegate* delegate_;

  ResponseCallback current_response_;

  std::string ticket_;
  scoped_refptr<base::RefCountedBytes> data_;
  base::FilePath pwg_file_path_;

  bool use_pdf_;
  bool has_capabilities_;
  bool has_extended_workflow_;
  bool started_;
  bool offline_;
  gfx::Size page_size_;
  int dpi_;

  std::string user_;
  std::string jobname_;

  std::string jobid_;

  int invalid_job_retries_;

  scoped_ptr<PrivetURLFetcher> url_fetcher_;
  scoped_ptr<PrivetJSONOperation> info_operation_;
  scoped_ptr<PWGRasterConverter> pwg_raster_converter_;

  base::WeakPtrFactory<PrivetLocalPrintOperationImpl> weak_factory_;
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

  virtual scoped_ptr<PrivetJSONOperation> CreateInfoOperation(
      const PrivetJSONOperation::ResultCallback& callback) OVERRIDE;

  virtual scoped_ptr<PrivetJSONOperation> CreateCapabilitiesOperation(
      const PrivetJSONOperation::ResultCallback& callback) OVERRIDE;

  virtual scoped_ptr<PrivetLocalPrintOperation> CreateLocalPrintOperation(
      PrivetLocalPrintOperation::Delegate* delegate) OVERRIDE;

  virtual scoped_ptr<PrivetJSONOperation> CreateStorageListOperation(
      const std::string& path,
      const PrivetJSONOperation::ResultCallback& callback) OVERRIDE;

  virtual scoped_ptr<PrivetDataReadOperation> CreateStorageReadOperation(
      const std::string& path,
      const PrivetDataReadOperation::ResultCallback& callback) OVERRIDE;

  virtual const std::string& GetName() OVERRIDE;

  scoped_ptr<PrivetURLFetcher> CreateURLFetcher(
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      PrivetURLFetcher::Delegate* delegate) const;

  void CacheInfo(const base::DictionaryValue* cached_info);

  bool HasToken() const;

  void RefreshPrivetToken(
      const PrivetURLFetcher::TokenCallback& token_callback);

 private:
  typedef std::vector<PrivetURLFetcher::TokenCallback> TokenCallbackVector;

  void OnPrivetInfoDone(const base::DictionaryValue* value);

  std::string name_;
  PrivetURLFetcherFactory fetcher_factory_;
  net::HostPortPair host_port_;
  scoped_ptr<base::DictionaryValue> cached_info_;

  scoped_ptr<PrivetJSONOperation> info_operation_;
  TokenCallbackVector token_callbacks_;
};

}  // namespace local_discovery
#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_IMPL_H_
