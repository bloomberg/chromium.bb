// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* DO NOT EDIT. Generated from components/cronet/native/generated/cronet.idl */

#include "components/cronet/native/generated/cronet.idl_impl_struct.h"

#include <utility>

#include "base/logging.h"

// Struct Cronet_Error.
Cronet_Error::Cronet_Error() {}

Cronet_Error::~Cronet_Error() {}

Cronet_ErrorPtr Cronet_Error_Create() {
  return new Cronet_Error();
}

void Cronet_Error_Destroy(Cronet_ErrorPtr self) {
  delete self;
}

// Struct Cronet_Error setters.
void Cronet_Error_set_errorCode(Cronet_ErrorPtr self,
                                Cronet_Error_ERROR_CODE errorCode) {
  DCHECK(self);
  self->errorCode = errorCode;
}

void Cronet_Error_set_message(Cronet_ErrorPtr self, Cronet_String message) {
  DCHECK(self);
  self->message = message;
}

void Cronet_Error_set_internal_error_code(Cronet_ErrorPtr self,
                                          int32_t internal_error_code) {
  DCHECK(self);
  self->internal_error_code = internal_error_code;
}

void Cronet_Error_set_immediately_retryable(Cronet_ErrorPtr self,
                                            bool immediately_retryable) {
  DCHECK(self);
  self->immediately_retryable = immediately_retryable;
}

void Cronet_Error_set_quic_detailed_error_code(
    Cronet_ErrorPtr self,
    int32_t quic_detailed_error_code) {
  DCHECK(self);
  self->quic_detailed_error_code = quic_detailed_error_code;
}

// Struct Cronet_Error getters.
Cronet_Error_ERROR_CODE Cronet_Error_get_errorCode(Cronet_ErrorPtr self) {
  DCHECK(self);
  return self->errorCode;
}

Cronet_String Cronet_Error_get_message(Cronet_ErrorPtr self) {
  DCHECK(self);
  return self->message.c_str();
}

int32_t Cronet_Error_get_internal_error_code(Cronet_ErrorPtr self) {
  DCHECK(self);
  return self->internal_error_code;
}

bool Cronet_Error_get_immediately_retryable(Cronet_ErrorPtr self) {
  DCHECK(self);
  return self->immediately_retryable;
}

int32_t Cronet_Error_get_quic_detailed_error_code(Cronet_ErrorPtr self) {
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
void Cronet_QuicHint_set_host(Cronet_QuicHintPtr self, Cronet_String host) {
  DCHECK(self);
  self->host = host;
}

void Cronet_QuicHint_set_port(Cronet_QuicHintPtr self, int32_t port) {
  DCHECK(self);
  self->port = port;
}

void Cronet_QuicHint_set_alternate_port(Cronet_QuicHintPtr self,
                                        int32_t alternate_port) {
  DCHECK(self);
  self->alternate_port = alternate_port;
}

// Struct Cronet_QuicHint getters.
Cronet_String Cronet_QuicHint_get_host(Cronet_QuicHintPtr self) {
  DCHECK(self);
  return self->host.c_str();
}

int32_t Cronet_QuicHint_get_port(Cronet_QuicHintPtr self) {
  DCHECK(self);
  return self->port;
}

int32_t Cronet_QuicHint_get_alternate_port(Cronet_QuicHintPtr self) {
  DCHECK(self);
  return self->alternate_port;
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
                                   Cronet_String host) {
  DCHECK(self);
  self->host = host;
}

void Cronet_PublicKeyPins_add_pins_sha256(Cronet_PublicKeyPinsPtr self,
                                          Cronet_String pins_sha256) {
  DCHECK(self);
  self->pins_sha256.push_back(pins_sha256);
}

void Cronet_PublicKeyPins_set_include_subdomains(Cronet_PublicKeyPinsPtr self,
                                                 bool include_subdomains) {
  DCHECK(self);
  self->include_subdomains = include_subdomains;
}

void Cronet_PublicKeyPins_set_expiration_date(Cronet_PublicKeyPinsPtr self,
                                              int64_t expiration_date) {
  DCHECK(self);
  self->expiration_date = expiration_date;
}

// Struct Cronet_PublicKeyPins getters.
Cronet_String Cronet_PublicKeyPins_get_host(Cronet_PublicKeyPinsPtr self) {
  DCHECK(self);
  return self->host.c_str();
}

uint32_t Cronet_PublicKeyPins_get_pins_sha256Size(
    Cronet_PublicKeyPinsPtr self) {
  DCHECK(self);
  return self->pins_sha256.size();
}
Cronet_String Cronet_PublicKeyPins_get_pins_sha256AtIndex(
    Cronet_PublicKeyPinsPtr self,
    uint32_t index) {
  DCHECK(self);
  DCHECK(index < self->pins_sha256.size());
  return self->pins_sha256[index].c_str();
}

bool Cronet_PublicKeyPins_get_include_subdomains(Cronet_PublicKeyPinsPtr self) {
  DCHECK(self);
  return self->include_subdomains;
}

int64_t Cronet_PublicKeyPins_get_expiration_date(Cronet_PublicKeyPinsPtr self) {
  DCHECK(self);
  return self->expiration_date;
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
void Cronet_EngineParams_set_enable_check_result(Cronet_EngineParamsPtr self,
                                                 bool enable_check_result) {
  DCHECK(self);
  self->enable_check_result = enable_check_result;
}

void Cronet_EngineParams_set_user_agent(Cronet_EngineParamsPtr self,
                                        Cronet_String user_agent) {
  DCHECK(self);
  self->user_agent = user_agent;
}

void Cronet_EngineParams_set_accept_language(Cronet_EngineParamsPtr self,
                                             Cronet_String accept_language) {
  DCHECK(self);
  self->accept_language = accept_language;
}

void Cronet_EngineParams_set_storage_path(Cronet_EngineParamsPtr self,
                                          Cronet_String storage_path) {
  DCHECK(self);
  self->storage_path = storage_path;
}

void Cronet_EngineParams_set_enable_quic(Cronet_EngineParamsPtr self,
                                         bool enable_quic) {
  DCHECK(self);
  self->enable_quic = enable_quic;
}

void Cronet_EngineParams_set_enable_http2(Cronet_EngineParamsPtr self,
                                          bool enable_http2) {
  DCHECK(self);
  self->enable_http2 = enable_http2;
}

void Cronet_EngineParams_set_enable_brotli(Cronet_EngineParamsPtr self,
                                           bool enable_brotli) {
  DCHECK(self);
  self->enable_brotli = enable_brotli;
}

void Cronet_EngineParams_set_http_cache_mode(
    Cronet_EngineParamsPtr self,
    Cronet_EngineParams_HTTP_CACHE_MODE http_cache_mode) {
  DCHECK(self);
  self->http_cache_mode = http_cache_mode;
}

void Cronet_EngineParams_set_http_cache_max_size(Cronet_EngineParamsPtr self,
                                                 int64_t http_cache_max_size) {
  DCHECK(self);
  self->http_cache_max_size = http_cache_max_size;
}

void Cronet_EngineParams_add_quic_hints(Cronet_EngineParamsPtr self,
                                        Cronet_QuicHintPtr quic_hints) {
  DCHECK(self);
  std::unique_ptr<Cronet_QuicHint> tmp_ptr(quic_hints);
  self->quic_hints.push_back(std::move(tmp_ptr));
}

void Cronet_EngineParams_add_public_key_pins(
    Cronet_EngineParamsPtr self,
    Cronet_PublicKeyPinsPtr public_key_pins) {
  DCHECK(self);
  std::unique_ptr<Cronet_PublicKeyPins> tmp_ptr(public_key_pins);
  self->public_key_pins.push_back(std::move(tmp_ptr));
}

void Cronet_EngineParams_set_enable_public_key_pinning_bypass_for_local_trust_anchors(
    Cronet_EngineParamsPtr self,
    bool enable_public_key_pinning_bypass_for_local_trust_anchors) {
  DCHECK(self);
  self->enable_public_key_pinning_bypass_for_local_trust_anchors =
      enable_public_key_pinning_bypass_for_local_trust_anchors;
}

void Cronet_EngineParams_set_experimental_options(
    Cronet_EngineParamsPtr self,
    Cronet_String experimental_options) {
  DCHECK(self);
  self->experimental_options = experimental_options;
}

// Struct Cronet_EngineParams getters.
bool Cronet_EngineParams_get_enable_check_result(Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->enable_check_result;
}

Cronet_String Cronet_EngineParams_get_user_agent(Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->user_agent.c_str();
}

Cronet_String Cronet_EngineParams_get_accept_language(
    Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->accept_language.c_str();
}

Cronet_String Cronet_EngineParams_get_storage_path(
    Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->storage_path.c_str();
}

bool Cronet_EngineParams_get_enable_quic(Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->enable_quic;
}

bool Cronet_EngineParams_get_enable_http2(Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->enable_http2;
}

bool Cronet_EngineParams_get_enable_brotli(Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->enable_brotli;
}

Cronet_EngineParams_HTTP_CACHE_MODE Cronet_EngineParams_get_http_cache_mode(
    Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->http_cache_mode;
}

int64_t Cronet_EngineParams_get_http_cache_max_size(
    Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->http_cache_max_size;
}

uint32_t Cronet_EngineParams_get_quic_hintsSize(Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->quic_hints.size();
}
Cronet_QuicHintPtr Cronet_EngineParams_get_quic_hintsAtIndex(
    Cronet_EngineParamsPtr self,
    uint32_t index) {
  DCHECK(self);
  DCHECK(index < self->quic_hints.size());
  return self->quic_hints[index].get();
}

uint32_t Cronet_EngineParams_get_public_key_pinsSize(
    Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->public_key_pins.size();
}
Cronet_PublicKeyPinsPtr Cronet_EngineParams_get_public_key_pinsAtIndex(
    Cronet_EngineParamsPtr self,
    uint32_t index) {
  DCHECK(self);
  DCHECK(index < self->public_key_pins.size());
  return self->public_key_pins[index].get();
}

bool Cronet_EngineParams_get_enable_public_key_pinning_bypass_for_local_trust_anchors(
    Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->enable_public_key_pinning_bypass_for_local_trust_anchors;
}

Cronet_String Cronet_EngineParams_get_experimental_options(
    Cronet_EngineParamsPtr self) {
  DCHECK(self);
  return self->experimental_options.c_str();
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
void Cronet_HttpHeader_set_name(Cronet_HttpHeaderPtr self, Cronet_String name) {
  DCHECK(self);
  self->name = name;
}

void Cronet_HttpHeader_set_value(Cronet_HttpHeaderPtr self,
                                 Cronet_String value) {
  DCHECK(self);
  self->value = value;
}

// Struct Cronet_HttpHeader getters.
Cronet_String Cronet_HttpHeader_get_name(Cronet_HttpHeaderPtr self) {
  DCHECK(self);
  return self->name.c_str();
}

Cronet_String Cronet_HttpHeader_get_value(Cronet_HttpHeaderPtr self) {
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
                                    Cronet_String url) {
  DCHECK(self);
  self->url = url;
}

void Cronet_UrlResponseInfo_add_url_chain(Cronet_UrlResponseInfoPtr self,
                                          Cronet_String url_chain) {
  DCHECK(self);
  self->url_chain.push_back(url_chain);
}

void Cronet_UrlResponseInfo_set_http_status_code(Cronet_UrlResponseInfoPtr self,
                                                 int32_t http_status_code) {
  DCHECK(self);
  self->http_status_code = http_status_code;
}

void Cronet_UrlResponseInfo_set_http_status_text(
    Cronet_UrlResponseInfoPtr self,
    Cronet_String http_status_text) {
  DCHECK(self);
  self->http_status_text = http_status_text;
}

void Cronet_UrlResponseInfo_add_all_headers_list(
    Cronet_UrlResponseInfoPtr self,
    Cronet_HttpHeaderPtr all_headers_list) {
  DCHECK(self);
  std::unique_ptr<Cronet_HttpHeader> tmp_ptr(all_headers_list);
  self->all_headers_list.push_back(std::move(tmp_ptr));
}

void Cronet_UrlResponseInfo_set_was_cached(Cronet_UrlResponseInfoPtr self,
                                           bool was_cached) {
  DCHECK(self);
  self->was_cached = was_cached;
}

void Cronet_UrlResponseInfo_set_negotiated_protocol(
    Cronet_UrlResponseInfoPtr self,
    Cronet_String negotiated_protocol) {
  DCHECK(self);
  self->negotiated_protocol = negotiated_protocol;
}

void Cronet_UrlResponseInfo_set_proxy_server(Cronet_UrlResponseInfoPtr self,
                                             Cronet_String proxy_server) {
  DCHECK(self);
  self->proxy_server = proxy_server;
}

void Cronet_UrlResponseInfo_set_received_byte_count(
    Cronet_UrlResponseInfoPtr self,
    int64_t received_byte_count) {
  DCHECK(self);
  self->received_byte_count = received_byte_count;
}

// Struct Cronet_UrlResponseInfo getters.
Cronet_String Cronet_UrlResponseInfo_get_url(Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->url.c_str();
}

uint32_t Cronet_UrlResponseInfo_get_url_chainSize(
    Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->url_chain.size();
}
Cronet_String Cronet_UrlResponseInfo_get_url_chainAtIndex(
    Cronet_UrlResponseInfoPtr self,
    uint32_t index) {
  DCHECK(self);
  DCHECK(index < self->url_chain.size());
  return self->url_chain[index].c_str();
}

int32_t Cronet_UrlResponseInfo_get_http_status_code(
    Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->http_status_code;
}

Cronet_String Cronet_UrlResponseInfo_get_http_status_text(
    Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->http_status_text.c_str();
}

uint32_t Cronet_UrlResponseInfo_get_all_headers_listSize(
    Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->all_headers_list.size();
}
Cronet_HttpHeaderPtr Cronet_UrlResponseInfo_get_all_headers_listAtIndex(
    Cronet_UrlResponseInfoPtr self,
    uint32_t index) {
  DCHECK(self);
  DCHECK(index < self->all_headers_list.size());
  return self->all_headers_list[index].get();
}

bool Cronet_UrlResponseInfo_get_was_cached(Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->was_cached;
}

Cronet_String Cronet_UrlResponseInfo_get_negotiated_protocol(
    Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->negotiated_protocol.c_str();
}

Cronet_String Cronet_UrlResponseInfo_get_proxy_server(
    Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->proxy_server.c_str();
}

int64_t Cronet_UrlResponseInfo_get_received_byte_count(
    Cronet_UrlResponseInfoPtr self) {
  DCHECK(self);
  return self->received_byte_count;
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
void Cronet_UrlRequestParams_set_http_method(Cronet_UrlRequestParamsPtr self,
                                             Cronet_String http_method) {
  DCHECK(self);
  self->http_method = http_method;
}

void Cronet_UrlRequestParams_add_request_headers(
    Cronet_UrlRequestParamsPtr self,
    Cronet_HttpHeaderPtr request_headers) {
  DCHECK(self);
  std::unique_ptr<Cronet_HttpHeader> tmp_ptr(request_headers);
  self->request_headers.push_back(std::move(tmp_ptr));
}

void Cronet_UrlRequestParams_set_disable_cache(Cronet_UrlRequestParamsPtr self,
                                               bool disable_cache) {
  DCHECK(self);
  self->disable_cache = disable_cache;
}

void Cronet_UrlRequestParams_set_priority(
    Cronet_UrlRequestParamsPtr self,
    Cronet_UrlRequestParams_REQUEST_PRIORITY priority) {
  DCHECK(self);
  self->priority = priority;
}

void Cronet_UrlRequestParams_set_upload_data_provider(
    Cronet_UrlRequestParamsPtr self,
    Cronet_UploadDataProviderPtr upload_data_provider) {
  DCHECK(self);
  self->upload_data_provider = upload_data_provider;
}

void Cronet_UrlRequestParams_set_upload_data_provider_executor(
    Cronet_UrlRequestParamsPtr self,
    Cronet_ExecutorPtr upload_data_provider_executor) {
  DCHECK(self);
  self->upload_data_provider_executor = upload_data_provider_executor;
}

void Cronet_UrlRequestParams_set_allow_direct_executor(
    Cronet_UrlRequestParamsPtr self,
    bool allow_direct_executor) {
  DCHECK(self);
  self->allow_direct_executor = allow_direct_executor;
}

void Cronet_UrlRequestParams_add_annotations(Cronet_UrlRequestParamsPtr self,
                                             Cronet_RawDataPtr annotations) {
  DCHECK(self);
  self->annotations.push_back(annotations);
}

// Struct Cronet_UrlRequestParams getters.
Cronet_String Cronet_UrlRequestParams_get_http_method(
    Cronet_UrlRequestParamsPtr self) {
  DCHECK(self);
  return self->http_method.c_str();
}

uint32_t Cronet_UrlRequestParams_get_request_headersSize(
    Cronet_UrlRequestParamsPtr self) {
  DCHECK(self);
  return self->request_headers.size();
}
Cronet_HttpHeaderPtr Cronet_UrlRequestParams_get_request_headersAtIndex(
    Cronet_UrlRequestParamsPtr self,
    uint32_t index) {
  DCHECK(self);
  DCHECK(index < self->request_headers.size());
  return self->request_headers[index].get();
}

bool Cronet_UrlRequestParams_get_disable_cache(
    Cronet_UrlRequestParamsPtr self) {
  DCHECK(self);
  return self->disable_cache;
}

Cronet_UrlRequestParams_REQUEST_PRIORITY Cronet_UrlRequestParams_get_priority(
    Cronet_UrlRequestParamsPtr self) {
  DCHECK(self);
  return self->priority;
}

Cronet_UploadDataProviderPtr Cronet_UrlRequestParams_get_upload_data_provider(
    Cronet_UrlRequestParamsPtr self) {
  DCHECK(self);
  return self->upload_data_provider;
}

Cronet_ExecutorPtr Cronet_UrlRequestParams_get_upload_data_provider_executor(
    Cronet_UrlRequestParamsPtr self) {
  DCHECK(self);
  return self->upload_data_provider_executor;
}

bool Cronet_UrlRequestParams_get_allow_direct_executor(
    Cronet_UrlRequestParamsPtr self) {
  DCHECK(self);
  return self->allow_direct_executor;
}

uint32_t Cronet_UrlRequestParams_get_annotationsSize(
    Cronet_UrlRequestParamsPtr self) {
  DCHECK(self);
  return self->annotations.size();
}
Cronet_RawDataPtr Cronet_UrlRequestParams_get_annotationsAtIndex(
    Cronet_UrlRequestParamsPtr self,
    uint32_t index) {
  DCHECK(self);
  DCHECK(index < self->annotations.size());
  return self->annotations[index];
}

// Struct Cronet_RequestFinishedInfo.
Cronet_RequestFinishedInfo::Cronet_RequestFinishedInfo() {}

Cronet_RequestFinishedInfo::~Cronet_RequestFinishedInfo() {}

Cronet_RequestFinishedInfoPtr Cronet_RequestFinishedInfo_Create() {
  return new Cronet_RequestFinishedInfo();
}

void Cronet_RequestFinishedInfo_Destroy(Cronet_RequestFinishedInfoPtr self) {
  delete self;
}

// Struct Cronet_RequestFinishedInfo setters.

// Struct Cronet_RequestFinishedInfo getters.
