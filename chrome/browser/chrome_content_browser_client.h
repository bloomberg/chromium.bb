// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_H_
#define CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/content_browser_client.h"

class ChromeContentBrowserClientParts;

namespace base {
class CommandLine;
}

namespace content {
class QuotaPermissionContext;
}

namespace extensions {
class BrowserPermissionsPolicyDelegate;
}

namespace prerender {
class PrerenderTracker;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chrome {

class ChromeContentBrowserClient : public content::ContentBrowserClient {
 public:
  ChromeContentBrowserClient();
  virtual ~ChromeContentBrowserClient();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Notification that the application locale has changed. This allows us to
  // update our I/O thread cache of this value.
  static void SetApplicationLocale(const std::string& locale);

  virtual content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) OVERRIDE;
  virtual std::string GetStoragePartitionIdForSite(
      content::BrowserContext* browser_context,
      const GURL& site) OVERRIDE;
  virtual bool IsValidStoragePartitionId(
      content::BrowserContext* browser_context,
      const std::string& partition_id) OVERRIDE;
  virtual void GetStoragePartitionConfigForSite(
      content::BrowserContext* browser_context,
      const GURL& site,
      bool can_be_default,
      std::string* partition_domain,
      std::string* partition_name,
      bool* in_memory) OVERRIDE;
  virtual content::WebContentsViewDelegate* GetWebContentsViewDelegate(
      content::WebContents* web_contents) OVERRIDE;
  virtual void RenderProcessWillLaunch(
      content::RenderProcessHost* host) OVERRIDE;
  virtual bool ShouldUseProcessPerSite(content::BrowserContext* browser_context,
                                       const GURL& effective_url) OVERRIDE;
  virtual GURL GetEffectiveURL(content::BrowserContext* browser_context,
                               const GURL& url) OVERRIDE;
  virtual void GetAdditionalWebUISchemes(
      std::vector<std::string>* additional_schemes) OVERRIDE;
  virtual void GetAdditionalWebUIHostsToIgnoreParititionCheck(
      std::vector<std::string>* hosts) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::BrowserContext* browser_context,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      content::BrowserContext* browser_context,
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) OVERRIDE;
  virtual bool IsHandledURL(const GURL& url) OVERRIDE;
  virtual bool CanCommitURL(content::RenderProcessHost* process_host,
                            const GURL& url) OVERRIDE;
  virtual bool ShouldAllowOpenURL(content::SiteInstance* site_instance,
                                  const GURL& url) OVERRIDE;
  virtual bool IsSuitableHost(content::RenderProcessHost* process_host,
                              const GURL& site_url) OVERRIDE;
  virtual bool MayReuseHost(content::RenderProcessHost* process_host) OVERRIDE;
  virtual bool ShouldTryToUseExistingProcessHost(
      content::BrowserContext* browser_context, const GURL& url) OVERRIDE;
  virtual void SiteInstanceGotProcess(
      content::SiteInstance* site_instance) OVERRIDE;
  virtual void SiteInstanceDeleting(content::SiteInstance* site_instance)
      OVERRIDE;
  virtual bool ShouldSwapBrowsingInstancesForNavigation(
      content::SiteInstance* site_instance,
      const GURL& current_url,
      const GURL& new_url) OVERRIDE;
  virtual bool ShouldSwapProcessesForRedirect(
      content::ResourceContext* resource_context,
      const GURL& current_url,
      const GURL& new_url) OVERRIDE;
  virtual bool ShouldAssignSiteForURL(const GURL& url) OVERRIDE;
  virtual std::string GetCanonicalEncodingNameByAliasName(
      const std::string& alias_name) OVERRIDE;
  virtual void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                              int child_process_id) OVERRIDE;
  virtual std::string GetApplicationLocale() OVERRIDE;
  virtual std::string GetAcceptLangs(
      content::BrowserContext* context) OVERRIDE;
  virtual const gfx::ImageSkia* GetDefaultFavicon() OVERRIDE;
  virtual bool AllowAppCache(const GURL& manifest_url,
                             const GURL& first_party,
                             content::ResourceContext* context) OVERRIDE;
  virtual bool AllowGetCookie(const GURL& url,
                              const GURL& first_party,
                              const net::CookieList& cookie_list,
                              content::ResourceContext* context,
                              int render_process_id,
                              int render_frame_id) OVERRIDE;
  virtual bool AllowSetCookie(const GURL& url,
                              const GURL& first_party,
                              const std::string& cookie_line,
                              content::ResourceContext* context,
                              int render_process_id,
                              int render_frame_id,
                              net::CookieOptions* options) OVERRIDE;
  virtual bool AllowSaveLocalState(content::ResourceContext* context) OVERRIDE;
  virtual bool AllowWorkerDatabase(
      const GURL& url,
      const base::string16& name,
      const base::string16& display_name,
      unsigned long estimated_size,
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_frames) OVERRIDE;
  virtual void AllowWorkerFileSystem(
      const GURL& url,
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_frames,
      base::Callback<void(bool)> callback) OVERRIDE;
  virtual bool AllowWorkerIndexedDB(
      const GURL& url,
      const base::string16& name,
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_frames) OVERRIDE;
  virtual net::URLRequestContext* OverrideRequestContextForURL(
      const GURL& url, content::ResourceContext* context) OVERRIDE;
  virtual content::QuotaPermissionContext*
      CreateQuotaPermissionContext() OVERRIDE;
  virtual void AllowCertificateError(
      int render_process_id,
      int render_frame_id,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      content::ResourceType resource_type,
      bool overridable,
      bool strict_enforcement,
      bool expired_previous_decision,
      const base::Callback<void(bool)>& callback,
      content::CertificateRequestResultType* request) OVERRIDE;
  virtual void SelectClientCertificate(
      int render_process_id,
      int render_frame_id,
      const net::HttpNetworkSession* network_session,
      net::SSLCertRequestInfo* cert_request_info,
      const base::Callback<void(net::X509Certificate*)>& callback) OVERRIDE;
  virtual void AddCertificate(net::CertificateMimeType cert_type,
                              const void* cert_data,
                              size_t cert_size,
                              int render_process_id,
                              int render_frame_id) OVERRIDE;
  virtual content::MediaObserver* GetMediaObserver() OVERRIDE;
  virtual void RequestDesktopNotificationPermission(
      const GURL& source_origin,
      content::RenderFrameHost* render_frame_host,
      const base::Callback<void(blink::WebNotificationPermission)>& callback)
          OVERRIDE;
  virtual blink::WebNotificationPermission
      CheckDesktopNotificationPermission(
          const GURL& source_origin,
          content::ResourceContext* context,
          int render_process_id) OVERRIDE;
  virtual void ShowDesktopNotification(
      const content::ShowDesktopNotificationHostMsgParams& params,
      content::RenderFrameHost* render_frame_host,
      scoped_ptr<content::DesktopNotificationDelegate> delegate,
      base::Closure* cancel_callback) OVERRIDE;
  virtual void RequestGeolocationPermission(
      content::WebContents* web_contents,
      int bridge_id,
      const GURL& requesting_frame,
      bool user_gesture,
      base::Callback<void(bool)> result_callback,
      base::Closure* cancel_callback) OVERRIDE;
  virtual void RequestMidiSysExPermission(
      content::WebContents* web_contents,
      int bridge_id,
      const GURL& requesting_frame,
      bool user_gesture,
      base::Callback<void(bool)> result_callback,
      base::Closure* cancel_callback) OVERRIDE;
  virtual void DidUseGeolocationPermission(content::WebContents* web_contents,
                                           const GURL& frame_url,
                                           const GURL& main_frame_url) OVERRIDE;
  virtual void RequestProtectedMediaIdentifierPermission(
      content::WebContents* web_contents,
      const GURL& origin,
      base::Callback<void(bool)> result_callback,
      base::Closure* cancel_callback) OVERRIDE;
  virtual bool CanCreateWindow(const GURL& opener_url,
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
  virtual void ResourceDispatcherHostCreated() OVERRIDE;
  virtual content::SpeechRecognitionManagerDelegate*
      GetSpeechRecognitionManagerDelegate() OVERRIDE;
  virtual net::NetLog* GetNetLog() OVERRIDE;
  virtual content::AccessTokenStore* CreateAccessTokenStore() OVERRIDE;
  virtual bool IsFastShutdownPossible() OVERRIDE;
  virtual void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                                   const GURL& url,
                                   content::WebPreferences* prefs) OVERRIDE;
  virtual void BrowserURLHandlerCreated(
      content::BrowserURLHandler* handler) OVERRIDE;
  virtual void ClearCache(content::RenderViewHost* rvh) OVERRIDE;
  virtual void ClearCookies(content::RenderViewHost* rvh) OVERRIDE;
  virtual base::FilePath GetDefaultDownloadDirectory() OVERRIDE;
  virtual std::string GetDefaultDownloadName() OVERRIDE;
  virtual void DidCreatePpapiPlugin(
      content::BrowserPpapiHost* browser_host) OVERRIDE;
  virtual content::BrowserPpapiHost* GetExternalBrowserPpapiHost(
      int plugin_process_id) OVERRIDE;
  virtual bool AllowPepperSocketAPI(
      content::BrowserContext* browser_context,
      const GURL& url,
      bool private_api,
      const content::SocketPermissionRequest* params) OVERRIDE;
  virtual ui::SelectFilePolicy* CreateSelectFilePolicy(
      content::WebContents* web_contents) OVERRIDE;
  virtual void GetAdditionalAllowedSchemesForFileSystem(
      std::vector<std::string>* additional_schemes) OVERRIDE;
  virtual void GetURLRequestAutoMountHandlers(
      std::vector<storage::URLRequestAutoMountHandler>* handlers) OVERRIDE;
  virtual void GetAdditionalFileSystemBackends(
      content::BrowserContext* browser_context,
      const base::FilePath& storage_partition_path,
      ScopedVector<storage::FileSystemBackend>* additional_backends) OVERRIDE;
  virtual content::DevToolsManagerDelegate*
      GetDevToolsManagerDelegate() OVERRIDE;
  virtual bool IsPluginAllowedToCallRequestOSFileHandle(
      content::BrowserContext* browser_context,
      const GURL& url) OVERRIDE;
  virtual bool IsPluginAllowedToUseDevChannelAPIs(
      content::BrowserContext* browser_context,
      const GURL& url) OVERRIDE;
  virtual net::CookieStore* OverrideCookieStoreForRenderProcess(
      int render_process_id) OVERRIDE;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  virtual void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      std::vector<content::FileDescriptorInfo>* mappings) OVERRIDE;
#endif
#if defined(OS_WIN)
  virtual const wchar_t* GetResourceDllName() OVERRIDE;
  virtual void PreSpawnRenderer(sandbox::TargetPolicy* policy,
                                bool* success) OVERRIDE;
#endif

 private:
  friend class DisableWebRtcEncryptionFlagTest;

#if defined(ENABLE_WEBRTC)
  // Copies disable WebRTC encryption switch depending on the channel.
  static void MaybeCopyDisableWebRtcEncryptionSwitch(
      base::CommandLine* to_command_line,
      const base::CommandLine& from_command_line,
      VersionInfo::Channel channel);
#endif

  void FileSystemAccessed(
      const GURL& url,
      const std::vector<std::pair<int, int> >& render_frames,
      base::Callback<void(bool)> callback,
      bool allow);

#if defined(ENABLE_EXTENSIONS)
  void GuestPermissionRequestHelper(
      const GURL& url,
      const std::vector<std::pair<int, int> >& render_frames,
      base::Callback<void(bool)> callback,
      bool allow);

  static void RequestFileSystemPermissionOnUIThread(
      int render_process_id,
      int render_frame_id,
      const GURL& url,
      bool allowed_by_default,
      const base::Callback<void(bool)>& callback);
#endif

#if defined(ENABLE_PLUGINS)
  // Set of origins that can use TCP/UDP private APIs from NaCl.
  std::set<std::string> allowed_socket_origins_;
  // Set of origins that can get a handle for FileIO from NaCl.
  std::set<std::string> allowed_file_handle_origins_;
  // Set of origins that can use "dev chanel" APIs from NaCl, even on stable
  // versions of Chrome.
  std::set<std::string> allowed_dev_channel_origins_;
#endif

  // The prerender tracker used to determine whether a render process is used
  // for prerendering and an override cookie store must be provided.
  // This needs to be kept as a member rather than just looked up from
  // the profile due to initialization ordering, as well as due to threading.
  // It is initialized on the UI thread when the ResoureDispatcherHost is
  // created. It is used only the IO thread.
  prerender::PrerenderTracker* prerender_tracker_;

  // Vector of additional ChromeContentBrowserClientParts.
  // Parts are deleted in the reverse order they are added.
  std::vector<ChromeContentBrowserClientParts*> extra_parts_;

  base::WeakPtrFactory<ChromeContentBrowserClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentBrowserClient);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_H_
