// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* DO NOT EDIT. Generated from components/cronet/native/generated/cronet.idl */

#ifndef COMPONENTS_CRONET_NATIVE_GENERATED_CRONET_IDL_IMPL_STRUCT_H_
#define COMPONENTS_CRONET_NATIVE_GENERATED_CRONET_IDL_IMPL_STRUCT_H_

#include "components/cronet/native/generated/cronet.idl_c.h"

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"

// Struct Cronet_Exception.
struct Cronet_Exception {
 public:
  Cronet_Exception();
  ~Cronet_Exception();

  Cronet_Exception_ERROR_CODE error_code =
      Cronet_Exception_ERROR_CODE_ERROR_CALLBACK;
  int32_t internal_error_code = 0;
  bool immediately_retryable = false;
  int32_t quic_detailed_error_code = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_Exception);
};

// Struct Cronet_QuicHint.
struct Cronet_QuicHint {
 public:
  Cronet_QuicHint();
  ~Cronet_QuicHint();

  std::string host;
  int32_t port = 0;
  int32_t alternatePort = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_QuicHint);
};

// Struct Cronet_PublicKeyPins.
struct Cronet_PublicKeyPins {
 public:
  Cronet_PublicKeyPins();
  ~Cronet_PublicKeyPins();

  std::string host;
  std::vector<RawDataPtr> pinsSha256;
  bool includeSubdomains = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_PublicKeyPins);
};

// Struct Cronet_EngineParams.
struct Cronet_EngineParams {
 public:
  Cronet_EngineParams();
  ~Cronet_EngineParams();

  std::string userAgent;
  std::string storagePath;
  bool enableQuic = false;
  bool enableHttp2 = true;
  bool enableBrotli = true;
  Cronet_EngineParams_HTTP_CACHE_MODE httpCacheMode =
      Cronet_EngineParams_HTTP_CACHE_MODE_DISABLED;
  int64_t httpCacheMaxSize = 0;
  std::vector<std::unique_ptr<Cronet_QuicHint>> quicHints;
  std::vector<std::unique_ptr<Cronet_PublicKeyPins>> publicKeyPins;
  bool enablePublicKeyPinningBypassForLocalTrustAnchors = true;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_EngineParams);
};

// Struct Cronet_HttpHeader.
struct Cronet_HttpHeader {
 public:
  Cronet_HttpHeader();
  ~Cronet_HttpHeader();

  std::string name;
  std::string value;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_HttpHeader);
};

// Struct Cronet_UrlResponseInfo.
struct Cronet_UrlResponseInfo {
 public:
  Cronet_UrlResponseInfo();
  ~Cronet_UrlResponseInfo();

  std::string url;
  std::vector<std::string> urlChain;
  int32_t httpStatusCode;
  std::string httpStatusText;
  std::vector<std::unique_ptr<Cronet_HttpHeader>> allHeadersList;
  bool wasCached;
  std::string negotiatedProtocol;
  std::string proxyServer;
  int64_t receivedByteCount;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_UrlResponseInfo);
};

// Struct Cronet_UrlRequestParams.
struct Cronet_UrlRequestParams {
 public:
  Cronet_UrlRequestParams();
  ~Cronet_UrlRequestParams();

  std::string httpMethod;
  std::vector<std::unique_ptr<Cronet_HttpHeader>> requestHeaders;
  bool disableCache = false;
  Cronet_UrlRequestParams_REQUEST_PRIORITY priority =
      Cronet_UrlRequestParams_REQUEST_PRIORITY_REQUEST_PRIORITY_MEDIUM;
  Cronet_UploadDataProviderPtr uploadDataProvider;
  Cronet_ExecutorPtr uploadDataProviderExecutor;
  bool allowDirectExecutor = false;
  std::vector<RawDataPtr> annotations;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_UrlRequestParams);
};

#endif  // COMPONENTS_CRONET_NATIVE_GENERATED_CRONET_IDL_IMPL_STRUCT_H_
