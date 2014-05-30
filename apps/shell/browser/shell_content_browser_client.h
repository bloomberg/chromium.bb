// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
#define APPS_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_

#include "base/compiler_specific.h"
#include "content/public/browser/content_browser_client.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
}

namespace apps {
class ShellBrowserMainDelegate;
class ShellBrowserMainParts;

// Content module browser process support for app_shell.
class ShellContentBrowserClient : public content::ContentBrowserClient {
 public:
  explicit ShellContentBrowserClient(
      ShellBrowserMainDelegate* browser_main_delegate);
  virtual ~ShellContentBrowserClient();

  // Returns the single instance.
  static ShellContentBrowserClient* Get();

  // Returns the single browser context for app_shell.
  content::BrowserContext* GetBrowserContext();

  // content::ContentBrowserClient overrides.
  virtual content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) OVERRIDE;
  virtual void RenderProcessWillLaunch(
      content::RenderProcessHost* host) OVERRIDE;
  virtual bool ShouldUseProcessPerSite(content::BrowserContext* browser_context,
                                       const GURL& effective_url) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::BrowserContext* browser_context,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) OVERRIDE;
  // TODO(jamescook): Quota management?
  // TODO(jamescook): Speech recognition?
  virtual bool IsHandledURL(const GURL& url) OVERRIDE;
  virtual void SiteInstanceGotProcess(content::SiteInstance* site_instance)
      OVERRIDE;
  virtual void SiteInstanceDeleting(content::SiteInstance* site_instance)
      OVERRIDE;
  virtual void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                              int child_process_id) OVERRIDE;
  virtual void GetAdditionalAllowedSchemesForFileSystem(
      std::vector<std::string>* additional_schemes) OVERRIDE;

 private:
  // Returns the extension or app associated with |site_instance| or NULL.
  const extensions::Extension* GetExtension(
      content::SiteInstance* site_instance);

  // Owned by content::BrowserMainLoop.
  ShellBrowserMainParts* browser_main_parts_;

  // Owned by ShellBrowserMainParts.
  ShellBrowserMainDelegate* browser_main_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ShellContentBrowserClient);
};

}  // namespace apps

#endif  // APPS_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
