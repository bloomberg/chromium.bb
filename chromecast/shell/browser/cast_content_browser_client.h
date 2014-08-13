// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SHELL_BROWSER_CAST_CONTENT_BROWSER_CLIENT_H_
#define CHROMECAST_SHELL_BROWSER_CAST_CONTENT_BROWSER_CLIENT_H_

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
      const content::MainFunctionParams& parameters) OVERRIDE;
  virtual void RenderProcessWillLaunch(
      content::RenderProcessHost* host) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::BrowserContext* browser_context,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors)
      OVERRIDE;
  virtual bool IsHandledURL(const GURL& url) OVERRIDE;
  virtual void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                              int child_process_id) OVERRIDE;
  virtual content::AccessTokenStore* CreateAccessTokenStore() OVERRIDE;
  virtual void OverrideWebkitPrefs(content::RenderViewHost* render_view_host,
                                   const GURL& url,
                                   content::WebPreferences* prefs) OVERRIDE;
  virtual std::string GetApplicationLocale() OVERRIDE;
  virtual void AllowCertificateError(
      int render_process_id,
      int render_view_id,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      content::ResourceType resource_type,
      bool overridable,
      bool strict_enforcement,
      const base::Callback<void(bool)>& callback,
      content::CertificateRequestResultType* result) OVERRIDE;
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
      bool* no_javascript_access) OVERRIDE;
  virtual void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      std::vector<content::FileDescriptorInfo>* mappings) OVERRIDE;

 private:
  // Note: BrowserMainLoop holds ownership of CastBrowserMainParts after it is
  // created.
  CastBrowserMainParts* shell_browser_main_parts_;
  scoped_ptr<URLRequestContextFactory> url_request_context_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastContentBrowserClient);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_SHELL_BROWSER_CAST_CONTENT_BROWSER_CLIENT_H_
