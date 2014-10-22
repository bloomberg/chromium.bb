// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_browser_client.h"

#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_main_parts.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_network_delegate.h"
#include "chromecast/browser/devtools/cast_dev_tools_delegate.h"
#include "chromecast/browser/geolocation/cast_access_token_store.h"
#include "chromecast/browser/url_request_context_factory.h"
#include "chromecast/common/cast_paths.h"
#include "chromecast/common/global_descriptors.h"
#include "components/crash/app/breakpad_linux.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "net/ssl/ssl_cert_request_info.h"

#if defined(OS_ANDROID)
#include "chromecast/browser/android/external_video_surface_container_impl.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
#include "components/crash/browser/crash_dump_manager_android.h"
#endif  // defined(OS_ANDROID)

namespace chromecast {
namespace shell {

CastContentBrowserClient::CastContentBrowserClient()
    : url_request_context_factory_(new URLRequestContextFactory()) {
}

CastContentBrowserClient::~CastContentBrowserClient() {
  content::BrowserThread::DeleteSoon(
      content::BrowserThread::IO,
      FROM_HERE,
      url_request_context_factory_.release());
}

content::BrowserMainParts* CastContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  return new CastBrowserMainParts(parameters,
                                  url_request_context_factory_.get());
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
#if defined(OS_ANDROID)
      url::kFileScheme,
#endif  // defined(OS_ANDROID)
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

  // Renderer process command-line
  if (process_type == switches::kRendererProcess) {
    // Any browser command-line switches that should be propagated to
    // the renderer go here.
#if defined(OS_ANDROID)
    command_line->AppendSwitch(switches::kForceUseOverlayEmbeddedVideo);
#endif  // defined(OS_ANDROID)
  }
}

content::AccessTokenStore* CastContentBrowserClient::CreateAccessTokenStore() {
  return new CastAccessTokenStore(
      CastBrowserProcess::GetInstance()->browser_context());
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
  const std::string locale(base::i18n::GetConfiguredLocale());
  return locale.empty() ? "en-US" : locale;
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
    bool expired_previous_decision,
    const base::Callback<void(bool)>& callback,
    content::CertificateRequestResultType* result) {
  // Allow developers to override certificate errors.
  // Otherwise, any fatal certificate errors will cause an abort.
  *result = content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
  return;
}

void CastContentBrowserClient::SelectClientCertificate(
    int render_process_id,
    int render_view_id,
    net::SSLCertRequestInfo* cert_request_info,
    const base::Callback<void(net::X509Certificate*)>& callback) {
  GURL requesting_url("https://" + cert_request_info->host_and_port.ToString());

  if (!requesting_url.is_valid()) {
    LOG(ERROR) << "Invalid URL string: "
               << requesting_url.possibly_invalid_spec();
    callback.Run(NULL);
    return;
  }

  // In our case there are no relevant certs in the cert_request_info. The cert
  // we need to return (if permitted) is the Cast device cert, which we can
  // access directly through the ClientAuthSigner instance. However, we need to
  // be on the IO thread to determine whether the app is whitelisted to return
  // it, because CastNetworkDelegate is bound to the IO thread.
  // Subsequently, the callback must then itself be performed back here
  // on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &CastContentBrowserClient::SelectClientCertificateOnIOThread,
          base::Unretained(this),
          requesting_url),
      callback);
}

net::X509Certificate*
CastContentBrowserClient::SelectClientCertificateOnIOThread(
    GURL requesting_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  CastNetworkDelegate* network_delegate =
      url_request_context_factory_->app_network_delegate();
  if (network_delegate->IsWhitelisted(requesting_url, false)) {
    return CastNetworkDelegate::DeviceCert();
  } else {
    LOG(ERROR) << "Invalid host for client certificate request: "
               << requesting_url.host();
    return NULL;
  }
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

content::DevToolsManagerDelegate*
CastContentBrowserClient::GetDevToolsManagerDelegate() {
  return new CastDevToolsManagerDelegate();
}

void CastContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::FileDescriptorInfo* mappings) {
#if defined(OS_ANDROID)
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
  base::FilePath pak_file;
  CHECK(PathService::Get(FILE_CAST_PAK, &pak_file));
  base::File pak_with_flags(pak_file, flags);
  if (!pak_with_flags.IsValid()) {
    NOTREACHED() << "Failed to open file when creating renderer process: "
                 << "cast_shell.pak";
  }
  mappings->Transfer(
      kAndroidPakDescriptor,
      base::ScopedFD(pak_with_flags.TakePlatformFile()));

  if (breakpad::IsCrashReporterEnabled()) {
    base::File minidump_file(
        breakpad::CrashDumpManager::GetInstance()->CreateMinidumpFile(
            child_process_id));
    if (!minidump_file.IsValid()) {
      LOG(ERROR) << "Failed to create file for minidump, crash reporting will "
                 << "be disabled for this process.";
    } else {
      mappings->Transfer(kAndroidMinidumpDescriptor,
                         base::ScopedFD(minidump_file.TakePlatformFile()));
    }
  }
#endif  // defined(OS_ANDROID)
}

#if defined(OS_ANDROID) && defined(VIDEO_HOLE)
content::ExternalVideoSurfaceContainer*
CastContentBrowserClient::OverrideCreateExternalVideoSurfaceContainer(
    content::WebContents* web_contents) {
  return new ExternalVideoSurfaceContainerImpl(web_contents);
}
#endif  // defined(OS_ANDROID) && defined(VIDEO_HOLE)


}  // namespace shell
}  // namespace chromecast
