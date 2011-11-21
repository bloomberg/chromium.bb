// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOCK_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_BROWSER_MOCK_CONTENT_BROWSER_CLIENT_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/scoped_temp_dir.h"
#include "content/public/browser/content_browser_client.h"

namespace content {

// Base for unit tests that need to mock the ContentBrowserClient.
class MockContentBrowserClient : public ContentBrowserClient {
 public:
  MockContentBrowserClient();
  virtual ~MockContentBrowserClient();

  virtual BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* widget) OVERRIDE;
  virtual TabContentsView* CreateTabContentsView(
      TabContents* tab_contents) OVERRIDE;
  virtual void RenderViewHostCreated(
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderProcessHostCreated(
      RenderProcessHost* host) OVERRIDE;
  virtual void PluginProcessHostCreated(PluginProcessHost* host) OVERRIDE;
  virtual WebUIFactory* GetWebUIFactory() OVERRIDE;
  virtual GURL GetEffectiveURL(content::BrowserContext* browser_context,
                               const GURL& url) OVERRIDE;
  virtual bool ShouldUseProcessPerSite(BrowserContext* browser_context,
                                       const GURL& effective_url) OVERRIDE;
  virtual bool IsURLSameAsAnySiteInstance(const GURL& url) OVERRIDE;
  virtual bool IsSuitableHost(RenderProcessHost* process_host,
                              const GURL& site_url) OVERRIDE;
  virtual void SiteInstanceGotProcess(SiteInstance* site_instance) OVERRIDE;
  virtual void SiteInstanceDeleting(SiteInstance* site_instance) OVERRIDE;
  virtual bool ShouldSwapProcessesForNavigation(const GURL& current_url,
                                                const GURL& new_url) OVERRIDE;
  virtual std::string GetCanonicalEncodingNameByAliasName(
      const std::string& alias_name) OVERRIDE;
  virtual void AppendExtraCommandLineSwitches(CommandLine* command_line,
                                              int child_process_id) OVERRIDE;
  virtual std::string GetApplicationLocale() OVERRIDE;
  virtual std::string GetAcceptLangs(
      content::BrowserContext* context) OVERRIDE;
  virtual SkBitmap* GetDefaultFavicon() OVERRIDE;
  virtual bool AllowAppCache(const GURL& manifest_url,
                             const GURL& first_party,
                             const content::ResourceContext& context) OVERRIDE;
  virtual bool AllowGetCookie(const GURL& url,
                              const GURL& first_party,
                              const net::CookieList& cookie_list,
                              const content::ResourceContext& context,
                              int render_process_id,
                              int render_view_id) OVERRIDE;
  virtual bool AllowSetCookie(const GURL& url,
                              const GURL& first_party,
                              const std::string& cookie_line,
                              const content::ResourceContext& context,
                              int render_process_id,
                              int render_view_id,
                              net::CookieOptions* options) OVERRIDE;
  virtual bool AllowSaveLocalState(
      const content::ResourceContext& context) OVERRIDE;
  virtual bool AllowWorkerDatabase(
      int worker_route_id,
      const GURL& url,
      const string16& name,
      const string16& display_name,
      unsigned long estimated_size,
      WorkerProcessHost* worker_process_host) OVERRIDE;
  virtual bool AllowWorkerFileSystem(
      int worker_route_id,
      const GURL& url,
      WorkerProcessHost* worker_process_host) OVERRIDE;
  virtual net::URLRequestContext* OverrideRequestContextForURL(
      const GURL& url, const content::ResourceContext& context) OVERRIDE;
  virtual QuotaPermissionContext* CreateQuotaPermissionContext() OVERRIDE;
  virtual void OpenItem(const FilePath& path) OVERRIDE;
  virtual void ShowItemInFolder(const FilePath& path) OVERRIDE;
  virtual void AllowCertificateError(
      SSLCertErrorHandler* handler,
      bool overridable,
      const base::Callback<void(SSLCertErrorHandler*, bool)>& callback)
      OVERRIDE;
  virtual void SelectClientCertificate(
      int render_process_id,
      int render_view_id,
      SSLClientAuthHandler* handler) OVERRIDE;
  virtual void AddNewCertificate(
      net::URLRequest* request,
      net::X509Certificate* cert,
      int render_process_id,
      int render_view_id) OVERRIDE;
  virtual void RequestDesktopNotificationPermission(
      const GURL& source_origin,
      int callback_context,
      int render_process_id,
      int render_view_id) OVERRIDE;
  virtual WebKit::WebNotificationPresenter::Permission
      CheckDesktopNotificationPermission(
          const GURL& source_origin,
          const content::ResourceContext& context,
          int render_process_id) OVERRIDE;
  virtual void ShowDesktopNotification(
      const content::ShowDesktopNotificationHostMsgParams& params,
      int render_process_id,
      int render_view_id,
      bool worker) OVERRIDE;
  virtual void CancelDesktopNotification(
      int render_process_id,
      int render_view_id,
      int notification_id) OVERRIDE;
  virtual bool CanCreateWindow(
      const GURL& source_origin,
      WindowContainerType container_type,
      const content::ResourceContext& context,
      int render_process_id) OVERRIDE;
  virtual std::string GetWorkerProcessTitle(
      const GURL& url, const content::ResourceContext& context) OVERRIDE;
  virtual ResourceDispatcherHost* GetResourceDispatcherHost() OVERRIDE;
  virtual ui::Clipboard* GetClipboard() OVERRIDE;
  virtual MHTMLGenerationManager* GetMHTMLGenerationManager() OVERRIDE;
  virtual DevToolsManager* GetDevToolsManager() OVERRIDE;
  virtual net::NetLog* GetNetLog() OVERRIDE;
  virtual speech_input::SpeechInputManager* GetSpeechInputManager() OVERRIDE;
  virtual AccessTokenStore* CreateAccessTokenStore() OVERRIDE;
  virtual bool IsFastShutdownPossible() OVERRIDE;
  virtual WebPreferences GetWebkitPrefs(RenderViewHost* rvh) OVERRIDE;
  virtual void UpdateInspectorSetting(RenderViewHost* rvh,
                                      const std::string& key,
                                      const std::string& value) OVERRIDE;
  virtual void ClearInspectorSettings(RenderViewHost* rvh) OVERRIDE;
  virtual void BrowserURLHandlerCreated(BrowserURLHandler* handler) OVERRIDE;
  virtual void ClearCache(RenderViewHost* rvh)  OVERRIDE;
  virtual void ClearCookies(RenderViewHost* rvh)  OVERRIDE;
  virtual FilePath GetDefaultDownloadDirectory() OVERRIDE;
  virtual std::string GetDefaultDownloadName() OVERRIDE;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  virtual int GetCrashSignalFD(const CommandLine& command_line) OVERRIDE;
#endif

#if defined(OS_WIN)
  virtual const wchar_t* GetResourceDllName() OVERRIDE;
#endif

#if defined(USE_NSS)
  virtual
      crypto::CryptoModuleBlockingPasswordDelegate* GetCryptoPasswordDelegate(
          const GURL& url) OVERRIDE;
#endif

 private:
  // Temporary directory for GetDefaultDownloadDirectory.
  ScopedTempDir download_dir_;

  DISALLOW_COPY_AND_ASSIGN(MockContentBrowserClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOCK_CONTENT_BROWSER_CLIENT_H_
