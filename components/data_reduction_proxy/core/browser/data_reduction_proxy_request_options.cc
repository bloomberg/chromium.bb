// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_client_config_parser.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/core/common/version.h"
#include "crypto/random.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/url_request.h"

#if defined(USE_GOOGLE_API_KEYS_FOR_AUTH_KEY)
#include "google_apis/google_api_keys.h"
#endif

namespace data_reduction_proxy {
namespace {

std::string FormatOption(const std::string& name, const std::string& value) {
  return name + "=" + value;
}

}  // namespace

const char kSessionHeaderOption[] = "ps";
const char kCredentialsHeaderOption[] = "sid";
const char kSecureSessionHeaderOption[] = "s";
const char kBuildNumberHeaderOption[] = "b";
const char kPatchNumberHeaderOption[] = "p";
const char kClientHeaderOption[] = "c";
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
    DataReductionProxyConfig* config)
    : client_(GetString(client)),
      use_assigned_credentials_(false),
      data_reduction_proxy_config_(config) {
  DCHECK(data_reduction_proxy_config_);
  GetChromiumBuildAndPatch(ChromiumVersion(), &build_, &patch_);
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyRequestOptions::DataReductionProxyRequestOptions(
    Client client,
    const std::string& version,
    DataReductionProxyConfig* config)
    : client_(GetString(client)),
      use_assigned_credentials_(false),
      data_reduction_proxy_config_(config) {
  DCHECK(data_reduction_proxy_config_);
  GetChromiumBuildAndPatch(version, &build_, &patch_);
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyRequestOptions::~DataReductionProxyRequestOptions() {
}

void DataReductionProxyRequestOptions::Init() {
  key_ = GetDefaultKey(),
  UpdateCredentials();
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
  std::vector<base::StringPiece> version_parts = base::SplitStringPiece(
      version, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (version_parts.size() != 4)
    return;
  version_parts[2].CopyToString(build);
  version_parts[3].CopyToString(patch);
}

void DataReductionProxyRequestOptions::UpdateVersion() {
  GetChromiumBuildAndPatch(version_, &build_, &patch_);
  RegenerateRequestHeaderValue();
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
    int64_t salt,
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

void DataReductionProxyRequestOptions::RandBytes(void* output,
                                                 size_t length) const {
  crypto::RandBytes(output, length);
}

void DataReductionProxyRequestOptions::MaybeAddRequestHeader(
    const net::ProxyServer& proxy_server,
    net::HttpRequestHeaders* request_headers) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!proxy_server.is_valid())
    return;
  if (proxy_server.is_direct())
    return;
  MaybeAddRequestHeaderImpl(proxy_server.host_port_pair(), false,
                            request_headers);
}

void DataReductionProxyRequestOptions::MaybeAddProxyTunnelRequestHandler(
    const net::HostPortPair& proxy_server,
    net::HttpRequestHeaders* request_headers) {
  DCHECK(thread_checker_.CalledOnValidThread());
  MaybeAddRequestHeaderImpl(proxy_server, true, request_headers);
}

void DataReductionProxyRequestOptions::SetHeader(
    net::HttpRequestHeaders* headers) {
  base::Time now = Now();
  // Authorization credentials must be regenerated if they are expired.
  if (!use_assigned_credentials_ && (now > credentials_expiration_time_))
    UpdateCredentials();
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
    std::string* credentials) const {
  DCHECK(session);
  DCHECK(credentials);
  int64_t timestamp = (now - base::Time::UnixEpoch()).InMilliseconds() / 1000;

  int32_t rand[3];
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
  base::Time now = Now();
  ComputeCredentials(now, &session_, &credentials_);
  credentials_expiration_time_ = now + base::TimeDelta::FromHours(24);
  RegenerateRequestHeaderValue();
}

void DataReductionProxyRequestOptions::SetKeyOnIO(const std::string& key) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if(!key.empty()) {
    key_ = key;
    UpdateCredentials();
  }
}

void DataReductionProxyRequestOptions::SetSecureSession(
    const std::string& secure_session) {
  DCHECK(thread_checker_.CalledOnValidThread());
  session_.clear();
  credentials_.clear();
  secure_session_ = secure_session;
  // Force skipping of credential regeneration. It should be handled by the
  // caller.
  use_assigned_credentials_ = true;
  RegenerateRequestHeaderValue();
}

void DataReductionProxyRequestOptions::Invalidate() {
  SetSecureSession(std::string());
}

std::string DataReductionProxyRequestOptions::GetDefaultKey() const {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string key =
    command_line.GetSwitchValueASCII(switches::kDataReductionProxyKey);
// Chrome on iOS gets the default key from a preprocessor constant. Chrome on
// Android and Chrome on desktop get the key from google_apis. Cronet and
// Webview have no default key.
#if defined(OS_IOS)
#if defined(SPDY_PROXY_AUTH_VALUE)
  if (key.empty())
    key = SPDY_PROXY_AUTH_VALUE;
#endif
#elif USE_GOOGLE_API_KEYS_FOR_AUTH_KEY
  if (key.empty()) {
    key = google_apis::GetSpdyProxyAuthValue();
  }
#endif  // defined(OS_IOS)
  return key;
}

const std::string& DataReductionProxyRequestOptions::GetSecureSession() const {
  return secure_session_;
}

void DataReductionProxyRequestOptions::MaybeAddRequestHeaderImpl(
    const net::HostPortPair& proxy_server,
    bool expect_ssl,
    net::HttpRequestHeaders* request_headers) {
  if (proxy_server.IsEmpty())
    return;
  if (data_reduction_proxy_config_->IsDataReductionProxy(proxy_server, NULL) &&
      data_reduction_proxy_config_->UsingHTTPTunnel(proxy_server) ==
          expect_ssl) {
    SetHeader(request_headers);
  }
}

void DataReductionProxyRequestOptions::RegenerateRequestHeaderValue() {
  std::vector<std::string> headers;
  if (!session_.empty())
    headers.push_back(FormatOption(kSessionHeaderOption, session_));
  if (!credentials_.empty())
    headers.push_back(FormatOption(kCredentialsHeaderOption, credentials_));
  if (!secure_session_.empty()) {
    headers.push_back(
        FormatOption(kSecureSessionHeaderOption, secure_session_));
  }
  if (!client_.empty())
    headers.push_back(FormatOption(kClientHeaderOption, client_));
  if (!build_.empty() && !patch_.empty()) {
    headers.push_back(FormatOption(kBuildNumberHeaderOption, build_));
    headers.push_back(FormatOption(kPatchNumberHeaderOption, patch_));
  }
  for (const auto& experiment : experiments_)
    headers.push_back(FormatOption(kExperimentsOption, experiment));

  header_value_ = base::JoinString(headers, ", ");
}

}  // namespace data_reduction_proxy
