// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "content/public/common/content_client.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/common/window_container_type.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPresenter.h"

class CommandLine;
class FilePath;
class GURL;
class PluginProcessHost;
class ResourceDispatcherHost;
class SkBitmap;

namespace webkit_glue {
struct WebPreferences;
}

namespace content {
class AccessTokenStore;
class BrowserMainParts;
class BrowserURLHandler;
class RenderProcessHost;
class SiteInstance;
class SpeechRecognitionManagerDelegate;
class WebContents;
class WebContentsView;
struct MainFunctionParams;
struct ShowDesktopNotificationHostMsgParams;
}

namespace crypto {
class CryptoModuleBlockingPasswordDelegate;
}

namespace net {
class CookieList;
class CookieOptions;
class HttpNetworkSession;
class NetLog;
class SSLCertRequestInfo;
class SSLInfo;
class URLRequest;
class URLRequestContext;
class X509Certificate;
}

namespace ui {
class Clipboard;
}

namespace content {

class AccessTokenStore;
class BrowserChildProcessHost;
class BrowserContext;
class BrowserMainParts;
class MediaObserver;
class QuotaPermissionContext;
class RenderProcessHost;
class RenderViewHost;
class ResourceContext;
class SiteInstance;
class SpeechInputManagerDelegate;
class WebContents;
class WebContentsView;
class WebContentsViewDelegate;
class WebUIControllerFactory;
struct MainFunctionParams;
struct ShowDesktopNotificationHostMsgParams;

typedef base::Callback< void(const content::MediaStreamDevices&) >
    MediaResponseCallback;

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
  virtual BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) = 0;

  // Allows an embedder to return their own WebContentsView implementation.
  // Return NULL to let the default one for the platform be created.
  virtual WebContentsView* OverrideCreateWebContentsView(
      WebContents* web_contents) = 0;

  // If content creates the WebContentsView implementation, it will ask the
  // embedder to return an (optional) delegate to customize it. The view will
  // own the delegate.
  virtual WebContentsViewDelegate* GetWebContentsViewDelegate(
      WebContents* web_contents) = 0;

  // Notifies that a new RenderHostView has been created.
  virtual void RenderViewHostCreated(
      content::RenderViewHost* render_view_host) = 0;

  // Notifies that a RenderProcessHost has been created. This is called before
  // the content layer adds its own BrowserMessageFilters, so that the
  // embedder's IPC filters have priority.
  virtual void RenderProcessHostCreated(
      content::RenderProcessHost* host) = 0;

  // Notifies that a BrowserChildProcessHost has been created.
  virtual void BrowserChildProcessHostCreated(
      content::BrowserChildProcessHost* host) {}

  // Gets the WebUIControllerFactory which will be responsible for generating
  // WebUIs. Can return NULL if the embedder doesn't need WebUI support.
  virtual WebUIControllerFactory* GetWebUIControllerFactory() = 0;

  // Get the effective URL for the given actual URL, to allow an embedder to
  // group different url schemes in the same SiteInstance.
  virtual GURL GetEffectiveURL(BrowserContext* browser_context,
                               const GURL& url) = 0;

  // Returns whether all instances of the specified effective URL should be
  // rendered by the same process, rather than using process-per-site-instance.
  virtual bool ShouldUseProcessPerSite(BrowserContext* browser_context,
                                       const GURL& effective_url) = 0;

  // Returns whether a specified URL is handled by the embedder's internal
  // protocol handlers.
  virtual bool IsHandledURL(const GURL& url) = 0;

  // Returns whether a new view for a given |site_url| can be launched in a
  // given |process_host|.
  virtual bool IsSuitableHost(content::RenderProcessHost* process_host,
                              const GURL& site_url) = 0;

  // Returns whether a new process should be created or an existing one should
  // be reused based on the URL we want to load. This should return false,
  // unless there is a good reason otherwise.
  virtual bool ShouldTryToUseExistingProcessHost(
      BrowserContext* browser_context, const GURL& url) = 0;

  // Called when a site instance is first associated with a process.
  virtual void SiteInstanceGotProcess(
      content::SiteInstance* site_instance) = 0;

  // Called from a site instance's destructor.
  virtual void SiteInstanceDeleting(content::SiteInstance* site_instance) = 0;

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
  virtual std::string GetAcceptLangs(
      content::BrowserContext* context) = 0;

  // Returns the default favicon.  The callee doesn't own the given bitmap.
  virtual SkBitmap* GetDefaultFavicon() = 0;

  // Allow the embedder to control if an AppCache can be used for the given url.
  // This is called on the IO thread.
  virtual bool AllowAppCache(const GURL& manifest_url,
                             const GURL& first_party,
                             content::ResourceContext* context) = 0;

  // Allow the embedder to control if the given cookie can be read.
  // This is called on the IO thread.
  virtual bool AllowGetCookie(const GURL& url,
                              const GURL& first_party,
                              const net::CookieList& cookie_list,
                              content::ResourceContext* context,
                              int render_process_id,
                              int render_view_id) = 0;

  // Allow the embedder to control if the given cookie can be set.
  // This is called on the IO thread.
  virtual bool AllowSetCookie(const GURL& url,
                              const GURL& first_party,
                              const std::string& cookie_line,
                              content::ResourceContext* context,
                              int render_process_id,
                              int render_view_id,
                              net::CookieOptions* options) = 0;

  // This is called on the IO thread.
  virtual bool AllowSaveLocalState(content::ResourceContext* context) = 0;

  // Allow the embedder to control if access to web database by a shared worker
  // is allowed. |render_views| is a vector of pairs of
  // RenderProcessID/RenderViewID of RenderViews that are using this worker.
  // This is called on the IO thread.
  virtual bool AllowWorkerDatabase(
      const GURL& url,
      const string16& name,
      const string16& display_name,
      unsigned long estimated_size,
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_views) = 0;

  // Allow the embedder to control if access to file system by a shared worker
  // is allowed.
  // This is called on the IO thread.
  virtual bool AllowWorkerFileSystem(
      const GURL& url,
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_views) = 0;

  // Allow the embedder to control if access to IndexedDB by a shared worker
  // is allowed.
  // This is called on the IO thread.
  virtual bool AllowWorkerIndexedDB(
      const GURL& url,
      const string16& name,
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_views) = 0;

  // Allows the embedder to override the request context based on the URL for
  // certain operations, like cookie access. Returns NULL to indicate the
  // regular request context should be used.
  // This is called on the IO thread.
  virtual net::URLRequestContext* OverrideRequestContextForURL(
      const GURL& url, content::ResourceContext* context) = 0;

  // Create and return a new quota permission context.
  virtual QuotaPermissionContext* CreateQuotaPermissionContext() = 0;

  // Open the given file in the desktop's default manner.
  virtual void OpenItem(const FilePath& path) = 0;

  // Show the given file in a file manager. If possible, select the file.
  virtual void ShowItemInFolder(const FilePath& path) = 0;

  // Informs the embedder that a certificate error has occured.  If
  // |overridable| is true and if |strict_enforcement| is false, the user
  // can ignore the error and continue. The embedder can call the callback
  // asynchronously. If |cancel_request| is set to true, the request will be
  // cancelled immediately and the callback won't be run.
  virtual void AllowCertificateError(
      int render_process_id,
      int render_view_id,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool overridable,
      bool strict_enforcement,
      const base::Callback<void(bool)>& callback,
      bool* cancel_request) = 0;

  // Selects a SSL client certificate and returns it to the |callback|. If no
  // certificate was selected NULL is returned to the |callback|.
  virtual void SelectClientCertificate(
      int render_process_id,
      int render_view_id,
      const net::HttpNetworkSession* network_session,
      net::SSLCertRequestInfo* cert_request_info,
      const base::Callback<void(net::X509Certificate*)>& callback) = 0;

  // Adds a downloaded client cert. The embedder should ensure that there's
  // a private key for the cert, displays the cert to the user, and adds it upon
  // user approval. If the downloaded data could not be interpreted as a valid
  // certificate, |cert| will be NULL.
  virtual void AddNewCertificate(
      net::URLRequest* request,
      net::X509Certificate* cert,
      int render_process_id,
      int render_view_id) = 0;

  // Returns a a class to get notifications about media event. The embedder can
  // return NULL if they're not interested.
  virtual MediaObserver* GetMediaObserver() = 0;

  // Asks permission to use the camera and/or microphone. If permission is
  // granted, a call should be made to |callback| with the devices. If the
  // request is denied, a call should be made to |callback| with an empty list
  // of devices. |request| has the details of the request (e.g. which of audio
  // and/or video devices are requested, and lists of available devices).
  virtual void RequestMediaAccessPermission(
      const content::MediaStreamRequest* request,
      const MediaResponseCallback& callback) = 0;

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
          content::ResourceContext* context,
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
  // type. If true is returned, |no_javascript_access| will indicate whether
  // the window that is created should be scriptable/in the same process.
  // This is called on the IO thread.
  virtual bool CanCreateWindow(
      const GURL& opener_url,
      const GURL& source_origin,
      WindowContainerType container_type,
      content::ResourceContext* context,
      int render_process_id,
      bool* no_javascript_access) = 0;

  // Returns a title string to use in the task manager for a process host with
  // the given URL, or the empty string to fall back to the default logic.
  // This is called on the IO thread.
  virtual std::string GetWorkerProcessTitle(
      const GURL& url, content::ResourceContext* context) = 0;

  // Notifies the embedder that the ResourceDispatcherHost has been created.
  // This is when it can optionally add a delegate or ResourceQueueDelegates.
  virtual void ResourceDispatcherHostCreated() = 0;

  // Allows the embedder to return a delegate for the SpeechRecognitionManager.
  // The delegate will be owned by the manager. It's valid to return NULL.
  virtual SpeechRecognitionManagerDelegate*
      GetSpeechRecognitionManagerDelegate() = 0;

  // Getters for common objects.
  virtual ui::Clipboard* GetClipboard() = 0;
  virtual net::NetLog* GetNetLog() = 0;

  // Creates a new AccessTokenStore for gelocation.
  virtual AccessTokenStore* CreateAccessTokenStore() = 0;

  // Returns true if fast shutdown is possible.
  virtual bool IsFastShutdownPossible() = 0;

  // Called by WebContents to override the WebKit preferences that are used by
  // the renderer. The content layer will add its own settings, and then it's up
  // to the embedder to update it if it wants.
  virtual void OverrideWebkitPrefs(content::RenderViewHost* render_view_host,
                                   const GURL& url,
                                   webkit_glue::WebPreferences* prefs) = 0;

  // Inspector setting was changed and should be persisted.
  virtual void UpdateInspectorSetting(content::RenderViewHost* rvh,
                                      const std::string& key,
                                      const std::string& value) = 0;

  // Clear the Inspector settings.
  virtual void ClearInspectorSettings(content::RenderViewHost* rvh) = 0;

  // Notifies that BrowserURLHandler has been created, so that the embedder can
  // optionally add their own handlers.
  virtual void BrowserURLHandlerCreated(BrowserURLHandler* handler) = 0;

  // Clears browser cache.
  virtual void ClearCache(content::RenderViewHost* rvh) = 0;

  // Clears browser cookies.
  virtual void ClearCookies(content::RenderViewHost* rvh) = 0;

  // Returns the default download directory.
  // This can be called on any thread.
  virtual FilePath GetDefaultDownloadDirectory() = 0;

  // Returns the default filename used in downloads when we have no idea what
  // else we should do with the file.
  virtual std::string GetDefaultDownloadName() = 0;

  // Returns true if given origin can use TCP/UDP sockets.
  virtual bool AllowSocketAPI(BrowserContext* browser_context,
                              const GURL& url) = 0;

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
