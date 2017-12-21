// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* DO NOT EDIT. Generated from components/cronet/native/generated/cronet.idl */

#include "components/cronet/native/generated/cronet.idl_impl_struct.h"

#include <utility>

#include "base/logging.h"

// Struct Cronet_Exception.
Cronet_Exception::Cronet_Exception() {}

Cronet_Exception::~Cronet_Exception() {}

Cronet_ExceptionPtr Cronet_Exception_Create() {
  return new Cronet_Exception();
}

void Cronet_Exception_Destroy(Cronet_ExceptionPtr self) {
  delete self;
}

// Struct Cronet_Exception setters.
void Cronet_Exception_set_error_code(Cronet_ExceptionPtr self,
                                     Cronet_Exception_ERROR_CODE error_code) {
  DCHECK(self);
  self->error_code = error_code;
}

void Cronet_Exception_set_internal_error_code(Cronet_ExceptionPtr self,
                                              int32_t internal_error_code) {
  DCHECK(self);
  self->internal_error_code = internal_error_code;
}

void Cronet_Exception_set_immediately_retryable(Cronet_ExceptionPtr self,
                                                bool immediately_retryable) {
  DCHECK(self);
  self->immediately_retryable = immediately_retryable;
}

void Cronet_Exception_set_quic_detailed_error_code(
    Cronet_ExceptionPtr self,
    int32_t quic_detailed_error_code) {
  DCHECK(self);
  self->quic_detailed_error_code = quic_detailed_error_code;
}

// Struct Cronet_Exception getters.
Cronet_Exception_ERROR_CODE Cronet_Exception_get_error_code(
    Cronet_ExceptionPtr self) {
  DCHECK(self);
  return self->error_code;
}

int32_t Cronet_Exception_get_internal_error_code(Cronet_ExceptionPtr self) {
  DCHECK(self);
  return self->internal_error_code;
}

bool Cronet_Exception_get_immediately_retryable(Cronet_ExceptionPtr self) {
  DCHECK(self);
  return self->immediately_retryable;
}

int32_t Cronet_Exception_get_quic_detailed_error_code(
    Cronet_ExceptionPtr self) {
  DCHECK(self);
  return self->quic_detailed_error_code;
}

// Struct Cronet_QuicHint.
Cronet_QuicHint::Cronet_QuicHint() {}

Cronet_QuicHint::~Cronet_QuicHint() {}

Cronet_QuicHintPtr Cronet_QuicHint_Create() {
  return new Cronet_QuicHint();
}

void Cronet_QuicHint_Destroy(Cronet_QuicHintPtr self) {
  delete self;
}

// Struct Cronet_QuicHint setters.
void Cronet_QuicHint_set_host(Cronet_QuicHintPtr self, CharString host) {
  DCHECK(self);
  self->host = host;
}

void Cronet_QuicHint_set_port(Cronet_QuicHintPtr self, int32_t port) {
  DCHECK(self);
  self->port = port;
}

void Cronet_QuicHint_set_alternatePort(Cronet_QuicHintPtr self,
                                       int32_t alternatePort) {
  DCHECK(self);
  self->alternatePort = alternatePort;
}

// Struct Cronet_QuicHint getters.
CharString Cronet_QuicHint_get_host(Cronet_QuicHintPtr self) {
  DCHECK(self);
  return self->host.c_str();
}

int32_t Cronet_QuicHint_get_port(Cronet_QuicHintPtr self) {
  DCHECK(self);
  return self->port;
}

int32_t Cronet_QuicHint_get_alternatePort(Cronet_QuicHintPtr self) {
  DCHECK(self);
  return self->alternatePort;
}

// Struct Cronet_PublicKeyPins.
Cronet_PublicKeyPins::Cronet_PublicKeyPins() {}

Cronet_PublicKeyPins::~Cronet_PublicKeyPins() {}

Cronet_PublicKeyPinsPtr Cronet_PublicKeyPins_Create() {
  return new Cronet_PublicKeyPins();
}

void Cronet_PublicKeyPins_Destroy(Cronet_PublicKeyPinsPtr self) {
  delete self;
}

// Struct Cronet_PublicKeyPins setters.
void Cronet_PublicKeyPins_set_host(Cronet_PublicKeyPinsPtr self,
                                   CharString host) {
  DCHECK(self);
  self->host = host;
}

void Cronet_PublicKeyPins_add_pinsSha256(Cronet_PublicKeyPinsPtr self,
                                         RawDataPtr pinsSha256) {
  DCHECK(self);
  self->pinsSha256.push_back(pinsSha256);
}

void Cronet_PublicKeyPins_set_includeSubdomains(Cronet_PublicKeyPinsPtr self,
                                                bool includeSubdomains) {
  DCHECK(self);
  self->includeSubdomains = includeSubdomains;
}

// Struct Cronet_PublicKeyPins getters.
CharString Cronet_PublicKeyPins_get_host(Cronet_PublicKeyPinsPtr self) {
  DCHECK(self);
  return self->host.c_str();
}

uint32_t Cronet_PublicKeyPins_get_pinsSha256Size(Cronet_PublicKeyPinsPtr self) {
  DCHECK(self);
  return self->pinsSha256.size();
}
RawDataPtr Cronet_PublicKeyPins_get_pinsSha256AtIndex(
    Cronet_PublicKeyPinsPtr self,
    uint32_t index) {
  DCHECK(self);
  DCHECK(index < self->pinsSha256.size());
  return self->pinsSha256[index];
}

bool Cronet_PublicKeyPins_get_includeSubdomains(Cronet_PublicKeyPinsPtr self) {
  DCHECK(self);
  return self->includeSubdomains;
}

// Struct Cronet_EngineParams.
Cronet_EngineParams::Cronet_EngineParams() {}

Cronet_EngineParams::~Cronet_EngineParams() {}

Cronet_EngineParamsPtr Cronet_EngineParams_Create() {
  return new Cronet_EngineParams();
}

void Cronet_EngineParams_Destroy(Cronet_EngineParamsPtr self) {
  delete self;
}

// Struct Cronet_EngineParams setters.
void Cronet_EngineParams_set_userAgent(Cronet_EngineParamsPtr self,
                                       CharString userAgent) {
  DCHECK(self);
  self->userAgent = userAgent;
}

void Cronet_EngineParams_set_storagePath(Cronet_EngineParamsPtr self,
                                         CharString storagePath) {
  DCHECK(self);
  self->storagePath = storagePath;
}

void Cronet_EngineParams_set_enableQuic(Cronet_EngineParamsPtr self,
                                        bool enableQuic) {
  DCHECK(self);
  self->enableQuic = enableQuic;
}

void Cronet_EngineParams_set_enableHttp2(Cronet_EngineParamsPtr self,
                                         bool enableHttp2) {
  DCHECK(self);
  self->enableHttp2 = enableHttp2;
}

void Cronet_EngineParams_set_enableBrotli(Cronet_EngineParamsPtr self,
                                          bool enableBrotli) {
  DCHECK(self);
  self->enableBrotli = enableBrotli;
}

void Cronet_EngineParams_set_httpCacheMode(
    Cronet_EngineParamsPtr self,
    Cronet_EngineParams_HTTP_CACHE_MODE httpCacheMode) {
  DCHECK(self);
  self->httpCacheMode = httpCacheMode;
}

void Cronet_EngineParams_set_httpCacheMaxSize(Cronet_EngineParamsPtr self,
                                              int64_t httpCacheMaxSize) {
  DCHECK(self);
  self->httpCacheMaxSize = httpCacheMaxSize;
}

void Cronet_EngineParams_add_quicHints(Cronet_EngineParamsPtr self,
                                       Cronet_QuicHintPtr quicHints) {
  DCHECK(self);
  std::unique_ptr<Cronet_QuicHint> tmp_ptr(quicHints);
  self->quicHints.push_back(std::move(tmp_ptr));
}

void Cronet_EngineParams_add_publicKeyPins(
    Cronet_EngineParamsPtr self,
    Cronet_PublicKeyPinsPtr publicKeyPins) {
  DCHECK(self);
  std::unique_ptr<Cronet_PublicKeyPins> tmp_ptr(publicKeyPins);
  self->publicKeyPins.push_back(std::move(tmp_ptr));
}

void Cronet_EngineParams_set_enablePublicKeyPinningBypassForLocalTrustAnchors(
    Cronet_EngineParamsPtr self,
    bool enablePublicKeyPinningBypassForLocalTrustAnchors) {
  DCHECK(self);
  self->enablePublicKeyPinningBypassForLocalTrustAnchors =
      enablePublicKeyPinningBypassForLocalTrustAnchors;
}

// Struct Cronet_EngineParams getters.
CharString Cronet_EngineParams_get_userAgent(Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->userAgent.c_str();
}

CharString Cronet_EngineParams_get_storagePath(Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->storagePath.c_str();
}

bool Cronet_EngineParams_get_enableQuic(Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->enableQuic;
}

bool Cronet_EngineParams_get_enableHttp2(Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->enableHttp2;
}

bool Cronet_EngineParams_get_enableBrotli(Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->enableBrotli;
}

Cronet_EngineParams_HTTP_CACHE_MODE Cronet_EngineParams_get_httpCacheMode(
    Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->httpCacheMode;
}

int64_t Cronet_EngineParams_get_httpCacheMaxSize(Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->httpCacheMaxSize;
}

uint32_t Cronet_EngineParams_get_quicHintsSize(Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->quicHints.size();
}
Cronet_QuicHintPtr Cronet_EngineParams_get_quicHintsAtIndex(
    Cronet_EngineParamsPtr self,
    uint32_t index) {
  DCHECK(self);
  DCHECK(index < self->quicHints.size());
  return self->quicHints[index].get();
}

uint32_t Cronet_EngineParams_get_publicKeyPinsSize(
    Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->publicKeyPins.size();
}
Cronet_PublicKeyPinsPtr Cronet_EngineParams_get_publicKeyPinsAtIndex(
    Cronet_EngineParamsPtr self,
    uint32_t index) {
  DCHECK(self);
  DCHECK(index < self->publicKeyPins.size());
  return self->publicKeyPins[index].get();
}

bool Cronet_EngineParams_get_enablePublicKeyPinningBypassForLocalTrustAnchors(
    Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->enablePublicKeyPinningBypassForLocalTrustAnchors;
}

// Struct Cronet_HttpHeader.
Cronet_HttpHeader::Cronet_HttpHeader() {}

Cronet_HttpHeader::~Cronet_HttpHeader() {}

Cronet_HttpHeaderPtr Cronet_HttpHeader_Create() {
  return new Cronet_HttpHeader();
}

void Cronet_HttpHeader_Destroy(Cronet_HttpHeaderPtr self) {
  delete self;
}

// Struct Cronet_HttpHeader setters.
void Cronet_HttpHeader_set_name(Cronet_HttpHeaderPtr self, CharString name) {
  DCHECK(self);
  self->name = name;
}

void Cronet_HttpHeader_set_value(Cronet_HttpHeaderPtr self, CharString value) {
  DCHECK(self);
  self->value = value;
}

// Struct Cronet_HttpHeader getters.
CharString Cronet_HttpHeader_get_name(Cronet_HttpHeaderPtr self) {
  DCHECK(self);
  return self->name.c_str();
}

CharString Cronet_HttpHeader_get_value(Cronet_HttpHeaderPtr self) {
  DCHECK(self);
  return self->value.c_str();
}

// Struct Cronet_UrlResponseInfo.
Cronet_UrlResponseInfo::Cronet_UrlResponseInfo() {}

Cronet_UrlResponseInfo::~Cronet_UrlResponseInfo() {}

Cronet_UrlResponseInfoPtr Cronet_UrlResponseInfo_Create() {
  return new Cronet_UrlResponseInfo();
}

void Cronet_UrlResponseInfo_Destroy(Cronet_UrlResponseInfoPtr self) {
  delete self;
}

// Struct Cronet_UrlResponseInfo setters.
void Cronet_UrlResponseInfo_set_url(Cronet_UrlResponseInfoPtr self,
                                    CharString url) {
  DCHECK(self);
  self->url = url;
}

void Cronet_UrlResponseInfo_add_urlChain(Cronet_UrlResponseInfoPtr self,
                                         CharString urlChain) {
  DCHECK(self);
  self->urlChain.push_back(urlChain);
}

void Cronet_UrlResponseInfo_set_httpStatusCode(Cronet_UrlResponseInfoPtr self,
                                               int32_t httpStatusCode) {
  DCHECK(self);
  self->httpStatusCode = httpStatusCode;
}

void Cronet_UrlResponseInfo_set_httpStatusText(Cronet_UrlResponseInfoPtr self,
                                               CharString httpStatusText) {
  DCHECK(self);
  self->httpStatusText = httpStatusText;
}

void Cronet_UrlResponseInfo_add_allHeadersList(
    Cronet_UrlResponseInfoPtr self,
    Cronet_HttpHeaderPtr allHeadersList) {
  DCHECK(self);
  std::unique_ptr<Cronet_HttpHeader> tmp_ptr(allHeadersList);
  self->allHeadersList.push_back(std::move(tmp_ptr));
}

void Cronet_UrlResponseInfo_set_wasCached(Cronet_UrlResponseInfoPtr self,
                                          bool wasCached) {
  DCHECK(self);
  self->wasCached = wasCached;
}

void Cronet_UrlResponseInfo_set_negotiatedProtocol(
    Cronet_UrlResponseInfoPtr self,
    CharString negotiatedProtocol) {
  DCHECK(self);
  self->negotiatedProtocol = negotiatedProtocol;
}

void Cronet_UrlResponseInfo_set_proxyServer(Cronet_UrlResponseInfoPtr self,
                                            CharString proxyServer) {
  DCHECK(self);
  self->proxyServer = proxyServer;
}

void Cronet_UrlResponseInfo_set_receivedByteCount(
    Cronet_UrlResponseInfoPtr self,
    int64_t receivedByteCount) {
  DCHECK(self);
  self->receivedByteCount = receivedByteCount;
}

// Struct Cronet_UrlResponseInfo getters.
CharString Cronet_UrlResponseInfo_get_url(Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->url.c_str();
}

uint32_t Cronet_UrlResponseInfo_get_urlChainSize(
    Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->urlChain.size();
}
CharString Cronet_UrlResponseInfo_get_urlChainAtIndex(
    Cronet_UrlResponseInfoPtr self,
    uint32_t index) {
  DCHECK(self);
  DCHECK(index < self->urlChain.size());
  return self->urlChain[index].c_str();
}

int32_t Cronet_UrlResponseInfo_get_httpStatusCode(
    Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->httpStatusCode;
}

CharString Cronet_UrlResponseInfo_get_httpStatusText(
    Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->httpStatusText.c_str();
}

uint32_t Cronet_UrlResponseInfo_get_allHeadersListSize(
    Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->allHeadersList.size();
}
Cronet_HttpHeaderPtr Cronet_UrlResponseInfo_get_allHeadersListAtIndex(
    Cronet_UrlResponseInfoPtr self,
    uint32_t index) {
  DCHECK(self);
  DCHECK(index < self->allHeadersList.size());
  return self->allHeadersList[index].get();
}

bool Cronet_UrlResponseInfo_get_wasCached(Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->wasCached;
}

CharString Cronet_UrlResponseInfo_get_negotiatedProtocol(
    Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->negotiatedProtocol.c_str();
}

CharString Cronet_UrlResponseInfo_get_proxyServer(
    Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->proxyServer.c_str();
}

int64_t Cronet_UrlResponseInfo_get_receivedByteCount(
    Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->receivedByteCount;
}

// Struct Cronet_UrlRequestParams.
Cronet_UrlRequestParams::Cronet_UrlRequestParams() {}

Cronet_UrlRequestParams::~Cronet_UrlRequestParams() {}

Cronet_UrlRequestParamsPtr Cronet_UrlRequestParams_Create() {
  return new Cronet_UrlRequestParams();
}

void Cronet_UrlRequestParams_Destroy(Cronet_UrlRequestParamsPtr self) {
  delete self;
}

// Struct Cronet_UrlRequestParams setters.
void Cronet_UrlRequestParams_set_httpMethod(Cronet_UrlRequestParamsPtr self,
                                            CharString httpMethod) {
  DCHECK(self);
  self->httpMethod = httpMethod;
}

void Cronet_UrlRequestParams_add_requestHeaders(
    Cronet_UrlRequestParamsPtr self,
    Cronet_HttpHeaderPtr requestHeaders) {
  DCHECK(self);
  std::unique_ptr<Cronet_HttpHeader> tmp_ptr(requestHeaders);
  self->requestHeaders.push_back(std::move(tmp_ptr));
}

void Cronet_UrlRequestParams_set_disableCache(Cronet_UrlRequestParamsPtr self,
                                              bool disableCache) {
  DCHECK(self);
  self->disableCache = disableCache;
}

void Cronet_UrlRequestParams_set_priority(
    Cronet_UrlRequestParamsPtr self,
    Cronet_UrlRequestParams_REQUEST_PRIORITY priority) {
  DCHECK(self);
  self->priority = priority;
}

void Cronet_UrlRequestParams_set_uploadDataProvider(
    Cronet_UrlRequestParamsPtr self,
    Cronet_UploadDataProviderPtr uploadDataProvider) {
  DCHECK(self);
  self->uploadDataProvider = uploadDataProvider;
}

void Cronet_UrlRequestParams_set_uploadDataProviderExecutor(
    Cronet_UrlRequestParamsPtr self,
    Cronet_ExecutorPtr uploadDataProviderExecutor) {
  DCHECK(self);
  self->uploadDataProviderExecutor = uploadDataProviderExecutor;
}

void Cronet_UrlRequestParams_set_allowDirectExecutor(
    Cronet_UrlRequestParamsPtr self,
    bool allowDirectExecutor) {
  DCHECK(self);
  self->allowDirectExecutor = allowDirectExecutor;
}

void Cronet_UrlRequestParams_add_annotations(Cronet_UrlRequestParamsPtr self,
                                             RawDataPtr annotations) {
  DCHECK(self);
  self->annotations.push_back(annotations);
}

// Struct Cronet_UrlRequestParams getters.
CharString Cronet_UrlRequestParams_get_httpMethod(
    Cronet_UrlRequestParamsPtr self) {
  DCHECK(self);
  return self->httpMethod.c_str();
}

uint32_t Cronet_UrlRequestParams_get_requestHeadersSize(
    Cronet_UrlRequestParamsPtr self) {
  DCHECK(self);
  return self->requestHeaders.size();
}
Cronet_HttpHeaderPtr Cronet_UrlRequestParams_get_requestHeadersAtIndex(
    Cronet_UrlRequestParamsPtr self,
    uint32_t index) {
  DCHECK(self);
  DCHECK(index < self->requestHeaders.size());
  return self->requestHeaders[index].get();
}

bool Cronet_UrlRequestParams_get_disableCache(Cronet_UrlRequestParamsPtr self) {
  DCHECK(self);
  return self->disableCache;
}

Cronet_UrlRequestParams_REQUEST_PRIORITY Cronet_UrlRequestParams_get_priority(
    Cronet_UrlRequestParamsPtr self) {
  DCHECK(self);
  return self->priority;
}

Cronet_UploadDataProviderPtr Cronet_UrlRequestParams_get_uploadDataProvider(
    Cronet_UrlRequestParamsPtr self) {
  DCHECK(self);
  return self->uploadDataProvider;
}

Cronet_ExecutorPtr Cronet_UrlRequestParams_get_uploadDataProviderExecutor(
    Cronet_UrlRequestParamsPtr self) {
  DCHECK(self);
  return self->uploadDataProviderExecutor;
}

bool Cronet_UrlRequestParams_get_allowDirectExecutor(
    Cronet_UrlRequestParamsPtr self) {
  DCHECK(self);
  return self->allowDirectExecutor;
}

uint32_t Cronet_UrlRequestParams_get_annotationsSize(
    Cronet_UrlRequestParamsPtr self) {
  DCHECK(self);
  return self->annotations.size();
}
RawDataPtr Cronet_UrlRequestParams_get_annotationsAtIndex(
    Cronet_UrlRequestParamsPtr self,
    uint32_t index) {
  DCHECK(self);
  DCHECK(index < self->annotations.size());
  return self->annotations[index];
}
