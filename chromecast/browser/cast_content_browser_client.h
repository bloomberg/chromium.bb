// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_CONTENT_BROWSER_CLIENT_H_
#define CHROMECAST_BROWSER_CAST_CONTENT_BROWSER_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chromecast/chromecast_features.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/content_browser_client.h"
#include "services/service_manager/public/cpp/binder_registry.h"

class PrefService;

namespace breakpad {
class CrashHandlerHostLinux;
}

namespace metrics {
class MetricsService;
}

namespace net {
class URLRequestContextGetter;
class X509Certificate;
}

namespace chromecast {
class CastService;
class CastWindowManager;

namespace media {
class MediaCapsImpl;
class MediaPipelineBackendFactory;
class MediaPipelineBackendManager;
class MediaResourceTracker;
class VideoPlaneController;
class VideoModeSwitcher;
class VideoResolutionPolicy;
}

namespace shell {

class CastBrowserMainParts;
class CastResourceDispatcherHostDelegate;
class URLRequestContextFactory;

class CastContentBrowserClient : public content::ContentBrowserClient {
 public:
  // Creates an implementation of CastContentBrowserClient. Platform should
  // link in an implementation as needed.
  static std::unique_ptr<CastContentBrowserClient> Create();

  ~CastContentBrowserClient() override;

  // Appends extra command line arguments before launching a new process.
  virtual void AppendExtraCommandLineSwitches(base::CommandLine* command_line);

  // Hook for code to run before browser threads created.
  virtual void PreCreateThreads();

  // Creates and returns the CastService instance for the current process.
  // Note: |request_context_getter| might be different than the main request
  // getter accessible via CastBrowserProcess.
  virtual std::unique_ptr<CastService> CreateCastService(
      content::BrowserContext* browser_context,
      PrefService* pref_service,
      net::URLRequestContextGetter* request_context_getter,
      media::VideoPlaneController* video_plane_controller,
      CastWindowManager* window_manager);

  virtual media::VideoModeSwitcher* GetVideoModeSwitcher();

#if BUILDFLAG(IS_CAST_USING_CMA_BACKEND)
  // Gets object for enforcing video resolution policy restrictions.
  virtual media::VideoResolutionPolicy* GetVideoResolutionPolicy();

  // Returns the task runner that must be used for media IO.
  scoped_refptr<base::SingleThreadTaskRunner> GetMediaTaskRunner();

  // Creates a MediaPipelineBackendFactory.
  virtual media::MediaPipelineBackendFactory* GetMediaPipelineBackendFactory();

  media::MediaResourceTracker* media_resource_tracker();

  media::MediaPipelineBackendManager* media_pipeline_backend_manager();

  std::unique_ptr<::media::AudioManager> CreateAudioManager(
      ::media::AudioLogFactory* audio_log_factory) override;
  std::unique_ptr<::media::CdmFactory> CreateCdmFactory() override;
#endif  // BUILDFLAG(IS_CAST_USING_CMA_BACKEND)
  media::MediaCapsImpl* media_caps();

  // Invoked when the metrics client ID changes.
  virtual void SetMetricsClientId(const std::string& client_id);

  // Allows registration of extra metrics providers.
  virtual void RegisterMetricsProviders(
      ::metrics::MetricsService* metrics_service);

  // Returns whether or not the remote debugging service should be started
  // on browser startup.
  virtual bool EnableRemoteDebuggingImmediately();

  // content::ContentBrowserClient implementation:
  content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) override;
  void RenderProcessWillLaunch(content::RenderProcessHost* host) override;
  bool IsHandledURL(const GURL& url) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  void OverrideWebkitPrefs(content::RenderViewHost* render_view_host,
                           content::WebPreferences* prefs) override;
  void ResourceDispatcherHostCreated() override;
  std::string GetApplicationLocale() override;
  content::QuotaPermissionContext* CreateQuotaPermissionContext() override;
  void GetQuotaSettings(
      content::BrowserContext* context,
      content::StoragePartition* partition,
      const storage::OptionalQuotaSettingsCallback& callback) override;
  void AllowCertificateError(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      content::ResourceType resource_type,
      bool overridable,
      bool strict_enforcement,
      bool expired_previous_decision,
      const base::Callback<void(content::CertificateRequestResultType)>&
          callback) override;
  void SelectClientCertificate(
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::CertificateList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) override;
  bool CanCreateWindow(content::RenderFrameHost* opener,
                       const GURL& opener_url,
                       const GURL& opener_top_level_frame_url,
                       const GURL& source_origin,
                       content::mojom::WindowContainerType container_type,
                       const GURL& target_url,
                       const content::Referrer& referrer,
                       const std::string& frame_name,
                       WindowOpenDisposition disposition,
                       const blink::mojom::WindowFeatures& features,
                       bool user_gesture,
                       bool opener_suppressed,
                       bool* no_javascript_access) override;
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      content::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) override;
  void RegisterInProcessServices(StaticServiceMap* services) override;
  std::unique_ptr<base::Value> GetServiceManifestOverlay(
      base::StringPiece service_name) override;
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::FileDescriptorInfo* mappings) override;
  void GetAdditionalWebUISchemes(
      std::vector<std::string>* additional_schemes) override;
  content::DevToolsManagerDelegate* GetDevToolsManagerDelegate() override;

 protected:
  CastContentBrowserClient();

  URLRequestContextFactory* url_request_context_factory() const {
    return url_request_context_factory_.get();
  }

 private:
  void AddNetworkHintsMessageFilter(int render_process_id,
                                    net::URLRequestContext* context);

  net::X509Certificate* SelectClientCertificateOnIOThread(
      GURL requesting_url,
      int render_process_id);

#if !defined(OS_ANDROID)
  // Returns the crash signal FD corresponding to the current process type.
  int GetCrashSignalFD(const base::CommandLine& command_line);

  // Creates a CrashHandlerHost instance for the given process type.
  breakpad::CrashHandlerHostLinux* CreateCrashHandlerHost(
      const std::string& process_type);

  // A static cache to hold crash_handlers for each process_type
  std::map<std::string, breakpad::CrashHandlerHostLinux*> crash_handlers_;
#endif  // !defined(OS_ANDROID)

  // Created by CastContentBrowserClient but owned by BrowserMainLoop.
  CastBrowserMainParts* cast_browser_main_parts_;
  std::unique_ptr<URLRequestContextFactory> url_request_context_factory_;
  std::unique_ptr<CastResourceDispatcherHostDelegate>
      resource_dispatcher_host_delegate_;
  std::unique_ptr<media::MediaPipelineBackendFactory>
      media_pipeline_backend_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastContentBrowserClient);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_CONTENT_BROWSER_CLIENT_H_
