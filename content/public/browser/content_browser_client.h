// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_
#pragma once

#include <string>
#include <vector>

#include "base/callback.h"
#include "content/public/common/window_container_type.h"
#include "content/public/common/content_client.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPresenter.h"

class AccessTokenStore;
class BrowserURLHandler;
class CommandLine;
class DevToolsManager;
class FilePath;
class GURL;
class MHTMLGenerationManager;
class PluginProcessHost;
class QuotaPermissionContext;
class RenderViewHost;
class RenderWidgetHost;
class RenderWidgetHostView;
class ResourceDispatcherHost;
class SiteInstance;
class SSLCertErrorHandler;
class SSLClientAuthHandler;
class SkBitmap;
class TabContents;
class TabContentsView;
class WorkerProcessHost;
struct WebPreferences;

namespace content {
class BrowserMainParts;
struct MainFunctionParams;
struct ShowDesktopNotificationHostMsgParams;
class RenderProcessHost;
}

namespace crypto {
class CryptoModuleBlockingPasswordDelegate;
}

namespace net {
class CookieList;
class CookieOptions;
class NetLog;
class URLRequest;
class URLRequestContext;
class X509Certificate;
}

namespace speech_input {
class SpeechInputManager;
}

namespace ui {
class Clipboard;
}

namespace content {

class BrowserContext;
class ResourceContext;
class WebUIFactory;

// Embedder API (or SPI) for participating in browser logic, to be implemented
// by the client of the content browser. See ChromeContentBrowserClient for the
// principal implementation. The methods are assumed to be called on the UI
// thread unless otherwise specified. Use this "escape hatch" sparingly, to
// avoid the embedder interface ballooning and becoming very specific to Chrome.
// (Often, the call out to the client can happen in a different part of the code
// that either already has a hook out to the embedder, or calls out to one of
// the observer interfaces.)
class ContentBrowserClient {
 public:
  virtual ~ContentBrowserClient() {}

  // Allows the embedder to set any number of custom BrowserMainParts
  // implementations for the browser startup code. See comments in
  // browser_main_parts.h.
  virtual void CreateBrowserMainParts(
      const content::MainFunctionParams& parameters,
      std::vector<BrowserMainParts*>* parts_list) = 0;

  // Platform-specific creator. Use this to construct new RenderWidgetHostViews
  // rather than using RenderWidgetHostViewWin & friends.
  //
  // This function must NOT size it, because the RenderView in the renderer
  // wouldn't have been created yet. The widget would set its "waiting for
  // resize ack" flag, and the ack would never come becasue no RenderView
  // received it.
  //
  // The RenderWidgetHost must already be created (because we can't know if it's
  // going to be a regular RenderWidgetHost or a RenderViewHost (a subclass).
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* widget) = 0;

  virtual TabContentsView* CreateTabContentsView(TabContents* tab_contents) = 0;

  // Notifies that a new RenderHostView has been created.
  virtual void RenderViewHostCreated(RenderViewHost* render_view_host) = 0;

  // Notifies that a RenderProcessHost has been created. This is called before
  // the content layer adds its own BrowserMessageFilters, so that the
  // embedder's IPC filters have priority.
  virtual void RenderProcessHostCreated(
      content::RenderProcessHost* host) = 0;

  // Notifies that a PluginProcessHost has been created. This is called
  // before the content layer adds its own message filters, so that the
  // embedder's IPC filters have priority.
  virtual void PluginProcessHostCreated(PluginProcessHost* host) = 0;

  // Gets the WebUIFactory which will be responsible for generating WebUIs.
  virtual WebUIFactory* GetWebUIFactory() = 0;

  // Get the effective URL for the given actual URL, to allow an embedder to
  // group different url schemes in the same SiteInstance.
  virtual GURL GetEffectiveURL(BrowserContext* browser_context,
                               const GURL& url) = 0;

  // Returns whether all instances of the specified effective URL should be
  // rendered by the same process, rather than using process-per-site-instance.
  virtual bool ShouldUseProcessPerSite(BrowserContext* browser_context,
                                       const GURL& effective_url) = 0;

  // Returns whether a specified URL is to be considered the same as any
  // SiteInstance.
  virtual bool IsURLSameAsAnySiteInstance(const GURL& url) = 0;

  // Returns whether a new view for a given |site_url| can be launched in a
  // given |process_host|.
  virtual bool IsSuitableHost(content::RenderProcessHost* process_host,
                              const GURL& site_url) = 0;

  // Called when a site instance is first associated with a process.
  virtual void SiteInstanceGotProcess(SiteInstance* site_instance) = 0;

  // Called from a site instance's destructor.
  virtual void SiteInstanceDeleting(SiteInstance* site_instance) = 0;

  // Returns true if for the navigation from |current_url| to |new_url|,
  // processes should be swapped (even if we are in a process model that
  // doesn't usually swap).
  virtual bool ShouldSwapProcessesForNavigation(const GURL& current_url,
                                                const GURL& new_url) = 0;

  // See CharacterEncoding's comment.
  virtual std::string GetCanonicalEncodingNameByAliasName(
      const std::string& alias_name) = 0;

  // Allows the embedder to pass extra command line flags.
  // switches::kProcessType will already be set at this point.
  virtual void AppendExtraCommandLineSwitches(CommandLine* command_line,
                                              int child_process_id) = 0;

  // Returns the locale used by the application.
  virtual std::string GetApplicationLocale() = 0;

  // Returns the languages used in the Accept-Languages HTTP header.
  // (Not called GetAcceptLanguages so it doesn't clash with win32).
  virtual std::string GetAcceptLangs(const TabContents* tab) = 0;

  // Returns the default favicon.  The callee doesn't own the given bitmap.
  virtual SkBitmap* GetDefaultFavicon() = 0;

  // Allow the embedder to control if an AppCache can be used for the given url.
  // This is called on the IO thread.
  virtual bool AllowAppCache(const GURL& manifest_url,
                             const GURL& first_party,
                             const content::ResourceContext& context) = 0;

  // Allow the embedder to control if the given cookie can be read.
  // This is called on the IO thread.
  virtual bool AllowGetCookie(const GURL& url,
                              const GURL& first_party,
                              const net::CookieList& cookie_list,
                              const content::ResourceContext& context,
                              int render_process_id,
                              int render_view_id) = 0;

  // Allow the embedder to control if the given cookie can be set.
  // This is called on the IO thread.
  virtual bool AllowSetCookie(const GURL& url,
                              const GURL& first_party,
                              const std::string& cookie_line,
                              const content::ResourceContext& context,
                              int render_process_id,
                              int render_view_id,
                              net::CookieOptions* options) = 0;

  // This is called on the IO thread.
  virtual bool AllowSaveLocalState(
      const content::ResourceContext& context) = 0;

  // Allow the embedder to control if access to web database by a worker is
  // allowed.
  // This is called on the IO thread.
  virtual bool AllowWorkerDatabase(int worker_route_id,
                                   const GURL& url,
                                   const string16& name,
                                   const string16& display_name,
                                   unsigned long estimated_size,
                                   WorkerProcessHost* worker_process_host) = 0;

  // Allow the embedder to control if access to file system by a worker is
  // allowed.
  // This is called on the IO thread.
  virtual bool AllowWorkerFileSystem(
      int worker_route_id,
      const GURL& url,
      WorkerProcessHost* worker_process_host) = 0;

  // Allows the embedder to override the request context based on the URL for
  // certain operations, like cookie access. Returns NULL to indicate the
  // regular request context should be used.
  // This is called on the IO thread.
  virtual net::URLRequestContext* OverrideRequestContextForURL(
      const GURL& url, const content::ResourceContext& context) = 0;

  // Create and return a new quota permission context.
  virtual QuotaPermissionContext* CreateQuotaPermissionContext() = 0;

  // Open the given file in the desktop's default manner.
  virtual void OpenItem(const FilePath& path) = 0;

  // Show the given file in a file manager. If possible, select the file.
  virtual void ShowItemInFolder(const FilePath& path) = 0;

  // Informs the embedder that a certificate error has occured.  If overridable
  // is true, the user can ignore the error and continue.  If it's false, then
  // the certificate error is severe and the user isn't allowed to proceed.  The
  // embedder can call the callback asynchronously.
  virtual void AllowCertificateError(
      SSLCertErrorHandler* handler,
      bool overridable,
      const base::Callback<void(SSLCertErrorHandler*, bool)>& callback) = 0;

  // Selects a SSL client certificate and returns it to the |handler|. If no
  // certificate was selected NULL is returned to the |handler|.
  virtual void SelectClientCertificate(
      int render_process_id,
      int render_view_id,
      SSLClientAuthHandler* handler) = 0;

  // Adds a newly-generated client cert. The embedder should ensure that there's
  // a private key for the cert, displays the cert to the user, and adds it upon
  // user approval.
  virtual void AddNewCertificate(
      net::URLRequest* request,
      net::X509Certificate* cert,
      int render_process_id,
      int render_view_id) = 0;

  // Asks permission to show desktop notifications.
  virtual void RequestDesktopNotificationPermission(
      const GURL& source_origin,
      int callback_context,
      int render_process_id,
      int render_view_id) = 0;

  // Checks if the given page has permission to show desktop notifications.
  // This is called on the IO thread.
  virtual WebKit::WebNotificationPresenter::Permission
      CheckDesktopNotificationPermission(
          const GURL& source_url,
          const content::ResourceContext& context,
          int render_process_id) = 0;

  // Show a desktop notification.  If |worker| is true, the request came from an
  // HTML5 web worker, otherwise, it came from a renderer.
  virtual void ShowDesktopNotification(
      const content::ShowDesktopNotificationHostMsgParams& params,
      int render_process_id,
      int render_view_id,
      bool worker) = 0;

  // Cancels a displayed desktop notification.
  virtual void CancelDesktopNotification(
      int render_process_id,
      int render_view_id,
      int notification_id) = 0;

  // Returns true if the given page is allowed to open a window of the given
  // type.
  // This is called on the IO thread.
  virtual bool CanCreateWindow(
      const GURL& source_origin,
      WindowContainerType container_type,
      const content::ResourceContext& context,
      int render_process_id) = 0;

  // Returns a title string to use in the task manager for a process host with
  // the given URL, or the empty string to fall back to the default logic.
  // This is called on the IO thread.
  virtual std::string GetWorkerProcessTitle(
      const GURL& url, const content::ResourceContext& context) = 0;

  // Getters for common objects.
  virtual ResourceDispatcherHost* GetResourceDispatcherHost() = 0;
  virtual ui::Clipboard* GetClipboard() = 0;
  virtual MHTMLGenerationManager* GetMHTMLGenerationManager() = 0;
  virtual DevToolsManager* GetDevToolsManager() = 0;
  virtual net::NetLog* GetNetLog() = 0;
  virtual speech_input::SpeechInputManager* GetSpeechInputManager() = 0;

  // Creates a new AccessTokenStore for gelocation.
  virtual AccessTokenStore* CreateAccessTokenStore() = 0;

  // Returns true if fast shutdown is possible.
  virtual bool IsFastShutdownPossible() = 0;

  // Returns the WebKit preferences that are used by the renderer.
  virtual WebPreferences GetWebkitPrefs(RenderViewHost* render_view_host) = 0;

  // Inspector setting was changed and should be persisted.
  virtual void UpdateInspectorSetting(RenderViewHost* rvh,
                                      const std::string& key,
                                      const std::string& value) = 0;

  // Clear the Inspector settings.
  virtual void ClearInspectorSettings(RenderViewHost* rvh) = 0;

  // Notifies that BrowserURLHandler has been created, so that the embedder can
  // optionally add their own handlers.
  virtual void BrowserURLHandlerCreated(BrowserURLHandler* handler) = 0;

  // Clears browser cache.
  virtual void ClearCache(RenderViewHost* rvh) = 0;

  // Clears browser cookies.
  virtual void ClearCookies(RenderViewHost* rvh) = 0;

  // Returns the default download directory.
  // This can be called on any thread.
  virtual FilePath GetDefaultDownloadDirectory() = 0;

  // Returns the default filename used in downloads when we have no idea what
  // else we should do with the file.
  virtual std::string GetDefaultDownloadName() = 0;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Can return an optional fd for crash handling, otherwise returns -1. The
  // passed |command_line| will be used to start the process in question.
  virtual int GetCrashSignalFD(const CommandLine& command_line) = 0;
#endif

#if defined(OS_WIN)
  // Returns the name of the dll that contains cursors and other resources.
  virtual const wchar_t* GetResourceDllName() = 0;
#endif

#if defined(USE_NSS)
  // Return a delegate to authenticate and unlock |module|.
  // This is called on a worker thread.
  virtual
      crypto::CryptoModuleBlockingPasswordDelegate* GetCryptoPasswordDelegate(
          const GURL& url) = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_
