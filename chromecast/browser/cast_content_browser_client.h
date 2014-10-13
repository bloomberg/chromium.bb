// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_CONTENT_BROWSER_CLIENT_H_
#define CHROMECAST_BROWSER_CAST_CONTENT_BROWSER_CLIENT_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/content_browser_client.h"

namespace chromecast {
namespace shell {

class CastBrowserMainParts;
class URLRequestContextFactory;

class CastContentBrowserClient: public content::ContentBrowserClient {
 public:
  CastContentBrowserClient();
  virtual ~CastContentBrowserClient();

  // content::ContentBrowserClient implementation:
  virtual content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) override;
  virtual void RenderProcessWillLaunch(
      content::RenderProcessHost* host) override;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::BrowserContext* browser_context,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors)
      override;
  virtual bool IsHandledURL(const GURL& url) override;
  virtual void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                              int child_process_id) override;
  virtual content::AccessTokenStore* CreateAccessTokenStore() override;
  virtual void OverrideWebkitPrefs(content::RenderViewHost* render_view_host,
                                   const GURL& url,
                                   content::WebPreferences* prefs) override;
  virtual std::string GetApplicationLocale() override;
  virtual void AllowCertificateError(
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
      content::CertificateRequestResultType* result) override;
  virtual void SelectClientCertificate(
      int render_process_id,
      int render_frame_id,
      net::SSLCertRequestInfo* cert_request_info,
      const base::Callback<void(net::X509Certificate*)>& callback) override;
  virtual bool CanCreateWindow(
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
      bool* no_javascript_access) override;
  virtual content::DevToolsManagerDelegate*
      GetDevToolsManagerDelegate() override;
  virtual void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::FileDescriptorInfo* mappings) override;
#if defined(OS_ANDROID) && defined(VIDEO_HOLE)
  virtual content::ExternalVideoSurfaceContainer*
  OverrideCreateExternalVideoSurfaceContainer(
      content::WebContents* web_contents) override;
#endif  // defined(OS_ANDROID) && defined(VIDEO_HOLE)

 private:
  net::X509Certificate* SelectClientCertificateOnIOThread(
      GURL requesting_url);

  scoped_ptr<URLRequestContextFactory> url_request_context_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastContentBrowserClient);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_CONTENT_BROWSER_CLIENT_H_
