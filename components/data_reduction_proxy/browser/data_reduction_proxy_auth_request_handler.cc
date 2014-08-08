// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_auth_request_handler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_protocol.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_switches.h"
#include "crypto/random.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

// The empty version for the authentication protocol. Currently used by
// Android webview.
#if defined(OS_ANDROID)
const char kAndroidWebViewProtocolVersion[] = "0";
#endif

// The clients supported by the data reduction proxy.
const char kClientAndroidWebview[] = "webview";
const char kClientChromeAndroid[] = "android";
const char kClientChromeIOS[] = "ios";

// static
bool DataReductionProxyAuthRequestHandler::IsKeySetOnCommandLine() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(
      data_reduction_proxy::switches::kDataReductionProxyKey);
}

DataReductionProxyAuthRequestHandler::DataReductionProxyAuthRequestHandler(
    const std::string& client,
    const std::string& version,
    DataReductionProxyParams* params,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
    : data_reduction_proxy_params_(params),
      network_task_runner_(network_task_runner) {
  client_ = client;
  version_ = version;
  Init();
}

void DataReductionProxyAuthRequestHandler::Init() {
  InitAuthenticationOnUI(GetDefaultKey());
}


DataReductionProxyAuthRequestHandler::~DataReductionProxyAuthRequestHandler() {
}

// static
base::string16 DataReductionProxyAuthRequestHandler::AuthHashForSalt(
    int64 salt,
    const std::string& key) {
  std::string salted_key =
      base::StringPrintf("%lld%s%lld",
                         static_cast<long long>(salt),
                         key.c_str(),
                         static_cast<long long>(salt));
  return base::UTF8ToUTF16(base::MD5String(salted_key));
}



base::Time DataReductionProxyAuthRequestHandler::Now() const {
  return base::Time::Now();
}

void DataReductionProxyAuthRequestHandler::RandBytes(
    void* output, size_t length) {
  crypto::RandBytes(output, length);
}

void DataReductionProxyAuthRequestHandler::MaybeAddRequestHeader(
    net::URLRequest* request,
    const net::ProxyServer& proxy_server,
    net::HttpRequestHeaders* request_headers) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  if (!proxy_server.is_valid())
    return;
  if (data_reduction_proxy_params_ &&
      data_reduction_proxy_params_->IsDataReductionProxy(
          proxy_server.host_port_pair(), NULL)) {
    AddAuthorizationHeader(request_headers);
  }
}

void DataReductionProxyAuthRequestHandler::AddAuthorizationHeader(
    net::HttpRequestHeaders* headers) {
  const char kChromeProxyHeader[] = "Chrome-Proxy";
  std::string header_value;
  if (headers->HasHeader(kChromeProxyHeader)) {
    headers->GetHeader(kChromeProxyHeader, &header_value);
    headers->RemoveHeader(kChromeProxyHeader);
    header_value += ", ";
  }
  header_value +=
      "ps=" + session_ + ", sid=" + credentials_ +  ", v=" + version_;
  if (!client_.empty())
    header_value += ", c=" + client_;
  headers->SetHeader(kChromeProxyHeader, header_value);
}

void DataReductionProxyAuthRequestHandler::InitAuthenticationOnUI(
    const std::string& key) {
  key_ = key;
  int64 timestamp =
      (Now() - base::Time::UnixEpoch()).InMilliseconds() / 1000;

  int32 rand[3];
  RandBytes(rand, 3 * sizeof(rand[0]));
  std::string session = base::StringPrintf("%lld-%u-%u-%u",
                                           static_cast<long long>(timestamp),
                                           rand[0],
                                           rand[1],
                                           rand[2]);
  std::string credentials = base::UTF16ToUTF8(AuthHashForSalt(timestamp, key_));

  DVLOG(1) << "session: [" << session << "] "
           << "password: [" << credentials  << "]";
  network_task_runner_->PostTask(FROM_HERE, base::Bind(
      &DataReductionProxyAuthRequestHandler::InitAuthentication,
      base::Unretained(this),
      session,
      credentials));
}

void DataReductionProxyAuthRequestHandler::InitAuthentication(
    const std::string& session,
    const std::string& credentials) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  session_ = session;
  credentials_ = credentials;
}

void DataReductionProxyAuthRequestHandler::SetKeyOnUI(const std::string& key) {
  if (!key.empty())
    InitAuthenticationOnUI(key);
}


std::string DataReductionProxyAuthRequestHandler::GetDefaultKey() const {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string key =
    command_line.GetSwitchValueASCII(switches::kDataReductionProxyKey);
#if defined(SPDY_PROXY_AUTH_VALUE)
  if (key.empty())
    key = SPDY_PROXY_AUTH_VALUE;
#endif
  return key;
}

}  // namespace data_reduction_proxy
