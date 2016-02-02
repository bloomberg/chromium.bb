// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_blink_platform_impl.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/blink/context_provider_web_context.h"
#include "components/scheduler/child/web_scheduler_impl.h"
#include "components/scheduler/child/web_task_runner_impl.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "components/scheduler/renderer/webthread_impl_for_renderer_scheduler.h"
#include "components/url_formatter/url_formatter.h"
#include "content/child/database_util.h"
#include "content/child/file_info_util.h"
#include "content/child/fileapi/webfilesystem_impl.h"
#include "content/child/indexed_db/webidbfactory_impl.h"
#include "content/child/npapi/npobject_util.h"
#include "content/child/quota_dispatcher.h"
#include "content/child/quota_message_filter.h"
#include "content/child/simple_webmimeregistry_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/web_database_observer_impl.h"
#include "content/child/web_url_loader_impl.h"
#include "content/child/webblobregistry_impl.h"
#include "content/child/webfileutilities_impl.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/file_utilities_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/mime_registry_messages.h"
#include "content/common/render_process_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_registry.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/media_stream_api.h"
#include "content/renderer/battery_status/battery_status_dispatcher.h"
#include "content/renderer/cache_storage/webserviceworkercachestorage_impl.h"
#include "content/renderer/device_sensors/device_light_event_pump.h"
#include "content/renderer/device_sensors/device_motion_event_pump.h"
#include "content/renderer/device_sensors/device_orientation_absolute_event_pump.h"
#include "content/renderer/device_sensors/device_orientation_event_pump.h"
#include "content/renderer/dom_storage/webstoragenamespace_impl.h"
#include "content/renderer/gamepad_shared_memory_reader.h"
#include "content/renderer/media/audio_decoder.h"
#include "content/renderer/media/canvas_capture_handler.h"
#include "content/renderer/media/html_video_element_capturer_source.h"
#include "content/renderer/media/media_recorder_handler.h"
#include "content/renderer/media/renderer_webaudiodevice_impl.h"
#include "content/renderer/media/renderer_webmidiaccessor_impl.h"
#include "content/renderer/media/rtc_certificate_generator.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_clipboard_delegate.h"
#include "content/renderer/screen_orientation/screen_orientation_observer.h"
#include "content/renderer/webclipboard_impl.h"
#include "content/renderer/webgraphicscontext3d_provider_impl.h"
#include "content/renderer/webpublicsuffixlist_impl.h"
#include "gpu/config/gpu_info.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/audio/audio_output_device.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/key_systems.h"
#include "media/base/mime_util.h"
#include "media/blink/webcontentdecryptionmodule_impl.h"
#include "media/filters/stream_parser_factory.h"
#include "storage/common/database/database_identifier.h"
#include "storage/common/quota/quota_types.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebBatteryStatusListener.h"
#include "third_party/WebKit/public/platform/WebBlobRegistry.h"
#include "third_party/WebKit/public/platform/WebDeviceLightListener.h"
#include "third_party/WebKit/public/platform/WebFileInfo.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenter.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenterClient.h"
#include "third_party/WebKit/public/platform/WebPluginListBuilder.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceMotionListener.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceOrientationListener.h"
#include "ui/gfx/color_profile.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "content/renderer/android/synchronous_compositor_factory.h"
#include "gpu/blink/webgraphicscontext3d_in_process_command_buffer_impl.h"
#endif

#if defined(OS_MACOSX)
#include "content/common/mac/font_descriptor.h"
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
#include "content/common/child_process_sandbox_support_impl_linux.h"
#include "third_party/WebKit/public/platform/linux/WebFallbackFont.h"
#include "third_party/WebKit/public/platform/linux/WebSandboxSupport.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#endif
#endif

#if defined(OS_WIN)
#include "content/common/child_process_messages.h"
#endif

#if defined(USE_AURA)
#include "content/renderer/webscrollbarbehavior_impl_gtkoraura.h"
#elif !defined(OS_MACOSX)
#include "third_party/WebKit/public/platform/WebScrollbarBehavior.h"
#define WebScrollbarBehaviorImpl blink::WebScrollbarBehavior
#endif

#if defined(ENABLE_WEBRTC)
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#endif

using blink::Platform;
using blink::WebAudioDevice;
using blink::WebBlobRegistry;
using blink::WebCanvasCaptureHandler;
using blink::WebDatabaseObserver;
using blink::WebFileInfo;
using blink::WebFileSystem;
using blink::WebGamepad;
using blink::WebGamepads;
using blink::WebIDBFactory;
using blink::WebMIDIAccessor;
using blink::WebMediaPlayer;
using blink::WebMediaRecorderHandler;
using blink::WebMediaStream;
using blink::WebMediaStreamCenter;
using blink::WebMediaStreamCenterClient;
using blink::WebMediaStreamTrack;
using blink::WebMimeRegistry;
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
double g_test_device_light_data = -1;
base::LazyInstance<blink::WebDeviceMotionData>::Leaky
    g_test_device_motion_data = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<blink::WebDeviceOrientationData>::Leaky
    g_test_device_orientation_data = LAZY_INSTANCE_INITIALIZER;
// Set in startListening() when running layout tests, unset in stopListening(),
// not owned by us.
blink::WebBatteryStatusListener* g_test_battery_status_listener = nullptr;

}  // namespace

//------------------------------------------------------------------------------

class RendererBlinkPlatformImpl::MimeRegistry
    : public SimpleWebMimeRegistryImpl {
 public:
  blink::WebMimeRegistry::SupportsType supportsMediaMIMEType(
      const blink::WebString& mime_type,
      const blink::WebString& codecs,
      const blink::WebString& key_system) override;
  bool supportsMediaSourceMIMEType(const blink::WebString& mime_type,
                                   const blink::WebString& codecs) override;
  blink::WebString mimeTypeForExtension(
      const blink::WebString& file_extension) override;
};

class RendererBlinkPlatformImpl::FileUtilities : public WebFileUtilitiesImpl {
 public:
  explicit FileUtilities(ThreadSafeSender* sender)
      : thread_safe_sender_(sender) {}
  bool getFileInfo(const WebString& path, WebFileInfo& result) override;

 private:
  bool SendSyncMessageFromAnyThread(IPC::SyncMessage* msg) const;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
};

#if !defined(OS_ANDROID) && !defined(OS_WIN)
class RendererBlinkPlatformImpl::SandboxSupport
    : public blink::WebSandboxSupport {
 public:
  virtual ~SandboxSupport() {}

#if defined(OS_MACOSX)
  bool loadFont(NSFont* src_font,
                CGFontRef* container,
                uint32_t* font_id) override;
#elif defined(OS_POSIX)
  void getFallbackFontForCharacter(
      blink::WebUChar32 character,
      const char* preferred_locale,
      blink::WebFallbackFont* fallbackFont) override;
  void getRenderStyleForStrike(const char* family,
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
    scheduler::RendererScheduler* renderer_scheduler)
    : BlinkPlatformImpl(renderer_scheduler->DefaultTaskRunner()),
      main_thread_(renderer_scheduler->CreateMainThread()),
      clipboard_delegate_(new RendererClipboardDelegate),
      clipboard_(new WebClipboardImpl(clipboard_delegate_.get())),
      mime_registry_(new RendererBlinkPlatformImpl::MimeRegistry),
      sudden_termination_disables_(0),
      plugin_refresh_allowed_(true),
      default_task_runner_(renderer_scheduler->DefaultTaskRunner()),
      loading_task_runner_(renderer_scheduler->LoadingTaskRunner()),
      web_scrollbar_behavior_(new WebScrollbarBehaviorImpl),
      renderer_scheduler_(renderer_scheduler) {
#if !defined(OS_ANDROID) && !defined(OS_WIN)
  if (g_sandbox_enabled && sandboxEnabled()) {
    sandbox_support_.reset(new RendererBlinkPlatformImpl::SandboxSupport);
  } else {
    DVLOG(1) << "Disabling sandbox support for testing.";
  }
#endif

  // ChildThread may not exist in some tests.
  if (ChildThreadImpl::current()) {
    sync_message_filter_ = ChildThreadImpl::current()->sync_message_filter();
    thread_safe_sender_ = ChildThreadImpl::current()->thread_safe_sender();
    quota_message_filter_ = ChildThreadImpl::current()->quota_message_filter();
    blob_registry_.reset(new WebBlobRegistryImpl(thread_safe_sender_.get()));
    web_idb_factory_.reset(new WebIDBFactoryImpl(thread_safe_sender_.get()));
    web_database_observer_impl_.reset(
        new WebDatabaseObserverImpl(sync_message_filter_.get()));
  }
}

RendererBlinkPlatformImpl::~RendererBlinkPlatformImpl() {
  WebFileSystemImpl::DeleteThreadSpecificInstance();
}

void RendererBlinkPlatformImpl::Shutdown() {
#if !defined(OS_ANDROID) && !defined(OS_WIN)
  // SandboxSupport contains a map of WebFontFamily objects, which hold
  // WebCStrings, which become invalidated when blink is shut down. Hence, we
  // need to clear that map now, just before blink::shutdown() is called.
  sandbox_support_.reset();
#endif
}

//------------------------------------------------------------------------------

double RendererBlinkPlatformImpl::currentTimeSeconds() {
  // TODO(alexclarke): Change the way this is plumbed through to blink.
  return renderer_scheduler_->VirtualTimeSeconds();
}

double RendererBlinkPlatformImpl::monotonicallyIncreasingTimeSeconds() {
  // TODO(alexclarke): Change the way this is plumbed through to blink.
  return renderer_scheduler_->MonotonicallyIncreasingVirtualTimeSeconds();
}

//------------------------------------------------------------------------------

blink::WebURLLoader* RendererBlinkPlatformImpl::createURLLoader() {
  ChildThreadImpl* child_thread = ChildThreadImpl::current();
  // There may be no child thread in RenderViewTests.  These tests can still use
  // data URLs to bypass the ResourceDispatcher.
  scoped_ptr<scheduler::WebTaskRunnerImpl> task_runner(
      new scheduler::WebTaskRunnerImpl(
        loading_task_runner_->BelongsToCurrentThread()
            ? loading_task_runner_ : base::ThreadTaskRunnerHandle::Get()));
  return new content::WebURLLoaderImpl(
      child_thread ? child_thread->resource_dispatcher() : NULL,
      std::move(task_runner));
}

blink::WebThread* RendererBlinkPlatformImpl::currentThread() {
  if (main_thread_->isCurrentThread())
    return main_thread_.get();
  return BlinkPlatformImpl::currentThread();
}

blink::WebClipboard* RendererBlinkPlatformImpl::clipboard() {
  blink::WebClipboard* clipboard =
      GetContentClient()->renderer()->OverrideWebClipboard();
  if (clipboard)
    return clipboard;
  return clipboard_.get();
}

blink::WebMimeRegistry* RendererBlinkPlatformImpl::mimeRegistry() {
  return mime_registry_.get();
}

blink::WebFileUtilities* RendererBlinkPlatformImpl::fileUtilities() {
  if (!file_utilities_) {
    file_utilities_.reset(new FileUtilities(thread_safe_sender_.get()));
    file_utilities_->set_sandbox_enabled(sandboxEnabled());
  }
  return file_utilities_.get();
}

blink::WebSandboxSupport* RendererBlinkPlatformImpl::sandboxSupport() {
#if defined(OS_ANDROID) || defined(OS_WIN)
  // These platforms do not require sandbox support.
  return NULL;
#else
  return sandbox_support_.get();
#endif
}

blink::WebCookieJar* RendererBlinkPlatformImpl::cookieJar() {
  NOTREACHED() << "Use WebFrameClient::cookieJar() instead!";
  return NULL;
}

blink::WebThemeEngine* RendererBlinkPlatformImpl::themeEngine() {
  blink::WebThemeEngine* theme_engine =
      GetContentClient()->renderer()->OverrideThemeEngine();
  if (theme_engine)
    return theme_engine;
  return BlinkPlatformImpl::themeEngine();
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

unsigned long long RendererBlinkPlatformImpl::visitedLinkHash(
    const char* canonical_url,
    size_t length) {
  return GetContentClient()->renderer()->VisitedLinkHash(canonical_url, length);
}

bool RendererBlinkPlatformImpl::isLinkVisited(unsigned long long link_hash) {
  return GetContentClient()->renderer()->IsLinkVisited(link_hash);
}

void RendererBlinkPlatformImpl::createMessageChannel(
    blink::WebMessagePortChannel** channel1,
    blink::WebMessagePortChannel** channel2) {
  WebMessagePortChannelImpl::CreatePair(
      default_task_runner_, channel1, channel2);
}

blink::WebPrescientNetworking*
RendererBlinkPlatformImpl::prescientNetworking() {
  return GetContentClient()->renderer()->GetPrescientNetworking();
}

void RendererBlinkPlatformImpl::cacheMetadata(const blink::WebURL& url,
                                              int64_t response_time,
                                              const char* data,
                                              size_t size) {
  // Let the browser know we generated cacheable metadata for this resource. The
  // browser may cache it and return it on subsequent responses to speed
  // the processing of this resource.
  std::vector<char> copy(data, data + size);
  RenderThread::Get()->Send(
      new RenderProcessHostMsg_DidGenerateCacheableMetadata(
          url, base::Time::FromInternalValue(response_time), copy));
}

WebString RendererBlinkPlatformImpl::defaultLocale() {
  return base::ASCIIToUTF16(RenderThread::Get()->GetLocale());
}

void RendererBlinkPlatformImpl::suddenTerminationChanged(bool enabled) {
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

  RenderThread* thread = RenderThread::Get();
  if (thread)  // NULL in unittests.
    thread->Send(new RenderProcessHostMsg_SuddenTerminationChanged(enabled));
}

WebStorageNamespace* RendererBlinkPlatformImpl::createLocalStorageNamespace() {
  return new WebStorageNamespaceImpl();
}


//------------------------------------------------------------------------------

WebIDBFactory* RendererBlinkPlatformImpl::idbFactory() {
  return web_idb_factory_.get();
}

//------------------------------------------------------------------------------

blink::WebServiceWorkerCacheStorage* RendererBlinkPlatformImpl::cacheStorage(
    const WebString& origin_identifier) {
  const GURL origin =
      storage::GetOriginFromIdentifier(origin_identifier.utf8());
  return new WebServiceWorkerCacheStorageImpl(thread_safe_sender_.get(),
                                              origin);
}

//------------------------------------------------------------------------------

WebFileSystem* RendererBlinkPlatformImpl::fileSystem() {
  return WebFileSystemImpl::ThreadSpecificInstance(default_task_runner_);
}

//------------------------------------------------------------------------------

WebMimeRegistry::SupportsType
RendererBlinkPlatformImpl::MimeRegistry::supportsMediaMIMEType(
    const WebString& mime_type,
    const WebString& codecs,
    const WebString& key_system) {
  const std::string mime_type_ascii = ToASCIIOrEmpty(mime_type);

  if (!key_system.isEmpty()) {
    // Check whether the key system is supported with the mime_type and codecs.

    // Chromium only supports ASCII parameters.
    if (!base::IsStringASCII(key_system))
      return IsNotSupported;

    std::string key_system_ascii =
        media::GetUnprefixedKeySystemName(base::UTF16ToASCII(
            base::StringPiece16(key_system)));
    std::vector<std::string> codec_vector;
    media::ParseCodecString(ToASCIIOrEmpty(codecs), &codec_vector, true);

    if (!media::PrefixedIsSupportedKeySystemWithMediaMimeType(
            mime_type_ascii, codec_vector, key_system_ascii)) {
      return IsNotSupported;
    }

    // Continue processing the mime_type and codecs.
  }

  std::vector<std::string> codec_vector;
  media::ParseCodecString(ToASCIIOrEmpty(codecs), &codec_vector, false);
  return static_cast<WebMimeRegistry::SupportsType>(
      media::IsSupportedMediaFormat(mime_type_ascii, codec_vector));
}

bool RendererBlinkPlatformImpl::MimeRegistry::supportsMediaSourceMIMEType(
    const blink::WebString& mime_type,
    const WebString& codecs) {
  const std::string mime_type_ascii = ToASCIIOrEmpty(mime_type);
  std::vector<std::string> parsed_codec_ids;
  media::ParseCodecString(ToASCIIOrEmpty(codecs), &parsed_codec_ids, false);
  if (mime_type_ascii.empty())
    return false;
  return media::StreamParserFactory::IsTypeSupported(
      mime_type_ascii, parsed_codec_ids);
}

WebString RendererBlinkPlatformImpl::MimeRegistry::mimeTypeForExtension(
    const WebString& file_extension) {
  if (IsPluginProcess())
    return SimpleWebMimeRegistryImpl::mimeTypeForExtension(file_extension);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  std::string mime_type;
  RenderThread::Get()->Send(
      new MimeRegistryMsg_GetMimeTypeFromExtension(
          base::FilePath::FromUTF16Unsafe(file_extension).value(), &mime_type));
  return base::ASCIIToUTF16(mime_type);
}

//------------------------------------------------------------------------------

bool RendererBlinkPlatformImpl::FileUtilities::getFileInfo(
    const WebString& path,
    WebFileInfo& web_file_info) {
  base::File::Info file_info;
  base::File::Error status = base::File::FILE_ERROR_MAX;
  if (!SendSyncMessageFromAnyThread(new FileUtilitiesMsg_GetFileInfo(
           base::FilePath::FromUTF16Unsafe(path), &file_info, &status)) ||
      status != base::File::FILE_OK) {
    return false;
  }
  FileInfoToWebFileInfo(file_info, &web_file_info);
  web_file_info.platformPath = path;
  return true;
}

bool RendererBlinkPlatformImpl::FileUtilities::SendSyncMessageFromAnyThread(
    IPC::SyncMessage* msg) const {
  base::TimeTicks begin = base::TimeTicks::Now();
  const bool success = thread_safe_sender_->Send(msg);
  base::TimeDelta delta = base::TimeTicks::Now() - begin;
  UMA_HISTOGRAM_TIMES("RendererSyncIPC.ElapsedTime", delta);
  return success;
}

//------------------------------------------------------------------------------

#if defined(OS_MACOSX)

bool RendererBlinkPlatformImpl::SandboxSupport::loadFont(NSFont* src_font,
                                                         CGFontRef* out,
                                                         uint32_t* font_id) {
  uint32_t font_data_size;
  FontDescriptor src_font_descriptor(src_font);
  base::SharedMemoryHandle font_data;
  if (!RenderThread::Get()->Send(new RenderProcessHostMsg_LoadFont(
        src_font_descriptor, &font_data_size, &font_data, font_id))) {
    *out = NULL;
    *font_id = 0;
    return false;
  }

  if (font_data_size == 0 || font_data == base::SharedMemory::NULLHandle() ||
      *font_id == 0) {
    LOG(ERROR) << "Bad response from RenderProcessHostMsg_LoadFont() for " <<
        src_font_descriptor.font_name;
    *out = NULL;
    *font_id = 0;
    return false;
  }

  // TODO(jeremy): Need to call back into WebKit to make sure that the font
  // isn't already activated, based on the font id.  If it's already
  // activated, don't reactivate it here - crbug.com/72727 .

  return FontLoader::CGFontRefFromBuffer(font_data, font_data_size, out);
}

#elif defined(OS_POSIX) && !defined(OS_ANDROID)

void RendererBlinkPlatformImpl::SandboxSupport::getFallbackFontForCharacter(
    blink::WebUChar32 character,
    const char* preferred_locale,
    blink::WebFallbackFont* fallbackFont) {
  base::AutoLock lock(unicode_font_families_mutex_);
  const std::map<int32_t, blink::WebFallbackFont>::const_iterator iter =
      unicode_font_families_.find(character);
  if (iter != unicode_font_families_.end()) {
    fallbackFont->name = iter->second.name;
    fallbackFont->filename = iter->second.filename;
    fallbackFont->fontconfigInterfaceId = iter->second.fontconfigInterfaceId;
    fallbackFont->ttcIndex = iter->second.ttcIndex;
    fallbackFont->isBold = iter->second.isBold;
    fallbackFont->isItalic = iter->second.isItalic;
    return;
  }

  GetFallbackFontForCharacter(character, preferred_locale, fallbackFont);
  unicode_font_families_.insert(std::make_pair(character, *fallbackFont));
}

void RendererBlinkPlatformImpl::SandboxSupport::getRenderStyleForStrike(
    const char* family,
    int sizeAndStyle,
    blink::WebFontRenderStyle* out) {
  GetRenderStyleForStrike(family, sizeAndStyle, out);
}

#endif

//------------------------------------------------------------------------------

Platform::FileHandle RendererBlinkPlatformImpl::databaseOpenFile(
    const WebString& vfs_file_name,
    int desired_flags) {
  return DatabaseUtil::DatabaseOpenFile(
      vfs_file_name, desired_flags, sync_message_filter_.get());
}

int RendererBlinkPlatformImpl::databaseDeleteFile(
    const WebString& vfs_file_name,
    bool sync_dir) {
  return DatabaseUtil::DatabaseDeleteFile(
      vfs_file_name, sync_dir, sync_message_filter_.get());
}

long RendererBlinkPlatformImpl::databaseGetFileAttributes(
    const WebString& vfs_file_name) {
  return DatabaseUtil::DatabaseGetFileAttributes(vfs_file_name,
                                                 sync_message_filter_.get());
}

long long RendererBlinkPlatformImpl::databaseGetFileSize(
    const WebString& vfs_file_name) {
  return DatabaseUtil::DatabaseGetFileSize(vfs_file_name,
                                           sync_message_filter_.get());
}

long long RendererBlinkPlatformImpl::databaseGetSpaceAvailableForOrigin(
    const WebString& origin_identifier) {
  return DatabaseUtil::DatabaseGetSpaceAvailable(origin_identifier,
                                                 sync_message_filter_.get());
}

bool RendererBlinkPlatformImpl::databaseSetFileSize(
    const WebString& vfs_file_name, long long size) {
  return DatabaseUtil::DatabaseSetFileSize(
      vfs_file_name, size, sync_message_filter_.get());
}

bool RendererBlinkPlatformImpl::canAccelerate2dCanvas() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  GpuChannelHost* host = thread->EstablishGpuChannelSync(
      CAUSE_FOR_GPU_LAUNCH_CANVAS_2D);
  if (!host)
    return false;

  return host->gpu_info().SupportsAccelerated2dCanvas();
}

bool RendererBlinkPlatformImpl::isThreadedCompositingEnabled() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  // thread can be NULL in tests.
  return thread && thread->compositor_task_runner().get();
}

bool RendererBlinkPlatformImpl::isThreadedAnimationEnabled() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread ? thread->IsThreadedAnimationEnabled() : true;
}

double RendererBlinkPlatformImpl::audioHardwareSampleRate() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread->GetAudioHardwareConfig()->GetOutputSampleRate();
}

size_t RendererBlinkPlatformImpl::audioHardwareBufferSize() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread->GetAudioHardwareConfig()->GetOutputBufferSize();
}

unsigned RendererBlinkPlatformImpl::audioHardwareOutputChannels() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread->GetAudioHardwareConfig()->GetOutputChannels();
}

WebDatabaseObserver* RendererBlinkPlatformImpl::databaseObserver() {
  return web_database_observer_impl_.get();
}

WebAudioDevice* RendererBlinkPlatformImpl::createAudioDevice(
    size_t buffer_size,
    unsigned input_channels,
    unsigned channels,
    double sample_rate,
    WebAudioDevice::RenderCallback* callback,
    const blink::WebString& input_device_id) {
  // Use a mock for testing.
  blink::WebAudioDevice* mock_device =
      GetContentClient()->renderer()->OverrideCreateAudioDevice(sample_rate);
  if (mock_device)
    return mock_device;

  // The |channels| does not exactly identify the channel layout of the
  // device. The switch statement below assigns a best guess to the channel
  // layout based on number of channels.
  media::ChannelLayout layout = media::CHANNEL_LAYOUT_UNSUPPORTED;
  switch (channels) {
    case 1:
      layout = media::CHANNEL_LAYOUT_MONO;
      break;
    case 2:
      layout = media::CHANNEL_LAYOUT_STEREO;
      break;
    case 3:
      layout = media::CHANNEL_LAYOUT_2_1;
      break;
    case 4:
      layout = media::CHANNEL_LAYOUT_4_0;
      break;
    case 5:
      layout = media::CHANNEL_LAYOUT_5_0;
      break;
    case 6:
      layout = media::CHANNEL_LAYOUT_5_1;
      break;
    case 7:
      layout = media::CHANNEL_LAYOUT_7_0;
      break;
    case 8:
      layout = media::CHANNEL_LAYOUT_7_1;
      break;
    default:
      // If the layout is not supported (more than 9 channels), falls back to
      // discrete mode.
      layout = media::CHANNEL_LAYOUT_DISCRETE;
  }

  int session_id = 0;
  if (input_device_id.isNull() ||
      !base::StringToInt(base::UTF16ToUTF8(
          base::StringPiece16(input_device_id)), &session_id)) {
    if (input_channels > 0)
      DLOG(WARNING) << "createAudioDevice(): request for audio input ignored";

    input_channels = 0;
  }

  // For CHANNEL_LAYOUT_DISCRETE, pass the explicit channel count along with
  // the channel layout when creating an |AudioParameters| object.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                layout, static_cast<int>(sample_rate), 16,
                                buffer_size);
  params.set_channels_for_discrete(channels);

  return new RendererWebAudioDeviceImpl(params, callback, session_id);
}

bool RendererBlinkPlatformImpl::loadAudioResource(
    blink::WebAudioBus* destination_bus,
    const char* audio_file_data,
    size_t data_size) {
  return DecodeAudioFileData(
      destination_bus, audio_file_data, data_size);
}

//------------------------------------------------------------------------------

blink::WebMIDIAccessor* RendererBlinkPlatformImpl::createMIDIAccessor(
    blink::WebMIDIAccessorClient* client) {
  blink::WebMIDIAccessor* accessor =
      GetContentClient()->renderer()->OverrideCreateMIDIAccessor(client);
  if (accessor)
    return accessor;

  return new RendererWebMIDIAccessorImpl(client);
}

void RendererBlinkPlatformImpl::getPluginList(
    bool refresh,
    blink::WebPluginListBuilder* builder) {
#if defined(ENABLE_PLUGINS)
  std::vector<WebPluginInfo> plugins;
  if (!plugin_refresh_allowed_)
    refresh = false;
  RenderThread::Get()->Send(new FrameHostMsg_GetPlugins(refresh, &plugins));
  for (const WebPluginInfo& plugin : plugins) {
    builder->addPlugin(
        plugin.name, plugin.desc,
        plugin.path.BaseName().AsUTF16Unsafe());

    for (const WebPluginMimeType& mime_type : plugin.mime_types) {
      builder->addMediaTypeToLastPlugin(
          WebString::fromUTF8(mime_type.mime_type), mime_type.description);

      for (const auto& extension : mime_type.file_extensions) {
        builder->addFileExtensionToLastMediaType(
            WebString::fromUTF8(extension));
      }
    }
  }
#endif
}

//------------------------------------------------------------------------------

blink::WebPublicSuffixList* RendererBlinkPlatformImpl::publicSuffixList() {
  return &public_suffix_list_;
}

//------------------------------------------------------------------------------

blink::WebString RendererBlinkPlatformImpl::signedPublicKeyAndChallengeString(
    unsigned key_size_index,
    const blink::WebString& challenge,
    const blink::WebURL& url,
    const blink::WebURL& top_origin) {
  std::string signed_public_key;
  RenderThread::Get()->Send(new RenderProcessHostMsg_Keygen(
      static_cast<uint32_t>(key_size_index), challenge.utf8(), GURL(url),
      GURL(top_origin), &signed_public_key));
  return WebString::fromUTF8(signed_public_key);
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::screenColorProfile(
    WebVector<char>* to_profile) {
#if defined(OS_WIN)
  // On Windows screen color profile is only available in the browser.
  std::vector<char> profile;
  // This Send() can be called from any impl-side thread. Use a thread
  // safe send to avoid crashing trying to access RenderThread::Get(),
  // which is not accessible from arbitrary threads.
  thread_safe_sender_->Send(
      new RenderProcessHostMsg_GetMonitorColorProfile(&profile));
  *to_profile = profile;
#else
  // On other platforms, the primary monitor color profile can be read
  // directly.
  gfx::ColorProfile profile;
  *to_profile = profile.profile();
#endif
}

//------------------------------------------------------------------------------

blink::WebScrollbarBehavior* RendererBlinkPlatformImpl::scrollbarBehavior() {
  return web_scrollbar_behavior_.get();
}

//------------------------------------------------------------------------------

WebBlobRegistry* RendererBlinkPlatformImpl::blobRegistry() {
  // blob_registry_ can be NULL when running some tests.
  return blob_registry_.get();
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::sampleGamepads(WebGamepads& gamepads) {
  PlatformEventObserverBase* observer =
      platform_event_observers_.Lookup(blink::WebPlatformEventTypeGamepad);
  if (!observer)
    return;
  static_cast<RendererGamepadProvider*>(observer)->SampleGamepads(gamepads);
}

//------------------------------------------------------------------------------

WebMediaRecorderHandler*
RendererBlinkPlatformImpl::createMediaRecorderHandler() {
#if defined(ENABLE_WEBRTC)
  return new content::MediaRecorderHandler();
#else
  return nullptr;
#endif
}

//------------------------------------------------------------------------------

WebRTCPeerConnectionHandler*
RendererBlinkPlatformImpl::createRTCPeerConnectionHandler(
    WebRTCPeerConnectionHandlerClient* client) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread);
  if (!render_thread)
    return NULL;

#if defined(ENABLE_WEBRTC)
  WebRTCPeerConnectionHandler* peer_connection_handler =
      GetContentClient()->renderer()->OverrideCreateWebRTCPeerConnectionHandler(
          client);
  if (peer_connection_handler)
    return peer_connection_handler;

  PeerConnectionDependencyFactory* rtc_dependency_factory =
      render_thread->GetPeerConnectionDependencyFactory();
  return rtc_dependency_factory->CreateRTCPeerConnectionHandler(client);
#else
  return NULL;
#endif  // defined(ENABLE_WEBRTC)
}

//------------------------------------------------------------------------------

blink::WebRTCCertificateGenerator*
RendererBlinkPlatformImpl::createRTCCertificateGenerator() {
#if defined(ENABLE_WEBRTC)
  return new RTCCertificateGenerator();
#else
  return nullptr;
#endif  // defined(ENABLE_WEBRTC)
}

//------------------------------------------------------------------------------

WebMediaStreamCenter* RendererBlinkPlatformImpl::createMediaStreamCenter(
    WebMediaStreamCenterClient* client) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread);
  if (!render_thread)
    return NULL;
  return render_thread->CreateMediaStreamCenter(client);
}

// static
bool RendererBlinkPlatformImpl::SetSandboxEnabledForTesting(bool enable) {
  bool was_enabled = g_sandbox_enabled;
  g_sandbox_enabled = enable;
  return was_enabled;
}

//------------------------------------------------------------------------------

WebCanvasCaptureHandler* RendererBlinkPlatformImpl::createCanvasCaptureHandler(
    const WebSize& size,
    double frame_rate,
    WebMediaStreamTrack* track) {
#if defined(ENABLE_WEBRTC)
  return CanvasCaptureHandler::CreateCanvasCaptureHandler(
      size, frame_rate, RenderThread::Get()->GetIOMessageLoopProxy(), track);
#else
  return nullptr;
#endif  // defined(ENABLE_WEBRTC)
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::createHTMLVideoElementCapturer(
    WebMediaStream* web_media_stream,
    WebMediaPlayer* web_media_player) {
#if defined(ENABLE_WEBRTC)
  DCHECK(web_media_stream);
  DCHECK(web_media_player);
  AddVideoTrackToMediaStream(
      HtmlVideoElementCapturerSource::CreateFromWebMediaPlayerImpl(
          web_media_player,
          content::RenderThread::Get()->GetIOMessageLoopProxy()),
      false /* is_remote */, false /* is_readonly */, web_media_stream);
#endif
}

//------------------------------------------------------------------------------

blink::WebSpeechSynthesizer* RendererBlinkPlatformImpl::createSpeechSynthesizer(
    blink::WebSpeechSynthesizerClient* client) {
  return GetContentClient()->renderer()->OverrideSpeechSynthesizer(client);
}

//------------------------------------------------------------------------------

blink::WebGraphicsContext3D*
RendererBlinkPlatformImpl::createOffscreenGraphicsContext3D(
    const blink::WebGraphicsContext3D::Attributes& attributes) {
  return createOffscreenGraphicsContext3D(attributes, NULL);
}

blink::WebGraphicsContext3D*
RendererBlinkPlatformImpl::createOffscreenGraphicsContext3D(
    const blink::WebGraphicsContext3D::Attributes& attributes,
    blink::WebGraphicsContext3D* share_context) {
  blink::WebGraphicsContext3D::WebGraphicsInfo gl_info;
  return createOffscreenGraphicsContext3D(attributes, share_context, &gl_info);
}

static void Collect3DContextInformationOnFailure(
    blink::WebGraphicsContext3D* share_context,
    blink::WebGraphicsContext3D::WebGraphicsInfo* gl_info,
    GpuChannelHost* host) {
  DCHECK(gl_info);
  std::string error_message("OffscreenContext Creation failed, ");
  if (host) {
    const gpu::GPUInfo& gpu_info = host->gpu_info();
    gl_info->vendorId = gpu_info.gpu.vendor_id;
    gl_info->deviceId = gpu_info.gpu.device_id;
    switch (gpu_info.context_info_state) {
      case gpu::kCollectInfoSuccess:
      case gpu::kCollectInfoNonFatalFailure:
        gl_info->rendererInfo = WebString::fromUTF8(gpu_info.gl_renderer);
        gl_info->vendorInfo = WebString::fromUTF8(gpu_info.gl_vendor);
        gl_info->driverVersion = WebString::fromUTF8(gpu_info.driver_version);
        gl_info->resetNotificationStrategy =
            gpu_info.gl_reset_notification_strategy;
        gl_info->sandboxed = gpu_info.sandboxed;
        gl_info->processCrashCount = gpu_info.process_crash_count;
        gl_info->amdSwitchable = gpu_info.amd_switchable;
        gl_info->optimus = gpu_info.optimus;
        break;
      case gpu::kCollectInfoFatalFailure:
      case gpu::kCollectInfoNone:
        error_message.append(
            "Failed to collect gpu information, GLSurface or GLContext "
            "creation failed");
        gl_info->errorMessage = WebString::fromUTF8(error_message);
        break;
      default:
        NOTREACHED();
    }
  } else {
    error_message.append("GpuChannelHost creation failed");
    gl_info->errorMessage = WebString::fromUTF8(error_message);
  }
}

blink::WebGraphicsContext3D*
RendererBlinkPlatformImpl::createOffscreenGraphicsContext3D(
    const blink::WebGraphicsContext3D::Attributes& attributes,
    blink::WebGraphicsContext3D* share_context,
    blink::WebGraphicsContext3D::WebGraphicsInfo* gl_info) {
  DCHECK(gl_info);
  if (!RenderThreadImpl::current()) {
    std::string error_message("Failed to run in Current RenderThreadImpl");
    gl_info->errorMessage = WebString::fromUTF8(error_message);
    return NULL;
  }

  scoped_refptr<GpuChannelHost> gpu_channel_host(
      RenderThreadImpl::current()->EstablishGpuChannelSync(
          CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE));

  WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits limits;
  bool lose_context_when_out_of_memory = false;
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context(
      WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
          gpu_channel_host.get(),
          attributes,
          lose_context_when_out_of_memory,
          blink::WebStringToGURL(attributes.topDocumentURL),
          limits,
          static_cast<WebGraphicsContext3DCommandBufferImpl*>(share_context)));

  // Most likely the GPU process exited and the attempt to reconnect to it
  // failed. Need to try to restore the context again later.
  if (!context || !context->InitializeOnCurrentThread() ||
      gl_info->testFailContext) {
    // Collect Graphicsinfo if there is a context failure or it is failed
    // purposefully in case of layout tests.
    Collect3DContextInformationOnFailure(share_context, gl_info,
                                         gpu_channel_host.get());
      return NULL;
  }
  return context.release();
}

//------------------------------------------------------------------------------

blink::WebGraphicsContext3DProvider*
RendererBlinkPlatformImpl::createSharedOffscreenGraphicsContext3DProvider() {
  scoped_refptr<cc_blink::ContextProviderWebContext> provider =
      RenderThreadImpl::current()->SharedMainThreadContextProvider();
  if (!provider.get())
    return NULL;
  return new WebGraphicsContext3DProviderImpl(provider);
}

//------------------------------------------------------------------------------

blink::WebCompositorSupport* RendererBlinkPlatformImpl::compositorSupport() {
  return &compositor_support_;
}

//------------------------------------------------------------------------------

blink::WebString RendererBlinkPlatformImpl::convertIDNToUnicode(
    const blink::WebString& host,
    const blink::WebString& languages) {
  return url_formatter::IDNToUnicode(host.utf8(), languages.utf8());
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::recordRappor(const char* metric,
                                             const blink::WebString& sample) {
  GetContentClient()->renderer()->RecordRappor(metric, sample.utf8());
}

void RendererBlinkPlatformImpl::recordRapporURL(const char* metric,
                                                const blink::WebURL& url) {
  GetContentClient()->renderer()->RecordRapporURL(metric, url);
}

//------------------------------------------------------------------------------

// static
void RendererBlinkPlatformImpl::SetMockDeviceLightDataForTesting(double data) {
  g_test_device_light_data = data;
}

//------------------------------------------------------------------------------

// static
void RendererBlinkPlatformImpl::SetMockDeviceMotionDataForTesting(
    const blink::WebDeviceMotionData& data) {
  g_test_device_motion_data.Get() = data;
}

//------------------------------------------------------------------------------

// static
void RendererBlinkPlatformImpl::SetMockDeviceOrientationDataForTesting(
    const blink::WebDeviceOrientationData& data) {
  g_test_device_orientation_data.Get() = data;
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::vibrate(unsigned int milliseconds) {
  GetConnectedVibrationManagerService()->Vibrate(
      base::checked_cast<int64_t>(milliseconds));
  vibration_manager_.reset();
}

void RendererBlinkPlatformImpl::cancelVibration() {
  GetConnectedVibrationManagerService()->Cancel();
  vibration_manager_.reset();
}

device::VibrationManagerPtr&
RendererBlinkPlatformImpl::GetConnectedVibrationManagerService() {
  if (!vibration_manager_) {
    RenderThread::Get()->GetServiceRegistry()->ConnectToRemoteService(
        mojo::GetProxy(&vibration_manager_));
  }
  return vibration_manager_;
}

//------------------------------------------------------------------------------

// static
PlatformEventObserverBase*
RendererBlinkPlatformImpl::CreatePlatformEventObserverFromType(
    blink::WebPlatformEventType type) {
  RenderThread* thread = RenderThreadImpl::current();

  // When running layout tests, those observers should not listen to the actual
  // hardware changes. In order to make that happen, they will receive a null
  // thread.
  if (thread && RenderThreadImpl::current()->layout_test_mode())
    thread = NULL;

  switch (type) {
    case blink::WebPlatformEventTypeDeviceMotion:
      return new DeviceMotionEventPump(thread);
    case blink::WebPlatformEventTypeDeviceOrientation:
      return new DeviceOrientationEventPump(thread);
    case blink::WebPlatformEventTypeDeviceOrientationAbsolute:
      return new DeviceOrientationAbsoluteEventPump(thread);
    case blink::WebPlatformEventTypeDeviceLight:
      return new DeviceLightEventPump(thread);
    case blink::WebPlatformEventTypeGamepad:
      return new GamepadSharedMemoryReader(thread);
    case blink::WebPlatformEventTypeScreenOrientation:
      return new ScreenOrientationObserver();
    default:
      // A default statement is required to prevent compilation errors when
      // Blink adds a new type.
      DVLOG(1) << "RendererBlinkPlatformImpl::startListening() with "
                  "unknown type.";
  }

  return NULL;
}

void RendererBlinkPlatformImpl::SetPlatformEventObserverForTesting(
    blink::WebPlatformEventType type,
    scoped_ptr<PlatformEventObserverBase> observer) {
  DCHECK(type != blink::WebPlatformEventTypeBattery);

  if (platform_event_observers_.Lookup(type))
    platform_event_observers_.Remove(type);
  platform_event_observers_.AddWithID(observer.release(), type);
}

void RendererBlinkPlatformImpl::startListening(
    blink::WebPlatformEventType type,
    blink::WebPlatformEventListener* listener) {
  if (type == blink::WebPlatformEventTypeBattery) {
    if (RenderThreadImpl::current() &&
        RenderThreadImpl::current()->layout_test_mode()) {
      g_test_battery_status_listener =
          static_cast<blink::WebBatteryStatusListener*>(listener);
    } else {
      battery_status_dispatcher_.reset(new BatteryStatusDispatcher(
          static_cast<blink::WebBatteryStatusListener*>(listener)));
    }
    return;
  }

  PlatformEventObserverBase* observer = platform_event_observers_.Lookup(type);
  if (!observer) {
    observer = CreatePlatformEventObserverFromType(type);
    if (!observer)
      return;
    platform_event_observers_.AddWithID(observer, static_cast<int32_t>(type));
  }
  observer->Start(listener);

  // Device events (motion, orientation and light) expect to get an event fired
  // as soon as a listener is registered if a fake data was passed before.
  // TODO(mlamouri,timvolodine): make those send mock values directly instead of
  // using this broken pattern.
  if (RenderThreadImpl::current() &&
      RenderThreadImpl::current()->layout_test_mode() &&
      (type == blink::WebPlatformEventTypeDeviceMotion ||
       type == blink::WebPlatformEventTypeDeviceOrientation ||
       type == blink::WebPlatformEventTypeDeviceOrientationAbsolute ||
       type == blink::WebPlatformEventTypeDeviceLight)) {
    SendFakeDeviceEventDataForTesting(type);
  }
}

void RendererBlinkPlatformImpl::SendFakeDeviceEventDataForTesting(
    blink::WebPlatformEventType type) {
  PlatformEventObserverBase* observer = platform_event_observers_.Lookup(type);
  CHECK(observer);

  void* data = 0;

  switch (type) {
  case blink::WebPlatformEventTypeDeviceMotion:
    if (!(g_test_device_motion_data == 0))
      data = &g_test_device_motion_data.Get();
    break;
  case blink::WebPlatformEventTypeDeviceOrientation:
  case blink::WebPlatformEventTypeDeviceOrientationAbsolute:
    if (!(g_test_device_orientation_data == 0))
      data = &g_test_device_orientation_data.Get();
    break;
  case blink::WebPlatformEventTypeDeviceLight:
    if (g_test_device_light_data >= 0)
      data = &g_test_device_light_data;
    break;
  default:
    NOTREACHED();
    break;
  }

  if (!data)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&PlatformEventObserverBase::SendFakeDataForTesting,
                            base::Unretained(observer), data));
}

void RendererBlinkPlatformImpl::stopListening(
    blink::WebPlatformEventType type) {
  if (type == blink::WebPlatformEventTypeBattery) {
    g_test_battery_status_listener = nullptr;
    battery_status_dispatcher_.reset();
    return;
  }

  PlatformEventObserverBase* observer = platform_event_observers_.Lookup(type);
  if (!observer)
    return;
  observer->Stop();
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::queryStorageUsageAndQuota(
    const blink::WebURL& storage_partition,
    blink::WebStorageQuotaType type,
    blink::WebStorageQuotaCallbacks callbacks) {
  if (!thread_safe_sender_.get() || !quota_message_filter_.get())
    return;
  QuotaDispatcher::ThreadSpecificInstance(thread_safe_sender_.get(),
                                          quota_message_filter_.get())
      ->QueryStorageUsageAndQuota(
          storage_partition,
          static_cast<storage::StorageType>(type),
          QuotaDispatcher::CreateWebStorageQuotaCallbacksWrapper(callbacks));
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::MockBatteryStatusChangedForTesting(
    const blink::WebBatteryStatus& status) {
  if (!g_test_battery_status_listener)
    return;
  g_test_battery_status_listener->updateBatteryStatus(status);
}

}  // namespace content
