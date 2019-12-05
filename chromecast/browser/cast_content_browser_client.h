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
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/metrics/cast_metrics_service_client.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/content_browser_client.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/url_request/url_request_context.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/mojom/interface_provider.mojom-forward.h"
#include "services/service_manager/public/mojom/service.mojom-forward.h"
#include "storage/browser/quota/quota_settings.h"

class PrefService;

namespace base {
struct OnTaskRunnerDeleter;
}

namespace breakpad {
class CrashHandlerHostLinux;
}

namespace device {
class BluetoothAdapterCast;
}

namespace media {
class CdmFactory;
}

namespace metrics {
class MetricsService;
}

namespace net {
class SSLPrivateKey;
class X509Certificate;
}

namespace chromecast {
class CastService;
class CastSystemMemoryPressureEvaluatorAdjuster;
class CastWindowManager;
class CastFeatureListCreator;
class GeneralAudienceBrowsingService;
class MemoryPressureControllerImpl;

namespace media {
class MediaCapsImpl;
class CmaBackendFactory;
class MediaPipelineBackendManager;
class MediaResourceTracker;
class VideoGeometrySetterService;
class VideoPlaneController;
class VideoModeSwitcher;
class VideoResolutionPolicy;
}

namespace shell {
class CastBrowserMainParts;
class CastNetworkContexts;
class URLRequestContextFactory;

class CastContentBrowserClient
    : public content::ContentBrowserClient,
      public chromecast::metrics::CastMetricsServiceDelegate {
 public:
  // Creates an implementation of CastContentBrowserClient. Platform should
  // link in an implementation as needed.
  static std::unique_ptr<CastContentBrowserClient> Create(
      CastFeatureListCreator* cast_feature_list_creator);

  // Returns a list of headers that will be exempt from CORS preflight checks.
  // This is needed since currently servers don't have the correct response to
  // preflight checks.
  static std::vector<std::string> GetCorsExemptHeadersList();

  ~CastContentBrowserClient() override;

  // Creates and returns the CastService instance for the current process.
  virtual std::unique_ptr<CastService> CreateCastService(
      content::BrowserContext* browser_context,
      CastSystemMemoryPressureEvaluatorAdjuster*
          cast_system_memory_pressure_evaluator_adjuster,
      PrefService* pref_service,
      media::VideoPlaneController* video_plane_controller,
      CastWindowManager* window_manager);

  virtual media::VideoModeSwitcher* GetVideoModeSwitcher();

  virtual void InitializeURLLoaderThrottleDelegate();

  // Returns the task runner that must be used for media IO.
  scoped_refptr<base::SingleThreadTaskRunner> GetMediaTaskRunner();

  // Gets object for enforcing video resolution policy restrictions.
  virtual media::VideoResolutionPolicy* GetVideoResolutionPolicy();

  // Creates a CmaBackendFactory.
  virtual media::CmaBackendFactory* GetCmaBackendFactory();

  media::MediaResourceTracker* media_resource_tracker();

  void ResetMediaResourceTracker();

  media::MediaPipelineBackendManager* media_pipeline_backend_manager();

  std::unique_ptr<::media::AudioManager> CreateAudioManager(
      ::media::AudioLogFactory* audio_log_factory) override;
  bool OverridesAudioManager() override;
  media::MediaCapsImpl* media_caps();

#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
  // Create a BluetoothAdapter for WebBluetooth support.
  // TODO(slan): This further couples the browser to the Cast service. Remove
  // this once the dedicated Bluetooth service has been implemented.
  // (b/76155468)
  virtual base::WeakPtr<device::BluetoothAdapterCast> CreateBluetoothAdapter();
#endif  // !defined(OS_ANDROID) && !defined(OS_FUCHSIA)

  // chromecast::metrics::CastMetricsServiceDelegate implementation:
  void SetMetricsClientId(const std::string& client_id) override;
  void RegisterMetricsProviders(
      ::metrics::MetricsService* metrics_service) override;

  // Returns whether or not the remote debugging service should be started
  // on browser startup.
  virtual bool EnableRemoteDebuggingImmediately();

  // content::ContentBrowserClient implementation:
  std::vector<std::string> GetStartupServices() override;
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) override;
  void RenderProcessWillLaunch(content::RenderProcessHost* host) override;
  bool IsHandledURL(const GURL& url) override;
  void SiteInstanceGotProcess(content::SiteInstance* site_instance) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  std::string GetAcceptLangs(content::BrowserContext* context) override;
  network::mojom::NetworkContext* GetSystemNetworkContext() override;
  void OverrideWebkitPrefs(content::RenderViewHost* render_view_host,
                           content::WebPreferences* prefs) override;
  std::string GetApplicationLocale() override;
  scoped_refptr<content::QuotaPermissionContext> CreateQuotaPermissionContext()
      override;
  void GetQuotaSettings(
      content::BrowserContext* context,
      content::StoragePartition* partition,
      storage::OptionalQuotaSettingsCallback callback) override;
  void AllowCertificateError(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool is_main_frame_request,
      bool strict_enforcement,
      base::OnceCallback<void(content::CertificateRequestResultType)> callback)
      override;
  base::OnceClosure SelectClientCertificate(
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) override;
  bool CanCreateWindow(content::RenderFrameHost* opener,
                       const GURL& opener_url,
                       const GURL& opener_top_level_frame_url,
                       const url::Origin& source_origin,
                       content::mojom::WindowContainerType container_type,
                       const GURL& target_url,
                       const content::Referrer& referrer,
                       const std::string& frame_name,
                       WindowOpenDisposition disposition,
                       const blink::mojom::WindowFeatures& features,
                       bool user_gesture,
                       bool opener_suppressed,
                       bool* no_javascript_access) override;
  // New Mojo bindings should be added to
  // cast_content_browser_client_receiver_bindings.cc, so that they go through
  // security review.
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) override;
  void ExposeInterfacesToMediaService(
      service_manager::BinderRegistry* registry,
      content::RenderFrameHost* render_frame_host) override;
  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      service_manager::BinderMapWithContext<content::RenderFrameHost*>* map)
      override;
  void RunServiceInstance(
      const service_manager::Identity& identity,
      mojo::PendingReceiver<service_manager::mojom::Service>* receiver)
      override;
  base::Optional<service_manager::Manifest> GetServiceManifestOverlay(
      base::StringPiece service_name) override;
  std::vector<service_manager::Manifest> GetExtraServiceManifests() override;
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
  void GetAdditionalWebUISchemes(
      std::vector<std::string>* additional_schemes) override;
  content::DevToolsManagerDelegate* GetDevToolsManagerDelegate() override;
  std::unique_ptr<content::NavigationUIData> GetNavigationUIData(
      content::NavigationHandle* navigation_handle) override;
  bool ShouldEnableStrictSiteIsolation() override;
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(content::NavigationHandle* handle) override;
  void RegisterNonNetworkNavigationURLLoaderFactories(
      int frame_tree_node_id,
      NonNetworkURLLoaderFactoryMap* factories) override;
  void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      NonNetworkURLLoaderFactoryMap* factories) override;
  void OnNetworkServiceCreated(
      network::mojom::NetworkService* network_service) override;
  mojo::Remote<network::mojom::NetworkContext> CreateNetworkContext(
      content::BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path) override;
  std::string GetUserAgent() override;
  bool DoesSiteRequireDedicatedProcess(content::BrowserContext* browser_context,
                                       const GURL& effective_site_url) override;
  // New Mojo bindings should be added to
  // cast_content_browser_client_receiver_bindings.cc, so that they go through
  // security review.
  void BindHostReceiverForRenderer(
      content::RenderProcessHost* render_process_host,
      mojo::GenericPendingReceiver receiver) override;
  CastFeatureListCreator* GetCastFeatureListCreator() {
    return cast_feature_list_creator_;
  }

  void CreateGeneralAudienceBrowsingService();

  virtual std::unique_ptr<::media::CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* host_interfaces);

#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void BindGpuHostReceiver(mojo::GenericPendingReceiver receiver) override;
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

  CastNetworkContexts* cast_network_contexts() {
    return cast_network_contexts_.get();
  }

 protected:
  explicit CastContentBrowserClient(
      CastFeatureListCreator* cast_feature_list_creator);

  URLRequestContextFactory* url_request_context_factory() const {
    return url_request_context_factory_.get();
  }

  void BindMediaRenderer(
      mojo::PendingReceiver<::media::mojom::Renderer> receiver);

  // Internal implementation overwrites this function to inject real values.
  virtual void GetApplicationMediaInfo(
      std::string* application_session_id,
      bool* mixer_audio_enabled,
      content::RenderFrameHost* render_frame_host);

 private:
  // Create device cert/key
  virtual scoped_refptr<net::X509Certificate> DeviceCert();
  virtual scoped_refptr<net::SSLPrivateKey> DeviceKey();

  void SelectClientCertificateOnIOThread(
      GURL requesting_url,
      const std::string& session_id,
      int render_process_id,
      int render_frame_id,
      scoped_refptr<base::SequencedTaskRunner> original_runner,
      const base::Callback<void(scoped_refptr<net::X509Certificate>,
                                scoped_refptr<net::SSLPrivateKey>)>&
          continue_callback);

#if !defined(OS_FUCHSIA)
  // Returns the crash signal FD corresponding to the current process type.
  int GetCrashSignalFD(const base::CommandLine& command_line);

#if !defined(OS_ANDROID)
  // Creates a CrashHandlerHost instance for the given process type.
  breakpad::CrashHandlerHostLinux* CreateCrashHandlerHost(
      const std::string& process_type);

  // A static cache to hold crash_handlers for each process_type
  std::map<std::string, breakpad::CrashHandlerHostLinux*> crash_handlers_;

  // Notify renderers of memory pressure (Android renderers register directly
  // with OS for this).
  std::unique_ptr<MemoryPressureControllerImpl> memory_pressure_controller_;
#endif  // !defined(OS_ANDROID)
#endif  // !defined(OS_FUCHSIA)

  // CMA thread used by AudioManager, MojoRenderer, and MediaPipelineBackend.
  std::unique_ptr<base::Thread> media_thread_;

  // Tracks usage of media resource by e.g. CMA pipeline, CDM.
  media::MediaResourceTracker* media_resource_tracker_ = nullptr;

#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void CreateMediaService(service_manager::mojom::ServiceRequest request);

  // VideoGeometrySetterService must be constructed On a sequence, and later
  // runs and destructs on this sequence.
  void CreateVideoGeometrySetterServiceOnMediaThread();
  void BindVideoGeometrySetterServiceOnMediaThread(
      mojo::GenericPendingReceiver receiver);
  // video_geometry_setter_service_ lives on media thread.
  std::unique_ptr<media::VideoGeometrySetterService, base::OnTaskRunnerDeleter>
      video_geometry_setter_service_;
#endif

  // Created by CastContentBrowserClient but owned by BrowserMainLoop.
  CastBrowserMainParts* cast_browser_main_parts_;
  std::unique_ptr<CastNetworkContexts> cast_network_contexts_;
  std::unique_ptr<URLRequestContextFactory> url_request_context_factory_;
  std::unique_ptr<media::CmaBackendFactory> cma_backend_factory_;
  std::unique_ptr<GeneralAudienceBrowsingService>
      general_audience_browsing_service_;

  CastFeatureListCreator* cast_feature_list_creator_;

  DISALLOW_COPY_AND_ASSIGN(CastContentBrowserClient);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_CONTENT_BROWSER_CLIENT_H_
