// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "content/public/browser/file_descriptor_info.h"
#include "content/public/common/socket_permission_request.h"
#include "content/public/common/content_client.h"
#include "content/public/common/window_container_type.h"
#include "net/base/mime_util.h"
#include "net/cookies/canonical_cookie.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPresenter.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "base/posix/global_descriptors.h"
#endif


class CommandLine;
class FilePath;
class GURL;

namespace webkit_glue {
struct WebPreferences;
}

namespace crypto {
class CryptoModuleBlockingPasswordDelegate;
}

namespace gfx {
class ImageSkia;
}

namespace net {
class CookieOptions;
class HttpNetworkSession;
class NetLog;
class SSLCertRequestInfo;
class SSLInfo;
class URLRequest;
class URLRequestContext;
class X509Certificate;
}

namespace content {

class AccessTokenStore;
class BrowserChildProcessHost;
class BrowserContext;
class BrowserMainParts;
class BrowserPpapiHost;
class BrowserURLHandler;
class MediaObserver;
class QuotaPermissionContext;
class RenderProcessHost;
class RenderViewHost;
class RenderViewHostDelegateView;
class ResourceContext;
class SiteInstance;
class SpeechInputManagerDelegate;
class SpeechRecognitionManagerDelegate;
class WebContents;
class WebContentsView;
class WebContentsViewDelegate;
class WebUIControllerFactory;
struct MainFunctionParams;
struct ShowDesktopNotificationHostMsgParams;

// Embedder API (or SPI) for participating in browser logic, to be implemented
// by the client of the content browser. See ChromeContentBrowserClient for the
// principal implementation. The methods are assumed to be called on the UI
// thread unless otherwise specified. Use this "escape hatch" sparingly, to
// avoid the embedder interface ballooning and becoming very specific to Chrome.
// (Often, the call out to the client can happen in a different part of the code
// that either already has a hook out to the embedder, or calls out to one of
// the observer interfaces.)
class CONTENT_EXPORT ContentBrowserClient {
 public:
  virtual ~ContentBrowserClient() {}

  // Allows the embedder to set any number of custom BrowserMainParts
  // implementations for the browser startup code. See comments in
  // browser_main_parts.h.
  virtual BrowserMainParts* CreateBrowserMainParts(
      const MainFunctionParams& parameters);

  // Allows an embedder to return their own WebContentsView implementation.
  // Return NULL to let the default one for the platform be created. Otherwise
  // |render_view_host_delegate_view| also needs to be provided, and it is
  // owned by the embedder.
  virtual WebContentsView* OverrideCreateWebContentsView(
      WebContents* web_contents,
      RenderViewHostDelegateView** render_view_host_delegate_view);

  // If content creates the WebContentsView implementation, it will ask the
  // embedder to return an (optional) delegate to customize it. The view will
  // own the delegate.
  virtual WebContentsViewDelegate* GetWebContentsViewDelegate(
      WebContents* web_contents);

  // Notifies that a new RenderHostView has been created.
  virtual void RenderViewHostCreated(RenderViewHost* render_view_host) {}

  // Notifies that a RenderProcessHost has been created. This is called before
  // the content layer adds its own BrowserMessageFilters, so that the
  // embedder's IPC filters have priority.
  virtual void RenderProcessHostCreated(RenderProcessHost* host) {}

  // Notifies that a BrowserChildProcessHost has been created.
  virtual void BrowserChildProcessHostCreated(BrowserChildProcessHost* host) {}

  // Gets the WebUIControllerFactory which will be responsible for generating
  // WebUIs. Can return NULL if the embedder doesn't need WebUI support.
  virtual WebUIControllerFactory* GetWebUIControllerFactory();

  // Get the effective URL for the given actual URL, to allow an embedder to
  // group different url schemes in the same SiteInstance.
  virtual GURL GetEffectiveURL(BrowserContext* browser_context,
                               const GURL& url);

  // Returns whether all instances of the specified effective URL should be
  // rendered by the same process, rather than using process-per-site-instance.
  virtual bool ShouldUseProcessPerSite(BrowserContext* browser_context,
                                       const GURL& effective_url);

  // Returns whether a specified URL is handled by the embedder's internal
  // protocol handlers.
  virtual bool IsHandledURL(const GURL& url);

  // Returns whether a new view for a given |site_url| can be launched in a
  // given |process_host|.
  virtual bool IsSuitableHost(RenderProcessHost* process_host,
                              const GURL& site_url);

  // Returns whether a new process should be created or an existing one should
  // be reused based on the URL we want to load. This should return false,
  // unless there is a good reason otherwise.
  virtual bool ShouldTryToUseExistingProcessHost(
      BrowserContext* browser_context, const GURL& url);

  // Called when a site instance is first associated with a process.
  virtual void SiteInstanceGotProcess(SiteInstance* site_instance) {}

  // Called from a site instance's destructor.
  virtual void SiteInstanceDeleting(SiteInstance* site_instance) {}

  // Returns true if for the navigation from |current_url| to |new_url|,
  // processes should be swapped (even if we are in a process model that
  // doesn't usually swap).
  virtual bool ShouldSwapProcessesForNavigation(const GURL& current_url,
                                                const GURL& new_url);

  // Returns true if the given navigation redirect should cause a renderer
  // process swap.
  // This is called on the IO thread.
  virtual bool ShouldSwapProcessesForRedirect(ResourceContext* resource_context,
                                              const GURL& current_url,
                                              const GURL& new_url);

  // See CharacterEncoding's comment.
  virtual std::string GetCanonicalEncodingNameByAliasName(
      const std::string& alias_name);

  // Allows the embedder to pass extra command line flags.
  // switches::kProcessType will already be set at this point.
  virtual void AppendExtraCommandLineSwitches(CommandLine* command_line,
                                              int child_process_id) {}

  // Returns the locale used by the application.
  // This is called on the UI and IO threads.
  virtual std::string GetApplicationLocale();

  // Returns the languages used in the Accept-Languages HTTP header.
  // (Not called GetAcceptLanguages so it doesn't clash with win32).
  virtual std::string GetAcceptLangs(BrowserContext* context);

  // Returns the default favicon.  The callee doesn't own the given bitmap.
  virtual gfx::ImageSkia* GetDefaultFavicon();

  // Allow the embedder to control if an AppCache can be used for the given url.
  // This is called on the IO thread.
  virtual bool AllowAppCache(const GURL& manifest_url,
                             const GURL& first_party,
                             ResourceContext* context);

  // Allow the embedder to control if the given cookie can be read.
  // This is called on the IO thread.
  virtual bool AllowGetCookie(const GURL& url,
                              const GURL& first_party,
                              const net::CookieList& cookie_list,
                              ResourceContext* context,
                              int render_process_id,
                              int render_view_id);

  // Allow the embedder to control if the given cookie can be set.
  // This is called on the IO thread.
  virtual bool AllowSetCookie(const GURL& url,
                              const GURL& first_party,
                              const std::string& cookie_line,
                              ResourceContext* context,
                              int render_process_id,
                              int render_view_id,
                              net::CookieOptions* options);

  // This is called on the IO thread.
  virtual bool AllowSaveLocalState(ResourceContext* context);

  // Allow the embedder to control if access to web database by a shared worker
  // is allowed. |render_views| is a vector of pairs of
  // RenderProcessID/RenderViewID of RenderViews that are using this worker.
  // This is called on the IO thread.
  virtual bool AllowWorkerDatabase(
      const GURL& url,
      const string16& name,
      const string16& display_name,
      unsigned long estimated_size,
      ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_views);

  // Allow the embedder to control if access to file system by a shared worker
  // is allowed.
  // This is called on the IO thread.
  virtual bool AllowWorkerFileSystem(
      const GURL& url,
      ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_views);

  // Allow the embedder to control if access to IndexedDB by a shared worker
  // is allowed.
  // This is called on the IO thread.
  virtual bool AllowWorkerIndexedDB(
      const GURL& url,
      const string16& name,
      ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_views);

  // Allow the embedder to override the request context based on the URL for
  // certain operations, like cookie access. Returns NULL to indicate the
  // regular request context should be used.
  // This is called on the IO thread.
  virtual net::URLRequestContext* OverrideRequestContextForURL(
      const GURL& url, ResourceContext* context);

  // Allow the embedder to specify a string version of the storage partition
  // config with a site.
  virtual std::string GetStoragePartitionIdForSite(
      content::BrowserContext* browser_context,
      const GURL& site);

  // Allows the embedder to provide a validation check for |partition_id|s.
  // This domain of valid entries should match the range of outputs for
  // GetStoragePartitionIdForChildProcess().
  virtual bool IsValidStoragePartitionId(BrowserContext* browser_context,
                                         const std::string& partition_id);

  // Allows the embedder to provide a storage parititon configuration for a
  // site. A storage partition configuration includes a domain of the embedder's
  // choice, an optional name within that domain, and whether the partition is
  // in-memory only.
  //
  // If |can_be_default| is false, the caller is telling the embedder that the
  // |site| is known to not be in the default partition. This is useful in
  // some shutdown situations where the bookkeeping logic that maps sites to
  // their partition configuration are no longer valid.
  //
  // The |partition_domain| is [a-z]* UTF-8 string, specifying the domain in
  // which partitions live (similar to namespace). Within a domain, partitions
  // can be uniquely identified by the combination of |partition_name| and
  // |in_memory| values. When a partition is not to be persisted, the
  // |in_memory| value must be set to true.
  virtual void GetStoragePartitionConfigForSite(
      content::BrowserContext* browser_context,
      const GURL& site,
      bool can_be_default,
      std::string* partition_domain,
      std::string* partition_name,
      bool* in_memory);

  // Create and return a new quota permission context.
  virtual QuotaPermissionContext* CreateQuotaPermissionContext();

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
      bool* cancel_request) {}

  // Selects a SSL client certificate and returns it to the |callback|. If no
  // certificate was selected NULL is returned to the |callback|.
  virtual void SelectClientCertificate(
      int render_process_id,
      int render_view_id,
      const net::HttpNetworkSession* network_session,
      net::SSLCertRequestInfo* cert_request_info,
      const base::Callback<void(net::X509Certificate*)>& callback) {}

  // Adds a new installable certificate or private key.
  // Typically used to install an X.509 user certificate.
  // Note that it's up to the embedder to verify that the data is
  // well-formed. |cert_data| will be NULL if file_size is 0.
  virtual void AddCertificate(
      net::URLRequest* request,
      net::CertificateMimeType cert_type,
      const void* cert_data,
      size_t cert_size,
      int render_process_id,
      int render_view_id) {}

  // Returns a a class to get notifications about media event. The embedder can
  // return NULL if they're not interested.
  virtual MediaObserver* GetMediaObserver();

  // Asks permission to show desktop notifications.
  virtual void RequestDesktopNotificationPermission(
      const GURL& source_origin,
      int callback_context,
      int render_process_id,
      int render_view_id) {}

  // Checks if the given page has permission to show desktop notifications.
  // This is called on the IO thread.
  virtual WebKit::WebNotificationPresenter::Permission
      CheckDesktopNotificationPermission(
          const GURL& source_url,
          ResourceContext* context,
          int render_process_id);

  // Show a desktop notification.  If |worker| is true, the request came from an
  // HTML5 web worker, otherwise, it came from a renderer.
  virtual void ShowDesktopNotification(
      const ShowDesktopNotificationHostMsgParams& params,
      int render_process_id,
      int render_view_id,
      bool worker) {}

  // Cancels a displayed desktop notification.
  virtual void CancelDesktopNotification(
      int render_process_id,
      int render_view_id,
      int notification_id) {}

  // Returns true if the given page is allowed to open a window of the given
  // type. If true is returned, |no_javascript_access| will indicate whether
  // the window that is created should be scriptable/in the same process.
  // This is called on the IO thread.
  virtual bool CanCreateWindow(
      const GURL& opener_url,
      const GURL& source_origin,
      WindowContainerType container_type,
      ResourceContext* context,
      int render_process_id,
      bool* no_javascript_access);

  // Returns a title string to use in the task manager for a process host with
  // the given URL, or the empty string to fall back to the default logic.
  // This is called on the IO thread.
  virtual std::string GetWorkerProcessTitle(const GURL& url,
                                            ResourceContext* context);

  // Notifies the embedder that the ResourceDispatcherHost has been created.
  // This is when it can optionally add a delegate.
  virtual void ResourceDispatcherHostCreated() {}

  // Allows the embedder to return a delegate for the SpeechRecognitionManager.
  // The delegate will be owned by the manager. It's valid to return NULL.
  virtual SpeechRecognitionManagerDelegate*
      GetSpeechRecognitionManagerDelegate();

  // Getters for common objects.
  virtual net::NetLog* GetNetLog();

  // Creates a new AccessTokenStore for gelocation.
  virtual AccessTokenStore* CreateAccessTokenStore();

  // Returns true if fast shutdown is possible.
  virtual bool IsFastShutdownPossible();

  // Called by WebContents to override the WebKit preferences that are used by
  // the renderer. The content layer will add its own settings, and then it's up
  // to the embedder to update it if it wants.
  virtual void OverrideWebkitPrefs(RenderViewHost* render_view_host,
                                   const GURL& url,
                                   webkit_glue::WebPreferences* prefs) {}

  // Inspector setting was changed and should be persisted.
  virtual void UpdateInspectorSetting(RenderViewHost* rvh,
                                      const std::string& key,
                                      const std::string& value) {}

  // Clear the Inspector settings.
  virtual void ClearInspectorSettings(RenderViewHost* rvh) {}

  // Notifies that BrowserURLHandler has been created, so that the embedder can
  // optionally add their own handlers.
  virtual void BrowserURLHandlerCreated(BrowserURLHandler* handler) {}

  // Clears browser cache.
  virtual void ClearCache(RenderViewHost* rvh) {}

  // Clears browser cookies.
  virtual void ClearCookies(RenderViewHost* rvh) {}

  // Returns the default download directory.
  // This can be called on any thread.
  virtual FilePath GetDefaultDownloadDirectory();

  // Returns the default filename used in downloads when we have no idea what
  // else we should do with the file.
  virtual std::string GetDefaultDownloadName();

  // Notification that a pepper plugin has just been spawned. This allows the
  // embedder to add filters onto the host to implement interfaces.
  // This is called on the IO thread.
  virtual void DidCreatePpapiPlugin(BrowserPpapiHost* browser_host) {}

  // Gets the host for an external out-of-process plugin.
  virtual content::BrowserPpapiHost* GetExternalBrowserPpapiHost(
      int plugin_child_id);

  // Returns true if renderer processes can use Pepper TCP/UDP sockets from
  // the given origin and connection type.
  virtual bool AllowPepperSocketAPI(BrowserContext* browser_context,
                                    const GURL& url,
                                    const SocketPermissionRequest& params);

  // Returns the directory containing hyphenation dictionaries.
  virtual FilePath GetHyphenDictionaryDirectory();

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Populates |mappings| with all files that need to be mapped before launching
  // a child process.
  virtual void GetAdditionalMappedFilesForChildProcess(
      const CommandLine& command_line,
      int child_process_id,
      std::vector<FileDescriptorInfo>* mappings) {}
#endif

#if defined(OS_WIN)
  // Returns the name of the dll that contains cursors and other resources.
  virtual const wchar_t* GetResourceDllName();
#endif

#if defined(USE_NSS)
  // Return a delegate to authenticate and unlock |module|.
  // This is called on a worker thread.
  virtual
      crypto::CryptoModuleBlockingPasswordDelegate* GetCryptoPasswordDelegate(
          const GURL& url);
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_
