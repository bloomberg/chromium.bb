// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_auth_request_handler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_protocol.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/common/version.h"
#include "crypto/random.h"
#include "net/base/host_port_pair.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

// The empty version for the authentication protocol. Currently used by
// Android webview.
#if defined(OS_ANDROID)
const char kAndroidWebViewProtocolVersion[] = "";
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
    DataReductionProxyParams* params,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
    : client_(client),
      data_reduction_proxy_params_(params),
      network_task_runner_(network_task_runner) {
  GetChromiumBuildAndPatch(ChromiumVersion(), &build_number_, &patch_number_);
  Init();
}

DataReductionProxyAuthRequestHandler::DataReductionProxyAuthRequestHandler(
    const std::string& client,
    const std::string& version,
    DataReductionProxyParams* params,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
    : client_(client),
      data_reduction_proxy_params_(params),
      network_task_runner_(network_task_runner) {
  GetChromiumBuildAndPatch(version, &build_number_, &patch_number_);
  Init();
}

std::string DataReductionProxyAuthRequestHandler::ChromiumVersion() const {
#if defined(PRODUCT_VERSION)
  return PRODUCT_VERSION;
#else
  return std::string();
#endif
}


void DataReductionProxyAuthRequestHandler::GetChromiumBuildAndPatch(
    const std::string& version,
    std::string* build,
    std::string* patch) const {
  std::vector<std::string> version_parts;
  base::SplitString(version, '.', &version_parts);
  if (version_parts.size() != 4)
    return;
  *build = version_parts[2];
  *patch = version_parts[3];
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
  if (proxy_server.is_direct())
    return;
  MaybeAddRequestHeaderImpl(proxy_server.host_port_pair(),
                            false,
                            request_headers);
}

void DataReductionProxyAuthRequestHandler::MaybeAddProxyTunnelRequestHandler(
    const net::HostPortPair& proxy_server,
    net::HttpRequestHeaders* request_headers) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  MaybeAddRequestHeaderImpl(proxy_server, true, request_headers);
}

void DataReductionProxyAuthRequestHandler::AddAuthorizationHeader(
    net::HttpRequestHeaders* headers) {
  base::Time now = Now();
  if (now - last_update_time_ > base::TimeDelta::FromHours(24)) {
    last_update_time_ = now;
    ComputeCredentials(last_update_time_, &session_, &credentials_);
  }
  const char kChromeProxyHeader[] = "Chrome-Proxy";
  std::string header_value;
  if (headers->HasHeader(kChromeProxyHeader)) {
    headers->GetHeader(kChromeProxyHeader, &header_value);
    headers->RemoveHeader(kChromeProxyHeader);
    header_value += ", ";
  }
  header_value +=
      "ps=" + session_ + ", sid=" + credentials_;
  if (!build_number_.empty() && !patch_number_.empty())
    header_value += ", b=" + build_number_ + ", p=" + patch_number_;
  if (!client_.empty())
    header_value += ", c=" + client_;
  headers->SetHeader(kChromeProxyHeader, header_value);
}

void DataReductionProxyAuthRequestHandler::InitAuthenticationOnUI(
    const std::string& key) {
  network_task_runner_->PostTask(FROM_HERE, base::Bind(
      &DataReductionProxyAuthRequestHandler::InitAuthentication,
      base::Unretained(this),
      key));
}

void DataReductionProxyAuthRequestHandler::ComputeCredentials(
    const base::Time& now,
    std::string* session,
    std::string* credentials) {
  DCHECK(session);
  DCHECK(credentials);
  int64 timestamp =
      (now - base::Time::UnixEpoch()).InMilliseconds() / 1000;

  int32 rand[3];
  RandBytes(rand, 3 * sizeof(rand[0]));
  *session = base::StringPrintf("%lld-%u-%u-%u",
                                static_cast<long long>(timestamp),
                                rand[0],
                                rand[1],
                                rand[2]);
  *credentials = base::UTF16ToUTF8(AuthHashForSalt(timestamp, key_));

  DVLOG(1) << "session: [" << *session << "] "
           << "password: [" << *credentials  << "]";
}

void DataReductionProxyAuthRequestHandler::InitAuthentication(
    const std::string& key) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  key_ = key;
  last_update_time_ = Now();
  ComputeCredentials(last_update_time_, &session_, &credentials_);
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

void DataReductionProxyAuthRequestHandler::MaybeAddRequestHeaderImpl(
    const net::HostPortPair& proxy_server,
    bool expect_ssl,
    net::HttpRequestHeaders* request_headers) {
  if (proxy_server.IsEmpty())
    return;
  if (data_reduction_proxy_params_ &&
      data_reduction_proxy_params_->IsDataReductionProxy(proxy_server, NULL) &&
      net::HostPortPair::FromURL(
          data_reduction_proxy_params_->ssl_origin()).Equals(
              proxy_server) == expect_ssl) {
    AddAuthorizationHeader(request_headers);
  }
}

}  // namespace data_reduction_proxy
