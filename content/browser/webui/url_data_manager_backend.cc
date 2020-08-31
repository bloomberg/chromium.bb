// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/url_data_manager_backend.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/webui/shared_resources_data_source.h"
#include "content/browser/webui/url_data_source_impl.h"
#include "content/browser/webui/web_ui_data_source_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/filter/source_stream.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "ui/base/template_expressions.h"
#include "ui/base/webui/i18n_source_stream.h"
#include "url/url_util.h"

namespace content {

namespace {

const char kChromeURLContentSecurityPolicyHeaderName[] =
    "Content-Security-Policy";
const char kChromeURLContentSecurityPolicyReportOnlyHeaderName[] =
    "Content-Security-Policy-Report-Only";
const char kChromeURLContentSecurityPolicyReportOnlyHeaderValue[] =
    "require-trusted-types-for 'script'";

const char kChromeURLXFrameOptionsHeaderName[] = "X-Frame-Options";
const char kChromeURLXFrameOptionsHeaderValue[] = "DENY";
const char kNetworkErrorKey[] = "netError";
const char kURLDataManagerBackendKeyName[] = "url_data_manager_backend";

bool SchemeIsInSchemes(const std::string& scheme,
                       const std::vector<std::string>& schemes) {
  return base::Contains(schemes, scheme);
}

}  // namespace

URLDataManagerBackend::URLDataManagerBackend() : next_request_id_(0) {
  URLDataSource* shared_source = new SharedResourcesDataSource();
  AddDataSource(new URLDataSourceImpl(shared_source->GetSource(),
                                      base::WrapUnique(shared_source)));
}

URLDataManagerBackend::~URLDataManagerBackend() = default;

URLDataManagerBackend* URLDataManagerBackend::GetForBrowserContext(
    BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context->GetUserData(kURLDataManagerBackendKeyName)) {
    context->SetUserData(kURLDataManagerBackendKeyName,
                         std::make_unique<URLDataManagerBackend>());
  }
  return static_cast<URLDataManagerBackend*>(
      context->GetUserData(kURLDataManagerBackendKeyName));
}

void URLDataManagerBackend::AddDataSource(URLDataSourceImpl* source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!source->source()->ShouldReplaceExistingSource()) {
    auto i = data_sources_.find(source->source_name());
    if (i != data_sources_.end())
      return;
  }
  data_sources_[source->source_name()] = source;
  source->backend_ = weak_factory_.GetWeakPtr();
}

void URLDataManagerBackend::UpdateWebUIDataSource(
    const std::string& source_name,
    const base::DictionaryValue& update) {
  auto it = data_sources_.find(source_name);
  if (it == data_sources_.end() || !it->second->IsWebUIDataSourceImpl()) {
    NOTREACHED();
    return;
  }
  static_cast<WebUIDataSourceImpl*>(it->second.get())
      ->AddLocalizedStrings(update);
}

URLDataSourceImpl* URLDataManagerBackend::GetDataSourceFromURL(
    const GURL& url) {
  // chrome-untrusted:// sources keys are of the form "chrome-untrusted://host".
  if (url.scheme() == kChromeUIUntrustedScheme) {
    auto i = data_sources_.find(url.GetOrigin().spec());
    if (i == data_sources_.end())
      return nullptr;
    return i->second.get();
  }

  // The input usually looks like: chrome://source_name/extra_bits?foo
  // so do a lookup using the host of the URL.
  auto i = data_sources_.find(url.host());
  if (i != data_sources_.end())
    return i->second.get();

  // No match using the host of the URL, so do a lookup using the scheme for
  // URLs on the form source_name://extra_bits/foo .
  i = data_sources_.find(url.scheme() + "://");
  if (i != data_sources_.end())
    return i->second.get();

  // No matches found, so give up.
  return nullptr;
}

scoped_refptr<net::HttpResponseHeaders> URLDataManagerBackend::GetHeaders(
    URLDataSourceImpl* source_impl,
    const std::string& path,
    const std::string& origin) {
  // Set the headers so that requests serviced by ChromeURLDataManager return a
  // status code of 200. Without this they return a 0, which makes the status
  // indistiguishable from other error types. Instant relies on getting a 200.
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  if (!source_impl)
    return headers;

  URLDataSource* source = source_impl->source();
  // Determine the least-privileged content security policy header, if any,
  // that is compatible with a given WebUI URL, and append it to the existing
  // response headers.
  if (source->ShouldAddContentSecurityPolicy()) {
    std::string csp_header;
    csp_header.append(source->GetContentSecurityPolicyChildSrc());
    csp_header.append(source->GetContentSecurityPolicyDefaultSrc());
    csp_header.append(source->GetContentSecurityPolicyImgSrc());
    csp_header.append(source->GetContentSecurityPolicyObjectSrc());
    csp_header.append(source->GetContentSecurityPolicyScriptSrc());
    csp_header.append(source->GetContentSecurityPolicyStyleSrc());
    csp_header.append(source->GetContentSecurityPolicyWorkerSrc());
    // TODO(crbug.com/1051745): Both CSP frame ancestors and XFO headers may be
    // added to the response but frame ancestors would take precedence. In the
    // future, XFO will be removed so when that happens remove the check and
    // always add frame ancestors.
    if (source->ShouldDenyXFrameOptions())
      csp_header.append(source->GetContentSecurityPolicyFrameAncestors());
    headers->SetHeader(kChromeURLContentSecurityPolicyHeaderName, csp_header);
  }

  if (source->ShouldDenyXFrameOptions()) {
    headers->SetHeader(kChromeURLXFrameOptionsHeaderName,
                       kChromeURLXFrameOptionsHeaderValue);
  }

  if (base::FeatureList::IsEnabled(features::kWebUIReportOnlyTrustedTypes))
    headers->SetHeader(kChromeURLContentSecurityPolicyReportOnlyHeaderName,
                       kChromeURLContentSecurityPolicyReportOnlyHeaderValue);

  if (!source->AllowCaching())
    headers->SetHeader("Cache-Control", "no-cache");

  std::string mime_type = source->GetMimeType(path);
  if (source->ShouldServeMimeTypeAsContentTypeHeader() && !mime_type.empty())
    headers->SetHeader(net::HttpRequestHeaders::kContentType, mime_type);

  if (!origin.empty()) {
    std::string header = source->GetAccessControlAllowOriginForOrigin(origin);
    DCHECK(header.empty() || header == origin || header == "*" ||
           header == "null");
    if (!header.empty()) {
      headers->SetHeader("Access-Control-Allow-Origin", header);
      headers->SetHeader("Vary", "Origin");
    }
  }

  return headers;
}

bool URLDataManagerBackend::CheckURLIsValid(const GURL& url) {
  std::vector<std::string> additional_schemes;
  DCHECK(url.SchemeIs(kChromeUIScheme) ||
         url.SchemeIs(kChromeUIUntrustedScheme) ||
         (GetContentClient()->browser()->GetAdditionalWebUISchemes(
              &additional_schemes),
          SchemeIsInSchemes(url.scheme(), additional_schemes)));

  if (!url.is_valid()) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool URLDataManagerBackend::IsValidNetworkErrorCode(int error_code) {
  std::unique_ptr<base::DictionaryValue> error_codes = net::GetNetConstants();
  const base::DictionaryValue* net_error_codes_dict = nullptr;

  for (base::DictionaryValue::Iterator itr(*error_codes); !itr.IsAtEnd();
       itr.Advance()) {
    if (itr.key() == kNetworkErrorKey) {
      itr.value().GetAsDictionary(&net_error_codes_dict);
      break;
    }
  }

  if (net_error_codes_dict != nullptr) {
    for (base::DictionaryValue::Iterator itr(*net_error_codes_dict);
         !itr.IsAtEnd(); itr.Advance()) {
      int net_error_code;
      itr.value().GetAsInteger(&net_error_code);
      if (error_code == net_error_code)
        return true;
    }
  }
  return false;
}

std::vector<std::string> URLDataManagerBackend::GetWebUISchemes() {
  std::vector<std::string> schemes;
  schemes.push_back(kChromeUIScheme);
  schemes.push_back(kChromeUIUntrustedScheme);
  GetContentClient()->browser()->GetAdditionalWebUISchemes(&schemes);
  return schemes;
}

}  // namespace content
