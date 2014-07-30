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
class ShellNotificationManager;
class ShellResourceDispatcherHostDelegate;

class ShellContentBrowserClient : public ContentBrowserClient {
 public:
  // Gets the current instance.
  static ShellContentBrowserClient* Get();

  static void SetSwapProcessesForRedirect(bool swap);

  ShellContentBrowserClient();
  virtual ~ShellContentBrowserClient();

  // Will be lazily created when running layout tests.
  ShellNotificationManager* GetShellNotificationManager();

  // ContentBrowserClient overrides.
  virtual BrowserMainParts* CreateBrowserMainParts(
      const MainFunctionParams& parameters) OVERRIDE;
  virtual void RenderProcessWillLaunch(RenderProcessHost* host) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      BrowserContext* browser_context,
      ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      BrowserContext* browser_context,
      const base::FilePath& partition_path,
      bool in_memory,
      ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors) OVERRIDE;
  virtual bool IsHandledURL(const GURL& url) OVERRIDE;
  virtual void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                              int child_process_id) OVERRIDE;
  virtual void OverrideWebkitPrefs(RenderViewHost* render_view_host,
                                   const GURL& url,
                                   WebPreferences* prefs) OVERRIDE;
  virtual void ResourceDispatcherHostCreated() OVERRIDE;
  virtual AccessTokenStore* CreateAccessTokenStore() OVERRIDE;
  virtual std::string GetDefaultDownloadName() OVERRIDE;
  virtual WebContentsViewDelegate* GetWebContentsViewDelegate(
      WebContents* web_contents) OVERRIDE;
  virtual QuotaPermissionContext* CreateQuotaPermissionContext() OVERRIDE;
  virtual void RequestDesktopNotificationPermission(
      const GURL& source_origin,
      RenderFrameHost* render_frame_host,
      const base::Callback<void(blink::WebNotificationPermission)>& callback)
          OVERRIDE;
  virtual blink::WebNotificationPermission
      CheckDesktopNotificationPermission(
          const GURL& source_url,
          ResourceContext* context,
          int render_process_id) OVERRIDE;
  virtual SpeechRecognitionManagerDelegate*
      GetSpeechRecognitionManagerDelegate() OVERRIDE;
  virtual net::NetLog* GetNetLog() OVERRIDE;
  virtual bool ShouldSwapProcessesForRedirect(ResourceContext* resource_context,
                                              const GURL& current_url,
                                              const GURL& new_url) OVERRIDE;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  virtual void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      std::vector<FileDescriptorInfo>* mappings) OVERRIDE;
#endif
#if defined(OS_WIN)
  virtual void PreSpawnRenderer(sandbox::TargetPolicy* policy,
                                bool* success) OVERRIDE;
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

  scoped_ptr<ShellNotificationManager> shell_notification_manager_;

  scoped_ptr<ShellResourceDispatcherHostDelegate>
      resource_dispatcher_host_delegate_;

  base::FilePath webkit_source_dir_;

  ShellBrowserMainParts* shell_browser_main_parts_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
