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
      const content::MainFunctionParams& parameters) override;
  virtual std::string GetStoragePartitionIdForSite(
      content::BrowserContext* browser_context,
      const GURL& site) override;
  virtual bool IsValidStoragePartitionId(
      content::BrowserContext* browser_context,
      const std::string& partition_id) override;
  virtual void GetStoragePartitionConfigForSite(
      content::BrowserContext* browser_context,
      const GURL& site,
      bool can_be_default,
      std::string* partition_domain,
      std::string* partition_name,
      bool* in_memory) override;
  virtual content::WebContentsViewDelegate* GetWebContentsViewDelegate(
      content::WebContents* web_contents) override;
  virtual void RenderProcessWillLaunch(
      content::RenderProcessHost* host) override;
  virtual bool ShouldUseProcessPerSite(content::BrowserContext* browser_context,
                                       const GURL& effective_url) override;
  virtual GURL GetEffectiveURL(content::BrowserContext* browser_context,
                               const GURL& url) override;
  virtual void GetAdditionalWebUISchemes(
      std::vector<std::string>* additional_schemes) override;
  virtual void GetAdditionalWebUIHostsToIgnoreParititionCheck(
      std::vector<std::string>* hosts) override;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::BrowserContext* browser_context,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      content::BrowserContext* browser_context,
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  virtual bool IsHandledURL(const GURL& url) override;
  virtual bool CanCommitURL(content::RenderProcessHost* process_host,
                            const GURL& url) override;
  virtual bool ShouldAllowOpenURL(content::SiteInstance* site_instance,
                                  const GURL& url) override;
  virtual bool IsSuitableHost(content::RenderProcessHost* process_host,
                              const GURL& site_url) override;
  virtual bool MayReuseHost(content::RenderProcessHost* process_host) override;
  virtual bool ShouldTryToUseExistingProcessHost(
      content::BrowserContext* browser_context, const GURL& url) override;
  virtual void SiteInstanceGotProcess(
      content::SiteInstance* site_instance) override;
  virtual void SiteInstanceDeleting(content::SiteInstance* site_instance)
      override;
  virtual bool ShouldSwapBrowsingInstancesForNavigation(
      content::SiteInstance* site_instance,
      const GURL& current_url,
      const GURL& new_url) override;
  virtual bool ShouldSwapProcessesForRedirect(
      content::ResourceContext* resource_context,
      const GURL& current_url,
      const GURL& new_url) override;
  virtual bool ShouldAssignSiteForURL(const GURL& url) override;
  virtual std::string GetCanonicalEncodingNameByAliasName(
      const std::string& alias_name) override;
  virtual void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                              int child_process_id) override;
  virtual std::string GetApplicationLocale() override;
  virtual std::string GetAcceptLangs(
      content::BrowserContext* context) override;
  virtual const gfx::ImageSkia* GetDefaultFavicon() override;
  virtual bool AllowAppCache(const GURL& manifest_url,
                             const GURL& first_party,
                             content::ResourceContext* context) override;
  virtual bool AllowServiceWorker(const GURL& scope,
                                  const GURL& first_party,
                                  content::ResourceContext* context) override;
  virtual bool AllowGetCookie(const GURL& url,
                              const GURL& first_party,
                              const net::CookieList& cookie_list,
                              content::ResourceContext* context,
                              int render_process_id,
                              int render_frame_id) override;
  virtual bool AllowSetCookie(const GURL& url,
                              const GURL& first_party,
                              const std::string& cookie_line,
                              content::ResourceContext* context,
                              int render_process_id,
                              int render_frame_id,
                              net::CookieOptions* options) override;
  virtual bool AllowSaveLocalState(content::ResourceContext* context) override;
  virtual bool AllowWorkerDatabase(
      const GURL& url,
      const base::string16& name,
      const base::string16& display_name,
      unsigned long estimated_size,
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_frames) override;
  virtual void AllowWorkerFileSystem(
      const GURL& url,
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_frames,
      base::Callback<void(bool)> callback) override;
  virtual bool AllowWorkerIndexedDB(
      const GURL& url,
      const base::string16& name,
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_frames) override;
  virtual net::URLRequestContext* OverrideRequestContextForURL(
      const GURL& url, content::ResourceContext* context) override;
  virtual content::QuotaPermissionContext*
      CreateQuotaPermissionContext() override;
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
      content::CertificateRequestResultType* request) override;
  virtual void SelectClientCertificate(
      int render_process_id,
      int render_frame_id,
      net::SSLCertRequestInfo* cert_request_info,
      const base::Callback<void(net::X509Certificate*)>& callback) override;
  virtual void AddCertificate(net::CertificateMimeType cert_type,
                              const void* cert_data,
                              size_t cert_size,
                              int render_process_id,
                              int render_frame_id) override;
  virtual content::MediaObserver* GetMediaObserver() override;
  virtual void RequestDesktopNotificationPermission(
      const GURL& source_origin,
      content::RenderFrameHost* render_frame_host,
      const base::Callback<void(blink::WebNotificationPermission)>& callback)
          override;
  virtual blink::WebNotificationPermission
      CheckDesktopNotificationPermission(
          const GURL& source_origin,
          content::ResourceContext* context,
          int render_process_id) override;
  virtual void ShowDesktopNotification(
      const content::ShowDesktopNotificationHostMsgParams& params,
      content::RenderFrameHost* render_frame_host,
      scoped_ptr<content::DesktopNotificationDelegate> delegate,
      base::Closure* cancel_callback) override;
  virtual void RequestGeolocationPermission(
      content::WebContents* web_contents,
      int bridge_id,
      const GURL& requesting_frame,
      bool user_gesture,
      const base::Callback<void(bool)>& result_callback) override;
  virtual void CancelGeolocationPermissionRequest(
      content::WebContents* web_contents,
      int bridge_id,
      const GURL& requesting_frame) override;
  virtual void RequestMidiSysExPermission(
      content::WebContents* web_contents,
      int bridge_id,
      const GURL& requesting_frame,
      bool user_gesture,
      base::Callback<void(bool)> result_callback,
      base::Closure* cancel_callback) override;
  virtual void DidUseGeolocationPermission(content::WebContents* web_contents,
                                           const GURL& frame_url,
                                           const GURL& main_frame_url) override;
  virtual void RequestProtectedMediaIdentifierPermission(
      content::WebContents* web_contents,
      const GURL& origin,
      base::Callback<void(bool)> result_callback,
      base::Closure* cancel_callback) override;
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
                               bool* no_javascript_access) override;
  virtual void ResourceDispatcherHostCreated() override;
  virtual content::SpeechRecognitionManagerDelegate*
      GetSpeechRecognitionManagerDelegate() override;
  virtual net::NetLog* GetNetLog() override;
  virtual content::AccessTokenStore* CreateAccessTokenStore() override;
  virtual bool IsFastShutdownPossible() override;
  virtual void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                                   const GURL& url,
                                   content::WebPreferences* prefs) override;
  virtual void BrowserURLHandlerCreated(
      content::BrowserURLHandler* handler) override;
  virtual void ClearCache(content::RenderViewHost* rvh) override;
  virtual void ClearCookies(content::RenderViewHost* rvh) override;
  virtual base::FilePath GetDefaultDownloadDirectory() override;
  virtual std::string GetDefaultDownloadName() override;
  virtual void DidCreatePpapiPlugin(
      content::BrowserPpapiHost* browser_host) override;
  virtual content::BrowserPpapiHost* GetExternalBrowserPpapiHost(
      int plugin_process_id) override;
  virtual bool AllowPepperSocketAPI(
      content::BrowserContext* browser_context,
      const GURL& url,
      bool private_api,
      const content::SocketPermissionRequest* params) override;
  virtual ui::SelectFilePolicy* CreateSelectFilePolicy(
      content::WebContents* web_contents) override;
  virtual void GetAdditionalAllowedSchemesForFileSystem(
      std::vector<std::string>* additional_schemes) override;
  virtual void GetURLRequestAutoMountHandlers(
      std::vector<storage::URLRequestAutoMountHandler>* handlers) override;
  virtual void GetAdditionalFileSystemBackends(
      content::BrowserContext* browser_context,
      const base::FilePath& storage_partition_path,
      ScopedVector<storage::FileSystemBackend>* additional_backends) override;
  virtual content::DevToolsManagerDelegate*
      GetDevToolsManagerDelegate() override;
  virtual bool IsPluginAllowedToCallRequestOSFileHandle(
      content::BrowserContext* browser_context,
      const GURL& url) override;
  virtual bool IsPluginAllowedToUseDevChannelAPIs(
      content::BrowserContext* browser_context,
      const GURL& url) override;
  virtual net::CookieStore* OverrideCookieStoreForRenderProcess(
      int render_process_id) override;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  virtual void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::FileDescriptorInfo* mappings) override;
#endif
#if defined(OS_WIN)
  virtual const wchar_t* GetResourceDllName() override;
  virtual void PreSpawnRenderer(sandbox::TargetPolicy* policy,
                                bool* success) override;
#endif
  virtual bool CheckMediaAccessPermission(
      content::BrowserContext* browser_context,
      const GURL& security_origin,
      content::MediaStreamType type) override;

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
