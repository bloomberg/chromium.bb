// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/shell/browser/cast_content_browser_client.h"

#include "base/command_line.h"
#include "chromecast/shell/browser/cast_browser_context.h"
#include "chromecast/shell/browser/cast_browser_main_parts.h"
#include "chromecast/shell/browser/geolocation/cast_access_token_store.h"
#include "chromecast/shell/browser/url_request_context_factory.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"

namespace chromecast {
namespace shell {

CastContentBrowserClient::CastContentBrowserClient()
    : url_request_context_factory_(new URLRequestContextFactory()) {
}

CastContentBrowserClient::~CastContentBrowserClient() {
}

content::BrowserMainParts* CastContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  shell_browser_main_parts_.reset(
      new CastBrowserMainParts(parameters, url_request_context_factory_.get()));
  return shell_browser_main_parts_.get();
}

void CastContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
}

net::URLRequestContextGetter* CastContentBrowserClient::CreateRequestContext(
    content::BrowserContext* browser_context,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return url_request_context_factory_->CreateMainGetter(
      browser_context,
      protocol_handlers,
      request_interceptors.Pass());
}

bool CastContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid())
    return false;

  static const char* const kProtocolList[] = {
      url::kBlobScheme,
      url::kFileSystemScheme,
      content::kChromeUIScheme,
      content::kChromeDevToolsScheme,
      url::kDataScheme,
  };

  const std::string& scheme = url.scheme();
  for (size_t i = 0; i < arraysize(kProtocolList); ++i) {
    if (scheme == kProtocolList[i])
      return true;
  }
  return false;
}

void CastContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {

  std::string process_type =
      command_line->GetSwitchValueNative(switches::kProcessType);
  // Renderer process comamndline
  if (process_type == switches::kRendererProcess) {
    // Any browser command-line switches that should be propagated to
    // the renderer go here.
  }
}

content::AccessTokenStore* CastContentBrowserClient::CreateAccessTokenStore() {
  return new CastAccessTokenStore(shell_browser_main_parts_->browser_context());
}

void CastContentBrowserClient::OverrideWebkitPrefs(
    content::RenderViewHost* render_view_host,
    const GURL& url,
    content::WebPreferences* prefs) {
  prefs->allow_scripts_to_close_windows = true;
  // TODO(lcwu): http://crbug.com/391089. This pref is set to true by default
  // because some content providers such as YouTube use plain http requests
  // to retrieve media data chunks while running in a https page. This pref
  // should be disabled once all the content providers are no longer doing that.
  prefs->allow_running_insecure_content = true;
}

std::string CastContentBrowserClient::GetApplicationLocale() {
  return "en-US";
}

void CastContentBrowserClient::AllowCertificateError(
    int render_process_id,
    int render_view_id,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    content::ResourceType resource_type,
    bool overridable,
    bool strict_enforcement,
    const base::Callback<void(bool)>& callback,
    content::CertificateRequestResultType* result) {
  // Allow developers to override certificate errors.
  // Otherwise, any fatal certificate errors will cause an abort.
  *result = content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
  return;
}

bool CastContentBrowserClient::CanCreateWindow(
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const GURL& source_origin,
    WindowContainerType container_type,
    const GURL& target_url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    const blink::WebWindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    content::ResourceContext* context,
    int render_process_id,
    int opener_id,
    bool* no_javascript_access) {
  *no_javascript_access = true;
  return false;
}

void CastContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    std::vector<content::FileDescriptorInfo>* mappings) {
}

}  // namespace shell
}  // namespace chromecast
