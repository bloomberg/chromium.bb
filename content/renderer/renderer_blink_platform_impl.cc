// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_blink_platform_impl.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/memory_coordinator_client_registry.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
#include "content/child/child_process.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/frame_messages.h"
#include "content/common/gpu_stream_constants.h"
#include "content/common/origin_trials/trial_policy_impl.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/media_stream_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/blob_storage/webblobregistry_impl.h"
#include "content/renderer/cache_storage/webserviceworkercachestorage_impl.h"
#include "content/renderer/device_sensors/device_motion_event_pump.h"
#include "content/renderer/device_sensors/device_orientation_event_pump.h"
#include "content/renderer/dom_storage/local_storage_cached_areas.h"
#include "content/renderer/dom_storage/local_storage_namespace.h"
#include "content/renderer/dom_storage/session_web_storage_namespace_impl.h"
#include "content/renderer/dom_storage/webstoragenamespace_impl.h"
#include "content/renderer/file_info_util.h"
#include "content/renderer/fileapi/webfilesystem_impl.h"
#include "content/renderer/gamepad_shared_memory_reader.h"
#include "content/renderer/image_capture/image_capture_frame_grabber.h"
#include "content/renderer/indexed_db/webidbfactory_impl.h"
#include "content/renderer/loader/child_url_loader_factory_getter_impl.h"
#include "content/renderer/loader/cors_url_loader_factory.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/loader/web_data_consumer_handle_impl.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/media/audio_decoder.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/renderer_webaudiodevice_impl.h"
#include "content/renderer/media/renderer_webmidiaccessor_impl.h"
#include "content/renderer/media_capture_from_element/canvas_capture_handler.h"
#include "content/renderer/media_capture_from_element/html_audio_element_capturer_source.h"
#include "content/renderer/media_capture_from_element/html_video_element_capturer_source.h"
#include "content/renderer/media_recorder/media_recorder_handler.h"
#include "content/renderer/mojo/blink_interface_provider_impl.h"
#include "content/renderer/notifications/notification_dispatcher.h"
#include "content/renderer/notifications/notification_manager.h"
#include "content/renderer/push_messaging/push_provider.h"
#include "content/renderer/quota_dispatcher.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/storage_util.h"
#include "content/renderer/web_database_observer_impl.h"
#include "content/renderer/webfileutilities_impl.h"
#include "content/renderer/webgraphicscontext3d_provider_impl.h"
#include "content/renderer/webpublicsuffixlist_impl.h"
#include "content/renderer/worker_thread_registry.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/sensors/public/cpp/motion_data.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/audio/audio_output_device.h"
#include "media/blink/webcontentdecryptionmodule_impl.h"
#include "media/filters/stream_parser_factory.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ppapi/features/features.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/WebKit/common/origin_trials/trial_token_validator.h"
#include "third_party/WebKit/common/quota/quota_types.mojom.h"
#include "third_party/WebKit/public/platform/BlameContext.h"
#include "third_party/WebKit/public/platform/FilePathConversion.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebAudioLatencyHint.h"
#include "third_party/WebKit/public/platform/WebBlobRegistry.h"
#include "third_party/WebKit/public/platform/WebFileInfo.h"
#include "third_party/WebKit/public/platform/WebMediaRecorderHandler.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenter.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenterClient.h"
#include "third_party/WebKit/public/platform/WebPluginListBuilder.h"
#include "third_party/WebKit/public/platform/WebRTCCertificateGenerator.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandler.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebSocketHandshakeThrottle.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLLoaderFactory.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceMotionListener.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceOrientationListener.h"
#include "third_party/WebKit/public/platform/modules/webmidi/WebMIDIAccessor.h"
#include "third_party/WebKit/public/platform/scheduler/child/webthread_base.h"
#include "third_party/WebKit/public/platform/scheduler/renderer/renderer_scheduler.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/sqlite/sqlite3.h"
#include "url/gurl.h"

#if defined(OS_MACOSX)
#include "content/common/mac/font_loader.h"
#include "content/renderer/webscrollbarbehavior_impl_mac.h"
#include "third_party/WebKit/public/platform/mac/WebSandboxSupport.h"
#endif

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
#include <map>
#include <string>

#include "base/synchronization/lock.h"
#include "content/child/child_process_sandbox_support_impl_linux.h"
#include "third_party/WebKit/public/platform/linux/WebFallbackFont.h"
#include "third_party/WebKit/public/platform/linux/WebSandboxSupport.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#endif
#endif

#if defined(USE_AURA)
#include "content/renderer/webscrollbarbehavior_impl_aura.h"
#elif !defined(OS_MACOSX)
#include "third_party/WebKit/public/platform/WebScrollbarBehavior.h"
#define WebScrollbarBehaviorImpl blink::WebScrollbarBehavior
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
#include "content/renderer/media/rtc_certificate_generator.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#endif

using blink::Platform;
using blink::WebAudioDevice;
using blink::WebAudioLatencyHint;
using blink::WebBlobRegistry;
using blink::WebCanvasCaptureHandler;
using blink::WebDatabaseObserver;
using blink::WebFileInfo;
using blink::WebFileSystem;
using blink::WebIDBFactory;
using blink::WebImageCaptureFrameGrabber;
using blink::WebMediaPlayer;
using blink::WebMediaRecorderHandler;
using blink::WebMediaStream;
using blink::WebMediaStreamCenter;
using blink::WebMediaStreamCenterClient;
using blink::WebMediaStreamTrack;
using blink::WebRTCPeerConnectionHandler;
using blink::WebRTCPeerConnectionHandlerClient;
using blink::WebStorageNamespace;
using blink::WebSize;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace content {

namespace {

bool g_sandbox_enabled = true;
base::LazyInstance<device::MotionData>::Leaky g_test_device_motion_data =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<device::OrientationData>::Leaky
    g_test_device_orientation_data = LAZY_INSTANCE_INITIALIZER;

media::AudioParameters GetAudioHardwareParams() {
  blink::WebLocalFrame* const web_frame =
      blink::WebLocalFrame::FrameForCurrentContext();
  RenderFrame* const render_frame = RenderFrame::FromWebFrame(web_frame);
  if (!render_frame)
    return media::AudioParameters::UnavailableDeviceParams();

  return AudioDeviceFactory::GetOutputDeviceInfo(render_frame->GetRoutingID(),
                                                 0, std::string(),
                                                 web_frame->GetSecurityOrigin())
      .output_params();
}

network::mojom::URLLoaderFactoryPtr GetBlobURLLoaderFactoryGetter() {
  network::mojom::URLLoaderFactoryPtr blob_loader_factory;
  RenderThreadImpl::current()->GetRendererHost()->GetBlobURLLoaderFactory(
      mojo::MakeRequest(&blob_loader_factory));
  return blob_loader_factory;
}

}  // namespace

//------------------------------------------------------------------------------

class RendererBlinkPlatformImpl::FileUtilities : public WebFileUtilitiesImpl {
 public:
  explicit FileUtilities(
      scoped_refptr<mojom::ThreadSafeFileUtilitiesHostPtr> host)
      : file_utilities_host_(std::move(host)) {}
  bool GetFileInfo(const WebString& path, WebFileInfo& result) override;

 private:
  mojom::FileUtilitiesHost& GetFileUtilitiesHost() {
    return **file_utilities_host_;
  }

  scoped_refptr<mojom::ThreadSafeFileUtilitiesHostPtr> file_utilities_host_;
};

#if !defined(OS_ANDROID) && !defined(OS_WIN) && !defined(OS_FUCHSIA)
class RendererBlinkPlatformImpl::SandboxSupport
    : public blink::WebSandboxSupport {
 public:
  virtual ~SandboxSupport() {}

#if defined(OS_MACOSX)
  bool LoadFont(CTFontRef src_font,
                CGFontRef* container,
                uint32_t* font_id) override;
#elif defined(OS_POSIX)
  void GetFallbackFontForCharacter(
      blink::WebUChar32 character,
      const char* preferred_locale,
      blink::WebFallbackFont* fallbackFont) override;
  void GetWebFontRenderStyleForStrike(const char* family,
                                      int sizeAndStyle,
                                      blink::WebFontRenderStyle* out) override;

 private:
  // WebKit likes to ask us for the correct font family to use for a set of
  // unicode code points. It needs this information frequently so we cache it
  // here.
  base::Lock unicode_font_families_mutex_;
  std::map<int32_t, blink::WebFallbackFont> unicode_font_families_;
#endif
};
#endif  // !defined(OS_ANDROID) && !defined(OS_WIN)

//------------------------------------------------------------------------------

RendererBlinkPlatformImpl::RendererBlinkPlatformImpl(
    blink::scheduler::RendererScheduler* renderer_scheduler)
    : BlinkPlatformImpl(renderer_scheduler->DefaultTaskRunner(),
                        RenderThreadImpl::current()
                            ? RenderThreadImpl::current()->GetIOTaskRunner()
                            : nullptr),
      compositor_thread_(nullptr),
      main_thread_(renderer_scheduler->CreateMainThread()),
      sudden_termination_disables_(0),
      plugin_refresh_allowed_(true),
      default_task_runner_(renderer_scheduler->DefaultTaskRunner()),
      web_scrollbar_behavior_(new WebScrollbarBehaviorImpl),
      renderer_scheduler_(renderer_scheduler) {
#if !defined(OS_ANDROID) && !defined(OS_WIN) && !defined(OS_FUCHSIA)
  if (g_sandbox_enabled && sandboxEnabled()) {
    sandbox_support_.reset(new RendererBlinkPlatformImpl::SandboxSupport);
  } else {
    DVLOG(1) << "Disabling sandbox support for testing.";
  }
#endif

  // RenderThread may not exist in some tests.
  if (RenderThreadImpl::current()) {
    connector_ = RenderThreadImpl::current()
                     ->GetServiceManagerConnection()
                     ->GetConnector()
                     ->Clone();
    sync_message_filter_ = RenderThreadImpl::current()->sync_message_filter();
    thread_safe_sender_ = RenderThreadImpl::current()->thread_safe_sender();
    shared_bitmap_manager_ =
        RenderThreadImpl::current()->shared_bitmap_manager();
    blob_registry_.reset(new WebBlobRegistryImpl(thread_safe_sender_.get()));
    web_idb_factory_.reset(new WebIDBFactoryImpl(
        sync_message_filter_,
        RenderThreadImpl::current()->GetIOTaskRunner().get()));
    notification_dispatcher_ =
        RenderThreadImpl::current()->notification_dispatcher();
  } else {
    service_manager::mojom::ConnectorRequest request;
    connector_ = service_manager::Connector::Create(&request);
  }

  blink_interface_provider_.reset(
      new BlinkInterfaceProviderImpl(connector_.get()));
  top_level_blame_context_.Initialize();
  renderer_scheduler_->SetTopLevelBlameContext(&top_level_blame_context_);

  GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&web_database_host_info_));

  GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&file_utilities_host_info_));
}

RendererBlinkPlatformImpl::~RendererBlinkPlatformImpl() {
  WebFileSystemImpl::DeleteThreadSpecificInstance();
  renderer_scheduler_->SetTopLevelBlameContext(nullptr);
  shared_bitmap_manager_ = nullptr;
}

void RendererBlinkPlatformImpl::Shutdown() {
#if !defined(OS_ANDROID) && !defined(OS_WIN) && !defined(OS_FUCHSIA)
  // SandboxSupport contains a map of WebFallbackFont objects, which hold
  // WebStrings and WebVectors, which become invalidated when blink is shut
  // down. Hence, we need to clear that map now, just before blink::shutdown()
  // is called.
  sandbox_support_.reset();
#endif
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebURLLoaderFactory>
RendererBlinkPlatformImpl::CreateDefaultURLLoaderFactory() {
  if (!RenderThreadImpl::current()) {
    // RenderThreadImpl is null in some tests, the default factory impl
    // takes care of that in the case.
    return std::make_unique<WebURLLoaderFactoryImpl>(nullptr, nullptr);
  }
  return std::make_unique<WebURLLoaderFactoryImpl>(
      RenderThreadImpl::current()->resource_dispatcher()->GetWeakPtr(),
      CreateDefaultURLLoaderFactoryGetter());
}

std::unique_ptr<blink::WebDataConsumerHandle>
RendererBlinkPlatformImpl::CreateDataConsumerHandle(
    mojo::ScopedDataPipeConsumerHandle handle) {
  return std::make_unique<WebDataConsumerHandleImpl>(std::move(handle));
}

scoped_refptr<ChildURLLoaderFactoryGetter>
RendererBlinkPlatformImpl::CreateDefaultURLLoaderFactoryGetter() {
  return base::MakeRefCounted<ChildURLLoaderFactoryGetterImpl>(
      CreateNetworkURLLoaderFactory(),
      base::FeatureList::IsEnabled(features::kNetworkService)
          ? base::BindOnce(&GetBlobURLLoaderFactoryGetter)
          : ChildURLLoaderFactoryGetterImpl::URLLoaderFactoryGetterCallback());
}

PossiblyAssociatedInterfacePtr<network::mojom::URLLoaderFactory>
RendererBlinkPlatformImpl::CreateNetworkURLLoaderFactory() {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread);
  PossiblyAssociatedInterfacePtr<network::mojom::URLLoaderFactory>
      url_loader_factory;

  if (base::FeatureList::IsEnabled(features::kNetworkService)) {
    network::mojom::URLLoaderFactoryPtr factory_ptr;
    connector_->BindInterface(mojom::kBrowserServiceName, &factory_ptr);
    url_loader_factory = std::move(factory_ptr);
  } else {
    network::mojom::URLLoaderFactoryAssociatedPtr factory_ptr;
    render_thread->channel()->GetRemoteAssociatedInterface(&factory_ptr);
    url_loader_factory = std::move(factory_ptr);
  }

  // Attach the CORS-enabled URLLoader for the network URLLoaderFactory. To
  // avoid thread hops and prevent jank on the main thread from affecting
  // requests from other threads this object should live on the IO thread.
  if (base::FeatureList::IsEnabled(features::kOutOfBlinkCORS)) {
    network::mojom::URLLoaderFactoryPtr factory_ptr;
    RenderThreadImpl::current()->GetIOTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&CORSURLLoaderFactory::CreateAndBind,
                                  url_loader_factory.PassInterface(),
                                  mojo::MakeRequest(&factory_ptr)));
    url_loader_factory = std::move(factory_ptr);
  }
  return url_loader_factory;
}

void RendererBlinkPlatformImpl::SetCompositorThread(
    blink::scheduler::WebThreadBase* compositor_thread) {
  compositor_thread_ = compositor_thread;
  if (compositor_thread_)
    WaitUntilWebThreadTLSUpdate(compositor_thread_);
}

blink::WebThread* RendererBlinkPlatformImpl::CurrentThread() {
  if (main_thread_->IsCurrentThread())
    return main_thread_.get();
  return BlinkPlatformImpl::CurrentThread();
}

blink::BlameContext* RendererBlinkPlatformImpl::GetTopLevelBlameContext() {
  return &top_level_blame_context_;
}

blink::WebClipboard* RendererBlinkPlatformImpl::Clipboard() {
  blink::WebClipboard* clipboard =
      GetContentClient()->renderer()->OverrideWebClipboard();
  if (clipboard)
    return clipboard;

  return BlinkPlatformImpl::Clipboard();
}

blink::WebFileUtilities* RendererBlinkPlatformImpl::GetFileUtilities() {
  if (!file_utilities_) {
    file_utilities_.reset(
        new FileUtilities(mojom::ThreadSafeFileUtilitiesHostPtr::Create(
            std::move(file_utilities_host_info_),
            base::CreateSequencedTaskRunnerWithTraits(
                {base::WithBaseSyncPrimitives()}))));
    file_utilities_->set_sandbox_enabled(sandboxEnabled());
  }
  return file_utilities_.get();
}

blink::WebSandboxSupport* RendererBlinkPlatformImpl::GetSandboxSupport() {
#if defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_FUCHSIA)
  // These platforms do not require sandbox support.
  return NULL;
#else
  return sandbox_support_.get();
#endif
}

blink::WebCookieJar* RendererBlinkPlatformImpl::CookieJar() {
  NOTREACHED() << "Use WebFrameClient::cookieJar() instead!";
  return nullptr;
}

blink::WebThemeEngine* RendererBlinkPlatformImpl::ThemeEngine() {
  blink::WebThemeEngine* theme_engine =
      GetContentClient()->renderer()->OverrideThemeEngine();
  if (theme_engine)
    return theme_engine;
  return BlinkPlatformImpl::ThemeEngine();
}

bool RendererBlinkPlatformImpl::sandboxEnabled() {
  // As explained in Platform.h, this function is used to decide
  // whether to allow file system operations to come out of WebKit or not.
  // Even if the sandbox is disabled, there's no reason why the code should
  // act any differently...unless we're in single process mode.  In which
  // case, we have no other choice.  Platform.h discourages using
  // this switch unless absolutely necessary, so hopefully we won't end up
  // with too many code paths being different in single-process mode.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess);
}

unsigned long long RendererBlinkPlatformImpl::VisitedLinkHash(
    const char* canonical_url,
    size_t length) {
  return GetContentClient()->renderer()->VisitedLinkHash(canonical_url, length);
}

bool RendererBlinkPlatformImpl::IsLinkVisited(unsigned long long link_hash) {
  return GetContentClient()->renderer()->IsLinkVisited(link_hash);
}

blink::WebPrescientNetworking*
RendererBlinkPlatformImpl::PrescientNetworking() {
  return GetContentClient()->renderer()->GetPrescientNetworking();
}

void RendererBlinkPlatformImpl::CacheMetadata(const blink::WebURL& url,
                                              base::Time response_time,
                                              const char* data,
                                              size_t size) {
  // Let the browser know we generated cacheable metadata for this resource. The
  // browser may cache it and return it on subsequent responses to speed
  // the processing of this resource.
  std::vector<uint8_t> copy(data, data + size);
  RenderThreadImpl::current()
      ->render_message_filter()
      ->DidGenerateCacheableMetadata(url, response_time, copy);
}

void RendererBlinkPlatformImpl::CacheMetadataInCacheStorage(
    const blink::WebURL& url,
    base::Time response_time,
    const char* data,
    size_t size,
    const blink::WebSecurityOrigin& cacheStorageOrigin,
    const blink::WebString& cacheStorageCacheName) {
  // Let the browser know we generated cacheable metadata for this resource in
  // CacheStorage. The browser may cache it and return it on subsequent
  // responses to speed the processing of this resource.
  std::vector<uint8_t> copy(data, data + size);
  RenderThreadImpl::current()
      ->render_message_filter()
      ->DidGenerateCacheableMetadataInCacheStorage(
          url, response_time, copy, cacheStorageOrigin,
          cacheStorageCacheName.Utf8());
}

WebString RendererBlinkPlatformImpl::DefaultLocale() {
  return WebString::FromASCII(RenderThread::Get()->GetLocale());
}

void RendererBlinkPlatformImpl::SuddenTerminationChanged(bool enabled) {
  if (enabled) {
    // We should not get more enables than disables, but we want it to be a
    // non-fatal error if it does happen.
    DCHECK_GT(sudden_termination_disables_, 0);
    sudden_termination_disables_ = std::max(sudden_termination_disables_ - 1,
                                            0);
    if (sudden_termination_disables_ != 0)
      return;
  } else {
    sudden_termination_disables_++;
    if (sudden_termination_disables_ != 1)
      return;
  }

  RenderThreadImpl* thread = RenderThreadImpl::current();
  if (thread)  // NULL in unittests.
    thread->GetRendererHost()->SuddenTerminationChanged(enabled);
}

void RendererBlinkPlatformImpl::AddRefProcess() {
  ChildProcess::current()->AddRefProcess();
}

void RendererBlinkPlatformImpl::ReleaseRefProcess() {
  ChildProcess::current()->ReleaseProcess();
}

blink::WebThread* RendererBlinkPlatformImpl::CompositorThread() const {
  return compositor_thread_;
}

std::unique_ptr<WebStorageNamespace>
RendererBlinkPlatformImpl::CreateLocalStorageNamespace() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableMojoLocalStorage)) {
    if (!local_storage_cached_areas_) {
      local_storage_cached_areas_.reset(new LocalStorageCachedAreas(
          RenderThreadImpl::current()->GetStoragePartitionService(),
          renderer_scheduler_));
    }
    return std::make_unique<LocalStorageNamespace>(
        local_storage_cached_areas_.get());
  }

  return std::make_unique<WebStorageNamespaceImpl>();
}

std::unique_ptr<blink::WebStorageNamespace>
RendererBlinkPlatformImpl::CreateSessionStorageNamespace(int64_t namespace_id) {
  if (base::FeatureList::IsEnabled(features::kMojoSessionStorage)) {
    if (!local_storage_cached_areas_) {
      local_storage_cached_areas_.reset(new LocalStorageCachedAreas(
          RenderThreadImpl::current()->GetStoragePartitionService(),
          renderer_scheduler_));
    }
    return std::make_unique<SessionWebStorageNamespaceImpl>(
        namespace_id, local_storage_cached_areas_.get());
  }

  return std::make_unique<WebStorageNamespaceImpl>(namespace_id);
}

//------------------------------------------------------------------------------

WebIDBFactory* RendererBlinkPlatformImpl::IdbFactory() {
  return web_idb_factory_.get();
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebServiceWorkerCacheStorage>
RendererBlinkPlatformImpl::CreateCacheStorage(
    const blink::WebSecurityOrigin& security_origin) {
  return std::make_unique<WebServiceWorkerCacheStorageImpl>(
      thread_safe_sender_.get(), security_origin);
}

//------------------------------------------------------------------------------

WebFileSystem* RendererBlinkPlatformImpl::FileSystem() {
  return WebFileSystemImpl::ThreadSpecificInstance(default_task_runner_);
}

WebString RendererBlinkPlatformImpl::FileSystemCreateOriginIdentifier(
    const blink::WebSecurityOrigin& origin) {
  return WebString::FromUTF8(
      storage::GetIdentifierFromOrigin(WebSecurityOriginToGURL(origin)));
}

//------------------------------------------------------------------------------

bool RendererBlinkPlatformImpl::FileUtilities::GetFileInfo(
    const WebString& path,
    WebFileInfo& web_file_info) {
  base::Optional<base::File::Info> file_info;
  if (!GetFileUtilitiesHost().GetFileInfo(blink::WebStringToFilePath(path),
                                          &file_info) ||
      !file_info) {
    return false;
  }
  FileInfoToWebFileInfo(file_info.value(), &web_file_info);
  web_file_info.platform_path = path;
  return true;
}

//------------------------------------------------------------------------------

#if defined(OS_MACOSX)

bool RendererBlinkPlatformImpl::SandboxSupport::LoadFont(CTFontRef src_font,
                                                         CGFontRef* out,
                                                         uint32_t* font_id) {
  uint32_t font_data_size;
  base::ScopedCFTypeRef<CFStringRef> name_ref(
      CTFontCopyPostScriptName(src_font));
  base::string16 font_name = SysCFStringRefToUTF16(name_ref);
  float font_point_size = CTFontGetSize(src_font);
  mojo::ScopedSharedBufferHandle font_data;
  if (!RenderThreadImpl::current()->render_message_filter()->LoadFont(
          font_name, font_point_size, &font_data_size, &font_data, font_id)) {
    *out = NULL;
    *font_id = 0;
    return false;
  }

  if (font_data_size == 0 || !font_data.is_valid() || *font_id == 0) {
    LOG(ERROR) << "Bad response from RenderProcessHostMsg_LoadFont() for "
               << font_name;
    *out = NULL;
    *font_id = 0;
    return false;
  }

  // TODO(jeremy): Need to call back into WebKit to make sure that the font
  // isn't already activated, based on the font id.  If it's already
  // activated, don't reactivate it here - crbug.com/72727 .

  return FontLoader::CGFontRefFromBuffer(std::move(font_data), font_data_size,
                                         out);
}

#elif defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_FUCHSIA)

void RendererBlinkPlatformImpl::SandboxSupport::GetFallbackFontForCharacter(
    blink::WebUChar32 character,
    const char* preferred_locale,
    blink::WebFallbackFont* fallbackFont) {
  base::AutoLock lock(unicode_font_families_mutex_);
  const std::map<int32_t, blink::WebFallbackFont>::const_iterator iter =
      unicode_font_families_.find(character);
  if (iter != unicode_font_families_.end()) {
    fallbackFont->name = iter->second.name;
    fallbackFont->filename = iter->second.filename;
    fallbackFont->fontconfig_interface_id =
        iter->second.fontconfig_interface_id;
    fallbackFont->ttc_index = iter->second.ttc_index;
    fallbackFont->is_bold = iter->second.is_bold;
    fallbackFont->is_italic = iter->second.is_italic;
    return;
  }

  content::GetFallbackFontForCharacter(character, preferred_locale,
                                       fallbackFont);
  unicode_font_families_.insert(std::make_pair(character, *fallbackFont));
}

void RendererBlinkPlatformImpl::SandboxSupport::GetWebFontRenderStyleForStrike(
    const char* family,
    int sizeAndStyle,
    blink::WebFontRenderStyle* out) {
  GetRenderStyleForStrike(family, sizeAndStyle, out);
}

#endif

//------------------------------------------------------------------------------

Platform::FileHandle RendererBlinkPlatformImpl::DatabaseOpenFile(
    const WebString& vfs_file_name,
    int desired_flags) {
  base::File file;
  GetWebDatabaseHost().OpenFile(vfs_file_name.Utf16(), desired_flags, &file);
  return file.TakePlatformFile();
}

int RendererBlinkPlatformImpl::DatabaseDeleteFile(
    const WebString& vfs_file_name,
    bool sync_dir) {
  int rv = SQLITE_IOERR_DELETE;
  GetWebDatabaseHost().DeleteFile(vfs_file_name.Utf16(), sync_dir, &rv);
  return rv;
}

long RendererBlinkPlatformImpl::DatabaseGetFileAttributes(
    const WebString& vfs_file_name) {
  int32_t rv = -1;
  GetWebDatabaseHost().GetFileAttributes(vfs_file_name.Utf16(), &rv);
  return rv;
}

long long RendererBlinkPlatformImpl::DatabaseGetFileSize(
    const WebString& vfs_file_name) {
  int64_t rv = 0LL;
  GetWebDatabaseHost().GetFileSize(vfs_file_name.Utf16(), &rv);
  return rv;
}

long long RendererBlinkPlatformImpl::DatabaseGetSpaceAvailableForOrigin(
    const blink::WebSecurityOrigin& origin) {
  int64_t rv = 0LL;
  GetWebDatabaseHost().GetSpaceAvailable(origin, &rv);
  return rv;
}

bool RendererBlinkPlatformImpl::DatabaseSetFileSize(
    const WebString& vfs_file_name,
    long long size) {
  bool rv = false;
  GetWebDatabaseHost().SetFileSize(vfs_file_name.Utf16(), size, &rv);
  return rv;
}

WebString RendererBlinkPlatformImpl::DatabaseCreateOriginIdentifier(
    const blink::WebSecurityOrigin& origin) {
  return WebString::FromUTF8(
      storage::GetIdentifierFromOrigin(WebSecurityOriginToGURL(origin)));
}

viz::FrameSinkId RendererBlinkPlatformImpl::GenerateFrameSinkId() {
  return viz::FrameSinkId(RenderThread::Get()->GetClientId(),
                          RenderThread::Get()->GenerateRoutingID());
}

bool RendererBlinkPlatformImpl::IsThreadedCompositingEnabled() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  // thread can be NULL in tests.
  return thread && thread->compositor_task_runner().get();
}

bool RendererBlinkPlatformImpl::IsGpuCompositingDisabled() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  RenderThreadImpl* thread = RenderThreadImpl::current();
  // |thread| can be NULL in tests.
  return !thread || thread->IsGpuCompositingDisabled();
}

bool RendererBlinkPlatformImpl::IsThreadedAnimationEnabled() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread ? thread->IsThreadedAnimationEnabled() : true;
}

double RendererBlinkPlatformImpl::AudioHardwareSampleRate() {
  return GetAudioHardwareParams().sample_rate();
}

size_t RendererBlinkPlatformImpl::AudioHardwareBufferSize() {
  return GetAudioHardwareParams().frames_per_buffer();
}

unsigned RendererBlinkPlatformImpl::AudioHardwareOutputChannels() {
  return GetAudioHardwareParams().channels();
}

WebDatabaseObserver* RendererBlinkPlatformImpl::DatabaseObserver() {
  if (!web_database_observer_impl_) {
    InitializeWebDatabaseHostIfNeeded();
    web_database_observer_impl_ =
        std::make_unique<WebDatabaseObserverImpl>(web_database_host_);
  }
  return web_database_observer_impl_.get();
}

std::unique_ptr<WebAudioDevice> RendererBlinkPlatformImpl::CreateAudioDevice(
    unsigned input_channels,
    unsigned channels,
    const blink::WebAudioLatencyHint& latency_hint,
    WebAudioDevice::RenderCallback* callback,
    const blink::WebString& input_device_id,
    const blink::WebSecurityOrigin& security_origin) {
  // Use a mock for testing.
  std::unique_ptr<blink::WebAudioDevice> mock_device =
      GetContentClient()->renderer()->OverrideCreateAudioDevice(latency_hint);
  if (mock_device)
    return mock_device;

  // The |channels| does not exactly identify the channel layout of the
  // device. The switch statement below assigns a best guess to the channel
  // layout based on number of channels.
  media::ChannelLayout layout = media::GuessChannelLayout(channels);
  if (layout == media::CHANNEL_LAYOUT_UNSUPPORTED)
    layout = media::CHANNEL_LAYOUT_DISCRETE;

  int session_id = 0;
  if (input_device_id.IsNull() ||
      !base::StringToInt(input_device_id.Utf8(), &session_id)) {
    session_id = 0;
  }

  return RendererWebAudioDeviceImpl::Create(
      layout, channels, latency_hint, callback, session_id,
      static_cast<url::Origin>(security_origin));
}

bool RendererBlinkPlatformImpl::DecodeAudioFileData(
    blink::WebAudioBus* destination_bus,
    const char* audio_file_data,
    size_t data_size) {
  return content::DecodeAudioFileData(destination_bus, audio_file_data,
                                      data_size);
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebMIDIAccessor>
RendererBlinkPlatformImpl::CreateMIDIAccessor(
    blink::WebMIDIAccessorClient* client) {
  std::unique_ptr<blink::WebMIDIAccessor> accessor =
      GetContentClient()->renderer()->OverrideCreateMIDIAccessor(client);
  if (accessor)
    return accessor;

  return std::make_unique<RendererWebMIDIAccessorImpl>(client);
}

void RendererBlinkPlatformImpl::GetPluginList(
    bool refresh,
    const blink::WebSecurityOrigin& mainFrameOrigin,
    blink::WebPluginListBuilder* builder) {
#if BUILDFLAG(ENABLE_PLUGINS)
  std::vector<WebPluginInfo> plugins;
  if (!plugin_refresh_allowed_)
    refresh = false;
  RenderThread::Get()->Send(
      new FrameHostMsg_GetPlugins(refresh, mainFrameOrigin, &plugins));
  for (const WebPluginInfo& plugin : plugins) {
    builder->AddPlugin(WebString::FromUTF16(plugin.name),
                       WebString::FromUTF16(plugin.desc),
                       blink::FilePathToWebString(plugin.path.BaseName()));

    for (const WebPluginMimeType& mime_type : plugin.mime_types) {
      builder->AddMediaTypeToLastPlugin(
          WebString::FromUTF8(mime_type.mime_type),
          WebString::FromUTF16(mime_type.description));

      for (const auto& extension : mime_type.file_extensions) {
        builder->AddFileExtensionToLastMediaType(
            WebString::FromUTF8(extension));
      }
    }
  }
#endif
}

//------------------------------------------------------------------------------

blink::WebPublicSuffixList* RendererBlinkPlatformImpl::PublicSuffixList() {
  return &public_suffix_list_;
}

//------------------------------------------------------------------------------

blink::WebScrollbarBehavior* RendererBlinkPlatformImpl::ScrollbarBehavior() {
  return web_scrollbar_behavior_.get();
}

//------------------------------------------------------------------------------

WebBlobRegistry* RendererBlinkPlatformImpl::GetBlobRegistry() {
  // blob_registry_ can be NULL when running some tests.
  return blob_registry_.get();
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::SampleGamepads(device::Gamepads& gamepads) {
  PlatformEventObserverBase* observer =
      platform_event_observers_.Lookup(blink::kWebPlatformEventTypeGamepad);
  if (!observer)
    return;
  static_cast<RendererGamepadProvider*>(observer)->SampleGamepads(gamepads);
}

//------------------------------------------------------------------------------

std::unique_ptr<WebMediaRecorderHandler>
RendererBlinkPlatformImpl::CreateMediaRecorderHandler() {
#if BUILDFLAG(ENABLE_WEBRTC)
  return std::make_unique<content::MediaRecorderHandler>();
#else
  return nullptr;
#endif
}

//------------------------------------------------------------------------------

std::unique_ptr<WebRTCPeerConnectionHandler>
RendererBlinkPlatformImpl::CreateRTCPeerConnectionHandler(
    WebRTCPeerConnectionHandlerClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread);
  if (!render_thread)
    return nullptr;

#if BUILDFLAG(ENABLE_WEBRTC)
  PeerConnectionDependencyFactory* rtc_dependency_factory =
      render_thread->GetPeerConnectionDependencyFactory();
  return rtc_dependency_factory->CreateRTCPeerConnectionHandler(client,
                                                                task_runner);
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_WEBRTC)
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebRTCCertificateGenerator>
RendererBlinkPlatformImpl::CreateRTCCertificateGenerator() {
#if BUILDFLAG(ENABLE_WEBRTC)
  return std::make_unique<RTCCertificateGenerator>();
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_WEBRTC)
}

//------------------------------------------------------------------------------

std::unique_ptr<WebMediaStreamCenter>
RendererBlinkPlatformImpl::CreateMediaStreamCenter(
    WebMediaStreamCenterClient* client) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread);
  if (!render_thread)
    return nullptr;
  return render_thread->CreateMediaStreamCenter(client);
}

// static
bool RendererBlinkPlatformImpl::SetSandboxEnabledForTesting(bool enable) {
  bool was_enabled = g_sandbox_enabled;
  g_sandbox_enabled = enable;
  return was_enabled;
}

//------------------------------------------------------------------------------

std::unique_ptr<WebCanvasCaptureHandler>
RendererBlinkPlatformImpl::CreateCanvasCaptureHandler(
    const WebSize& size,
    double frame_rate,
    WebMediaStreamTrack* track) {
#if BUILDFLAG(ENABLE_WEBRTC)
  return CanvasCaptureHandler::CreateCanvasCaptureHandler(
      size, frame_rate, RenderThread::Get()->GetIOTaskRunner(), track);
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_WEBRTC)
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::CreateHTMLVideoElementCapturer(
    WebMediaStream* web_media_stream,
    WebMediaPlayer* web_media_player,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
#if BUILDFLAG(ENABLE_WEBRTC)
  DCHECK(web_media_stream);
  DCHECK(web_media_player);
  AddVideoTrackToMediaStream(
      HtmlVideoElementCapturerSource::CreateFromWebMediaPlayerImpl(
          web_media_player, content::RenderThread::Get()->GetIOTaskRunner(),
          task_runner),
      false,  // is_remote
      web_media_stream);
#endif
}

void RendererBlinkPlatformImpl::CreateHTMLAudioElementCapturer(
    WebMediaStream* web_media_stream,
    WebMediaPlayer* web_media_player) {
#if BUILDFLAG(ENABLE_WEBRTC)
  DCHECK(web_media_stream);
  DCHECK(web_media_player);

  blink::WebMediaStreamSource web_media_stream_source;
  blink::WebMediaStreamTrack web_media_stream_track;
  const WebString track_id = WebString::FromUTF8(base::GenerateGUID());

  web_media_stream_source.Initialize(track_id,
                                     blink::WebMediaStreamSource::kTypeAudio,
                                     track_id, false /* is_remote */);
  web_media_stream_track.Initialize(web_media_stream_source);

  MediaStreamAudioSource* const media_stream_source =
      HtmlAudioElementCapturerSource::CreateFromWebMediaPlayerImpl(
          web_media_player);

  // Takes ownership of |media_stream_source|.
  web_media_stream_source.SetExtraData(media_stream_source);

  media_stream_source->ConnectToTrack(web_media_stream_track);
  web_media_stream->AddTrack(web_media_stream_track);
#endif
}

//------------------------------------------------------------------------------

std::unique_ptr<WebImageCaptureFrameGrabber>
RendererBlinkPlatformImpl::CreateImageCaptureFrameGrabber() {
#if BUILDFLAG(ENABLE_WEBRTC)
  return std::make_unique<ImageCaptureFrameGrabber>();
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_WEBRTC)
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebSocketHandshakeThrottle>
RendererBlinkPlatformImpl::CreateWebSocketHandshakeThrottle() {
  return GetContentClient()->renderer()->CreateWebSocketHandshakeThrottle();
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebSpeechSynthesizer>
RendererBlinkPlatformImpl::CreateSpeechSynthesizer(
    blink::WebSpeechSynthesizerClient* client) {
  return GetContentClient()->renderer()->OverrideSpeechSynthesizer(client);
}

//------------------------------------------------------------------------------

static void Collect3DContextInformation(
    blink::Platform::GraphicsInfo* gl_info,
    const gpu::GPUInfo& gpu_info) {
  DCHECK(gl_info);
  gl_info->vendor_id = gpu_info.gpu.vendor_id;
  gl_info->device_id = gpu_info.gpu.device_id;
  switch (gpu_info.context_info_state) {
    case gpu::kCollectInfoSuccess:
    case gpu::kCollectInfoNonFatalFailure:
      gl_info->renderer_info = WebString::FromUTF8(gpu_info.gl_renderer);
      gl_info->vendor_info = WebString::FromUTF8(gpu_info.gl_vendor);
      gl_info->driver_version = WebString::FromUTF8(gpu_info.driver_version);
      gl_info->reset_notification_strategy =
          gpu_info.gl_reset_notification_strategy;
      gl_info->sandboxed = gpu_info.sandboxed;
      gl_info->process_crash_count = gpu_info.process_crash_count;
      gl_info->amd_switchable = gpu_info.amd_switchable;
      gl_info->optimus = gpu_info.optimus;
      break;
    case gpu::kCollectInfoFatalFailure:
    case gpu::kCollectInfoNone:
      gl_info->error_message = WebString::FromUTF8(
          "Failed to collect gpu information, GLSurface or GLContext "
          "creation failed");
      break;
  }
}

std::unique_ptr<blink::WebGraphicsContext3DProvider>
RendererBlinkPlatformImpl::CreateOffscreenGraphicsContext3DProvider(
    const blink::Platform::ContextAttributes& web_attributes,
    const blink::WebURL& top_document_web_url,
    blink::WebGraphicsContext3DProvider* share_provider,
    blink::Platform::GraphicsInfo* gl_info) {
  DCHECK(gl_info);
  if (!RenderThreadImpl::current()) {
    std::string error_message("Failed to run in Current RenderThreadImpl");
    gl_info->error_message = WebString::FromUTF8(error_message);
    return nullptr;
  }

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      RenderThreadImpl::current()->EstablishGpuChannelSync());
  if (!gpu_channel_host) {
    std::string error_message(
        "OffscreenContext Creation failed, GpuChannelHost creation failed");
    gl_info->error_message = WebString::FromUTF8(error_message);
    return nullptr;
  }
  Collect3DContextInformation(gl_info, gpu_channel_host->gpu_info());

  content::WebGraphicsContext3DProviderImpl* share_provider_impl =
      static_cast<content::WebGraphicsContext3DProviderImpl*>(share_provider);
  ui::ContextProviderCommandBuffer* share_context = nullptr;

  // WebGL contexts must fail creation if the share group is lost.
  if (share_provider_impl) {
    auto* gl = share_provider_impl->ContextGL();
    if (gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
      std::string error_message(
          "OffscreenContext Creation failed, Shared context is lost");
      gl_info->error_message = WebString::FromUTF8(error_message);
      return nullptr;
    }
    share_context = share_provider_impl->context_provider();
  }

  bool is_software_rendering = gpu_channel_host->gpu_info().software_rendering;

  // This is an offscreen context. Generally it won't use the default
  // frame buffer, in that case don't request any alpha, depth, stencil,
  // antialiasing. But we do need those attributes for the "own
  // offscreen surface" optimization which supports directly drawing
  // to a custom surface backed frame buffer.
  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = web_attributes.support_alpha ? 8 : -1;
  attributes.depth_size = web_attributes.support_depth ? 24 : 0;
  attributes.stencil_size = web_attributes.support_stencil ? 8 : 0;
  attributes.samples = web_attributes.support_antialias ? 4 : 0;
  attributes.own_offscreen_surface =
      web_attributes.support_alpha || web_attributes.support_depth ||
      web_attributes.support_stencil || web_attributes.support_antialias;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  // Prefer discrete GPU for WebGL.
  attributes.gpu_preference = gl::PreferDiscreteGpu;

  attributes.fail_if_major_perf_caveat =
      web_attributes.fail_if_major_performance_caveat;
  DCHECK_GT(web_attributes.web_gl_version, 0u);
  DCHECK_LE(web_attributes.web_gl_version, 2u);
  if (web_attributes.web_gl_version == 2)
    attributes.context_type = gpu::CONTEXT_TYPE_WEBGL2;
  else
    attributes.context_type = gpu::CONTEXT_TYPE_WEBGL1;

  constexpr bool automatic_flushes = true;
  constexpr bool support_locking = false;

  scoped_refptr<ui::ContextProviderCommandBuffer> provider(
      new ui::ContextProviderCommandBuffer(
          std::move(gpu_channel_host),
          RenderThreadImpl::current()->GetGpuMemoryBufferManager(),
          kGpuStreamIdDefault, kGpuStreamPriorityDefault,
          gpu::kNullSurfaceHandle, GURL(top_document_web_url),
          automatic_flushes, support_locking, gpu::SharedMemoryLimits(),
          attributes, share_context,
          ui::command_buffer_metrics::OFFSCREEN_CONTEXT_FOR_WEBGL));
  return std::make_unique<WebGraphicsContext3DProviderImpl>(
      std::move(provider), is_software_rendering);
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebGraphicsContext3DProvider>
RendererBlinkPlatformImpl::CreateSharedOffscreenGraphicsContext3DProvider() {
  auto* thread = RenderThreadImpl::current();

  scoped_refptr<ui::ContextProviderCommandBuffer> provider =
      thread->SharedMainThreadContextProvider();
  if (!provider)
    return nullptr;

  scoped_refptr<gpu::GpuChannelHost> host = thread->EstablishGpuChannelSync();
  // This shouldn't normally fail because we just got |provider|. But the
  // channel can become lost on the IO thread since then. It is important that
  // this happens after getting |provider|. In the case that this GpuChannelHost
  // is not the same one backing |provider|, the context behind the |provider|
  // will be already lost/dead on arrival, so the value we get for
  // |is_software_rendering| will never be wrong.
  if (!host)
    return nullptr;

  bool is_software_rendering = host->gpu_info().software_rendering;

  return std::make_unique<WebGraphicsContext3DProviderImpl>(
      std::move(provider), is_software_rendering);
}

//------------------------------------------------------------------------------

gpu::GpuMemoryBufferManager*
RendererBlinkPlatformImpl::GetGpuMemoryBufferManager() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread ? thread->GetGpuMemoryBufferManager() : nullptr;
}

//------------------------------------------------------------------------------

std::unique_ptr<viz::SharedBitmap>
RendererBlinkPlatformImpl::AllocateSharedBitmap(const blink::WebSize& size) {
  return shared_bitmap_manager_
      ->AllocateSharedBitmap(gfx::Size(size.width, size.height));
}

//------------------------------------------------------------------------------

blink::WebCompositorSupport* RendererBlinkPlatformImpl::CompositorSupport() {
  return &compositor_support_;
}

//------------------------------------------------------------------------------

blink::WebString RendererBlinkPlatformImpl::ConvertIDNToUnicode(
    const blink::WebString& host) {
  return WebString::FromUTF16(url_formatter::IDNToUnicode(host.Utf8()));
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::RecordRappor(const char* metric,
                                             const blink::WebString& sample) {
  GetContentClient()->renderer()->RecordRappor(metric, sample.Utf8());
}

void RendererBlinkPlatformImpl::RecordRapporURL(const char* metric,
                                                const blink::WebURL& url) {
  GetContentClient()->renderer()->RecordRapporURL(metric, url);
}

//------------------------------------------------------------------------------

// static
void RendererBlinkPlatformImpl::SetMockDeviceMotionDataForTesting(
    const device::MotionData& data) {
  g_test_device_motion_data.Get() = data;
}

//------------------------------------------------------------------------------

// static
void RendererBlinkPlatformImpl::SetMockDeviceOrientationDataForTesting(
    const device::OrientationData& data) {
  g_test_device_orientation_data.Get() = data;
}

//------------------------------------------------------------------------------

// static
std::unique_ptr<PlatformEventObserverBase>
RendererBlinkPlatformImpl::CreatePlatformEventObserverFromType(
    blink::WebPlatformEventType type) {
  RenderThread* thread = RenderThreadImpl::current();

  // When running layout tests, those observers should not listen to the actual
  // hardware changes. In order to make that happen, they will receive a null
  // thread.
  if (thread && RenderThreadImpl::current()->layout_test_mode())
    thread = nullptr;

  switch (type) {
    case blink::kWebPlatformEventTypeDeviceMotion:
      return std::make_unique<DeviceMotionEventPump>(thread);
    case blink::kWebPlatformEventTypeDeviceOrientation:
      return std::make_unique<DeviceOrientationEventPump>(thread,
                                                          false /* absolute */);
    case blink::kWebPlatformEventTypeDeviceOrientationAbsolute:
      return std::make_unique<DeviceOrientationEventPump>(thread,
                                                          true /* absolute */);
    case blink::kWebPlatformEventTypeGamepad:
      return std::make_unique<GamepadSharedMemoryReader>(thread);
    default:
      // A default statement is required to prevent compilation errors when
      // Blink adds a new type.
      DVLOG(1) << "RendererBlinkPlatformImpl::startListening() with "
                  "unknown type.";
  }

  return nullptr;
}

void RendererBlinkPlatformImpl::SetPlatformEventObserverForTesting(
    blink::WebPlatformEventType type,
    std::unique_ptr<PlatformEventObserverBase> observer) {
  if (platform_event_observers_.Lookup(type))
    platform_event_observers_.Remove(type);
  platform_event_observers_.AddWithID(std::move(observer), type);
}

service_manager::Connector* RendererBlinkPlatformImpl::GetConnector() {
  return connector_.get();
}

blink::InterfaceProvider* RendererBlinkPlatformImpl::GetInterfaceProvider() {
  return blink_interface_provider_.get();
}

void RendererBlinkPlatformImpl::StartListening(
    blink::WebPlatformEventType type,
    blink::WebPlatformEventListener* listener) {
  PlatformEventObserverBase* observer = platform_event_observers_.Lookup(type);
  if (!observer) {
    std::unique_ptr<PlatformEventObserverBase> new_observer =
        CreatePlatformEventObserverFromType(type);
    if (!new_observer)
      return;
    observer = new_observer.get();
    platform_event_observers_.AddWithID(std::move(new_observer),
                                        static_cast<int32_t>(type));
  }
  observer->Start(listener);

  // Device events (motion and orientation) expect to get an event fired
  // as soon as a listener is registered if a fake data was passed before.
  // TODO(mlamouri,timvolodine): make those send mock values directly instead of
  // using this broken pattern.
  if (RenderThreadImpl::current() &&
      RenderThreadImpl::current()->layout_test_mode() &&
      (type == blink::kWebPlatformEventTypeDeviceMotion ||
       type == blink::kWebPlatformEventTypeDeviceOrientation ||
       type == blink::kWebPlatformEventTypeDeviceOrientationAbsolute)) {
    SendFakeDeviceEventDataForTesting(type);
  }
}

void RendererBlinkPlatformImpl::SendFakeDeviceEventDataForTesting(
    blink::WebPlatformEventType type) {
  PlatformEventObserverBase* observer = platform_event_observers_.Lookup(type);
  CHECK(observer);

  void* data = nullptr;

  switch (type) {
    case blink::kWebPlatformEventTypeDeviceMotion:
      if (g_test_device_motion_data.IsCreated())
        data = &g_test_device_motion_data.Get();
      break;
    case blink::kWebPlatformEventTypeDeviceOrientation:
    case blink::kWebPlatformEventTypeDeviceOrientationAbsolute:
      if (g_test_device_orientation_data.IsCreated())
        data = &g_test_device_orientation_data.Get();
      break;
    default:
      NOTREACHED();
      break;
  }

  if (!data)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PlatformEventObserverBase::SendFakeDataForTesting,
                     base::Unretained(observer), data));
}

void RendererBlinkPlatformImpl::StopListening(
    blink::WebPlatformEventType type) {
  PlatformEventObserverBase* observer = platform_event_observers_.Lookup(type);
  if (!observer)
    return;
  observer->Stop();
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::QueryStorageUsageAndQuota(
    const blink::WebSecurityOrigin& storage_partition,
    blink::mojom::StorageType type,
    QueryStorageUsageAndQuotaCallback callback) {
  QuotaDispatcher::ThreadSpecificInstance(default_task_runner_)
      ->QueryStorageUsageAndQuota(storage_partition, type, std::move(callback));
}

//------------------------------------------------------------------------------

blink::WebPushProvider* RendererBlinkPlatformImpl::PushProvider() {
  return PushProvider::ThreadSpecificInstance(default_task_runner_);
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebTrialTokenValidator>
RendererBlinkPlatformImpl::CreateTrialTokenValidator() {
  return std::make_unique<WebTrialTokenValidatorImpl>(
      TrialPolicyImpl::CreateValidatorForPolicy());
}

//------------------------------------------------------------------------------

blink::WebNotificationManager*
RendererBlinkPlatformImpl::GetWebNotificationManager() {
  if (!thread_safe_sender_.get() || !notification_dispatcher_.get())
    return nullptr;

  return NotificationManager::ThreadSpecificInstance(
      thread_safe_sender_.get(),
      notification_dispatcher_.get());
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::DidStartWorkerThread() {
  WorkerThreadRegistry::Instance()->DidStartCurrentWorkerThread();
}

void RendererBlinkPlatformImpl::WillStopWorkerThread() {
  WorkerThreadRegistry::Instance()->WillStopCurrentWorkerThread();
}

void RendererBlinkPlatformImpl::WorkerContextCreated(
    const v8::Local<v8::Context>& worker) {
  GetContentClient()->renderer()->DidInitializeWorkerContextOnWorkerThread(
      worker);
}

//------------------------------------------------------------------------------
void RendererBlinkPlatformImpl::RequestPurgeMemory() {
  // TODO(tasak|bashi): We should use ChildMemoryCoordinator here, but
  // ChildMemoryCoordinator isn't always available as it's only initialized
  // when kMemoryCoordinatorV0 is enabled.
  // Use ChildMemoryCoordinator when memory coordinator is always enabled.
  base::MemoryCoordinatorClientRegistry::GetInstance()->PurgeMemory();
}

void RendererBlinkPlatformImpl::InitializeWebDatabaseHostIfNeeded() {
  if (!web_database_host_) {
    web_database_host_ = blink::mojom::ThreadSafeWebDatabaseHostPtr::Create(
        std::move(web_database_host_info_),
        base::CreateSequencedTaskRunnerWithTraits(
            {base::WithBaseSyncPrimitives()}));
  }
}

blink::mojom::WebDatabaseHost& RendererBlinkPlatformImpl::GetWebDatabaseHost() {
  InitializeWebDatabaseHostIfNeeded();
  return **web_database_host_;
}

}  // namespace content
