// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/content_browser_client.h"
#include "content/shell/browser/shell_speech_recognition_manager_delegate.h"

namespace content {

class ShellBrowserContext;
class ShellBrowserMainParts;
class ShellResourceDispatcherHostDelegate;

class ShellContentBrowserClient : public ContentBrowserClient {
 public:
  // Gets the current instance.
  static ShellContentBrowserClient* Get();

  static void SetSwapProcessesForRedirect(bool swap);

  ShellContentBrowserClient();
  virtual ~ShellContentBrowserClient();

  // ContentBrowserClient overrides.
  virtual BrowserMainParts* CreateBrowserMainParts(
      const MainFunctionParams& parameters) override;
  virtual void RenderProcessWillLaunch(RenderProcessHost* host) override;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      BrowserContext* browser_context,
      ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors) override;
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      BrowserContext* browser_context,
      const base::FilePath& partition_path,
      bool in_memory,
      ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors) override;
  virtual bool IsHandledURL(const GURL& url) override;
  virtual void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                              int child_process_id) override;
  virtual void OverrideWebkitPrefs(RenderViewHost* render_view_host,
                                   const GURL& url,
                                   WebPreferences* prefs) override;
  virtual void ResourceDispatcherHostCreated() override;
  virtual AccessTokenStore* CreateAccessTokenStore() override;
  virtual std::string GetDefaultDownloadName() override;
  virtual WebContentsViewDelegate* GetWebContentsViewDelegate(
      WebContents* web_contents) override;
  virtual QuotaPermissionContext* CreateQuotaPermissionContext() override;
  virtual SpeechRecognitionManagerDelegate*
      GetSpeechRecognitionManagerDelegate() override;
  virtual net::NetLog* GetNetLog() override;
  virtual bool ShouldSwapProcessesForRedirect(ResourceContext* resource_context,
                                              const GURL& current_url,
                                              const GURL& new_url) override;
  virtual DevToolsManagerDelegate* GetDevToolsManagerDelegate() override;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  virtual void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      FileDescriptorInfo* mappings) override;
#endif
#if defined(OS_WIN)
  virtual void PreSpawnRenderer(sandbox::TargetPolicy* policy,
                                bool* success) override;
#endif

  ShellBrowserContext* browser_context();
  ShellBrowserContext* off_the_record_browser_context();
  ShellResourceDispatcherHostDelegate* resource_dispatcher_host_delegate() {
    return resource_dispatcher_host_delegate_.get();
  }
  ShellBrowserMainParts* shell_browser_main_parts() {
    return shell_browser_main_parts_;
  }

 private:
  ShellBrowserContext* ShellBrowserContextForBrowserContext(
      BrowserContext* content_browser_context);

  scoped_ptr<ShellResourceDispatcherHostDelegate>
      resource_dispatcher_host_delegate_;

  ShellBrowserMainParts* shell_browser_main_parts_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
