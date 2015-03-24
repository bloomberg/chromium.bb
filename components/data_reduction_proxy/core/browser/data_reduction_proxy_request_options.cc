// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/core/common/version.h"
#include "crypto/random.h"
#include "net/base/host_port_pair.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/url_request.h"

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "google_apis/google_api_keys.h"
#endif

namespace data_reduction_proxy {

namespace {
std::string FormatOption(const std::string& name, const std::string& value) {
  return name + "=" + value;
}
}  //namespace

const char kSessionHeaderOption[] = "ps";
const char kCredentialsHeaderOption[] = "sid";
const char kBuildNumberHeaderOption[] = "b";
const char kPatchNumberHeaderOption[] = "p";
const char kClientHeaderOption[] = "c";
const char kLoFiHeaderOption[] = "q";
const char kExperimentsOption[] = "exp";

// The empty version for the authentication protocol. Currently used by
// Android webview.
#if defined(OS_ANDROID)
const char kAndroidWebViewProtocolVersion[] = "";
#endif

#define CLIENT_ENUM(name, str_value) \
    case name: return str_value;
const char* GetString(Client client) {
  switch (client) {
    CLIENT_ENUMS_LIST
  }
  NOTREACHED();
  return "";
}
#undef CLIENT_ENUM

// static
bool DataReductionProxyRequestOptions::IsKeySetOnCommandLine() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(
      data_reduction_proxy::switches::kDataReductionProxyKey);
}

DataReductionProxyRequestOptions::DataReductionProxyRequestOptions(
    Client client,
    DataReductionProxyConfig* config,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
    : client_(GetString(client)),
      data_reduction_proxy_config_(config),
      network_task_runner_(network_task_runner) {
  GetChromiumBuildAndPatch(ChromiumVersion(), &build_, &patch_);
}

DataReductionProxyRequestOptions::DataReductionProxyRequestOptions(
    Client client,
    const std::string& version,
    DataReductionProxyConfig* config,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
    : client_(GetString(client)),
      data_reduction_proxy_config_(config),
      network_task_runner_(network_task_runner) {
  GetChromiumBuildAndPatch(version, &build_, &patch_);
}

DataReductionProxyRequestOptions::~DataReductionProxyRequestOptions() {
}

void DataReductionProxyRequestOptions::Init() {
  key_ = GetDefaultKey(),
  UpdateCredentials();
  UpdateLoFi();
  UpdateVersion();
  UpdateExperiments();
}

std::string DataReductionProxyRequestOptions::ChromiumVersion() const {
#if defined(PRODUCT_VERSION)
  return PRODUCT_VERSION;
#else
  return std::string();
#endif
}

void DataReductionProxyRequestOptions::GetChromiumBuildAndPatch(
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

void DataReductionProxyRequestOptions::UpdateVersion() {
  GetChromiumBuildAndPatch(version_, &build_, &patch_);
  RegenerateRequestHeaderValue();
}

void DataReductionProxyRequestOptions::UpdateLoFi() {
  // LoFi was not enabled, but now is. Add the header option.
  if (lofi_.empty() &&
      DataReductionProxyParams::IsLoFiEnabled()) {
    lofi_ = "low";
    RegenerateRequestHeaderValue();
    return;
  }
  // LoFi was enabled, but no longer is. Remove the header option.
  if (!lofi_.empty() && !DataReductionProxyParams::IsLoFiEnabled()) {
    lofi_ = std::string();
    RegenerateRequestHeaderValue();
  }
}

void DataReductionProxyRequestOptions::UpdateExperiments() {
  std::string experiments =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          data_reduction_proxy::switches::kDataReductionProxyExperiment);
  if (experiments.empty())
    return;
  base::StringTokenizer experiment_tokenizer(experiments, ", ");
  experiment_tokenizer.set_quote_chars("\"");
  while (experiment_tokenizer.GetNext()) {
    if (!experiment_tokenizer.token().empty())
      experiments_.push_back(experiment_tokenizer.token());
  }
  RegenerateRequestHeaderValue();
}

// static
base::string16 DataReductionProxyRequestOptions::AuthHashForSalt(
    int64 salt,
    const std::string& key) {
  std::string salted_key =
      base::StringPrintf("%lld%s%lld",
                         static_cast<long long>(salt),
                         key.c_str(),
                         static_cast<long long>(salt));
  return base::UTF8ToUTF16(base::MD5String(salted_key));
}

base::Time DataReductionProxyRequestOptions::Now() const {
  return base::Time::Now();
}

void DataReductionProxyRequestOptions::RandBytes(void* output, size_t length) {
  crypto::RandBytes(output, length);
}

void DataReductionProxyRequestOptions::MaybeAddRequestHeader(
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

void DataReductionProxyRequestOptions::MaybeAddProxyTunnelRequestHandler(
    const net::HostPortPair& proxy_server,
    net::HttpRequestHeaders* request_headers) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  MaybeAddRequestHeaderImpl(proxy_server, true, request_headers);
}

void DataReductionProxyRequestOptions::SetHeader(
    net::HttpRequestHeaders* headers) {
  base::Time now = Now();
  // Authorization credentials must be regenerated at least every 24 hours.
  if (now - last_credentials_update_time_ > base::TimeDelta::FromHours(24)) {
    UpdateCredentials();
  }
  UpdateLoFi();
  const char kChromeProxyHeader[] = "Chrome-Proxy";
  std::string header_value;
  if (headers->HasHeader(kChromeProxyHeader)) {
    headers->GetHeader(kChromeProxyHeader, &header_value);
    headers->RemoveHeader(kChromeProxyHeader);
    header_value += ", ";
  }
  header_value += header_value_;
  headers->SetHeader(kChromeProxyHeader, header_value);
}

void DataReductionProxyRequestOptions::ComputeCredentials(
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

void DataReductionProxyRequestOptions::UpdateCredentials() {
  std::string session;
  std::string credentials;
  last_credentials_update_time_ = Now();
  ComputeCredentials(last_credentials_update_time_, &session_, &credentials_);
  RegenerateRequestHeaderValue();
}

void DataReductionProxyRequestOptions::SetKeyOnIO(const std::string& key) {
  if(!key.empty()) {
    key_ = key;
    UpdateCredentials();
  }
}

std::string DataReductionProxyRequestOptions::GetDefaultKey() const {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string key =
    command_line.GetSwitchValueASCII(switches::kDataReductionProxyKey);
// Android and iOS get the default key from a preprocessor constant. All other
// platforms get the key from google_apis
#if defined(OS_ANDROID) || defined(OS_IOS)
#if defined(SPDY_PROXY_AUTH_VALUE)
  if (key.empty())
    key = SPDY_PROXY_AUTH_VALUE;
#endif
#else
  if (key.empty()) {
    key = google_apis::GetSpdyProxyAuthValue();
  }
#endif  // defined(OS_ANDROID) || defined(OS_IOS)
  return key;
}

void DataReductionProxyRequestOptions::MaybeAddRequestHeaderImpl(
    const net::HostPortPair& proxy_server,
    bool expect_ssl,
    net::HttpRequestHeaders* request_headers) {
  if (proxy_server.IsEmpty())
    return;
  if (data_reduction_proxy_config_ &&
      data_reduction_proxy_config_->IsDataReductionProxy(proxy_server, NULL) &&
      data_reduction_proxy_config_->UsingHTTPTunnel(proxy_server) ==
          expect_ssl) {
    SetHeader(request_headers);
  }
}

void DataReductionProxyRequestOptions::RegenerateRequestHeaderValue() {
  header_value_ = FormatOption(kSessionHeaderOption, session_)
                + ", " + FormatOption(kCredentialsHeaderOption, credentials_)
                + (client_.empty() ?
                       "" : ", " + FormatOption(kClientHeaderOption, client_))
                + (build_.empty() || patch_.empty() ?
                       "" :
                       ", " + FormatOption(kBuildNumberHeaderOption, build_)
                       + ", " + FormatOption(kPatchNumberHeaderOption, patch_))
                + (lofi_.empty() ?
                       "" : ", " + FormatOption(kLoFiHeaderOption, lofi_));
  for (const auto& experiment : experiments_)
    header_value_ += ", " + FormatOption(kExperimentsOption, experiment);
}

}  // namespace data_reduction_proxy
