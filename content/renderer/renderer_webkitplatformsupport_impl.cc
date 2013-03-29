// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webkitplatformsupport_impl.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/platform_file.h"
#include "base/shared_memory.h"
#include "base/utf_string_conversions.h"
#include "content/common/database_util.h"
#include "content/common/file_utilities_messages.h"
#include "content/common/fileapi/webblobregistry_impl.h"
#include "content/common/fileapi/webfilesystem_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/indexed_db/proxy_webidbfactory_impl.h"
#include "content/common/mime_registry_messages.h"
#include "content/common/npobject_util.h"
#include "content/common/thread_safe_sender.h"
#include "content/common/view_messages.h"
#include "content/common/webmessageportchannel_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_info.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/dom_storage/webstoragenamespace_impl.h"
#include "content/renderer/gamepad_shared_memory_reader.h"
#include "content/renderer/hyphenator/hyphenator.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/renderer_webaudiodevice_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_clipboard_client.h"
#include "content/renderer/websharedworkerrepository_impl.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/audio/audio_output_device.h"
#include "media/base/audio_hardware_config.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebBlobRegistry.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFileInfo.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGamepads.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebHyphenator.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamCenter.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamCenterClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webclipboard_impl.h"
#include "webkit/glue/webfileutilities_impl.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_WIN)
#include "content/common/child_process_messages.h"
#include "third_party/WebKit/Source/Platform/chromium/public/win/WebSandboxSupport.h"
#endif

#if defined(OS_MACOSX)
#include "content/common/mac/font_descriptor.h"
#include "content/common/mac/font_loader.h"
#include "third_party/WebKit/Source/Platform/chromium/public/mac/WebSandboxSupport.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
#include <map>
#include <string>

#include "base/synchronization/lock.h"
#include "content/common/child_process_sandbox_support_impl_linux.h"
#include "third_party/WebKit/Source/Platform/chromium/public/linux/WebFontFamily.h"
#include "third_party/WebKit/Source/Platform/chromium/public/linux/WebSandboxSupport.h"
#endif

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

using WebKit::WebAudioDevice;
using WebKit::WebBlobRegistry;
using WebKit::WebFileInfo;
using WebKit::WebFileSystem;
using WebKit::WebFrame;
using WebKit::WebGamepads;
using WebKit::WebIDBFactory;
using WebKit::Platform;
using WebKit::WebMediaStreamCenter;
using WebKit::WebMediaStreamCenterClient;
using WebKit::WebRTCPeerConnectionHandler;
using WebKit::WebRTCPeerConnectionHandlerClient;
using WebKit::WebStorageNamespace;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebVector;

namespace content {

static bool g_sandbox_enabled = true;
base::LazyInstance<WebGamepads>::Leaky g_test_gamepads =
    LAZY_INSTANCE_INITIALIZER;

//------------------------------------------------------------------------------

class RendererWebKitPlatformSupportImpl::MimeRegistry
    : public webkit_glue::SimpleWebMimeRegistryImpl {
 public:
  virtual WebKit::WebString mimeTypeForExtension(const WebKit::WebString&);
  virtual WebKit::WebString mimeTypeFromFile(const WebKit::WebString&);
  virtual WebKit::WebString preferredExtensionForMIMEType(
      const WebKit::WebString&);
};

class RendererWebKitPlatformSupportImpl::FileUtilities
    : public webkit_glue::WebFileUtilitiesImpl {
 public:
  explicit FileUtilities(ThreadSafeSender* sender)
      : thread_safe_sender_(sender) {}
  virtual bool getFileInfo(const WebString& path, WebFileInfo& result);
  virtual base::PlatformFile openFile(const WebKit::WebString& path,
                                      int mode);
 private:
  bool SendSyncMessageFromAnyThread(IPC::SyncMessage* msg) const;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
};

class RendererWebKitPlatformSupportImpl::Hyphenator
    : public WebKit::WebHyphenator {
 public:
  Hyphenator();
  virtual ~Hyphenator();

  virtual bool canHyphenate(const WebKit::WebString& locale) OVERRIDE;
  virtual size_t computeLastHyphenLocation(
      const char16* characters,
      size_t length,
      size_t before_index,
      const WebKit::WebString& locale) OVERRIDE;
 private:
  scoped_ptr<content::Hyphenator> hyphenator_;

  DISALLOW_COPY_AND_ASSIGN(Hyphenator);
};

#if defined(OS_ANDROID)
// WebKit doesn't use WebSandboxSupport on android so we don't need to
// implement anything here.
class RendererWebKitPlatformSupportImpl::SandboxSupport {
};
#else
class RendererWebKitPlatformSupportImpl::SandboxSupport
    : public WebKit::WebSandboxSupport {
 public:
  virtual ~SandboxSupport() {}

#if defined(OS_WIN)
  virtual bool ensureFontLoaded(HFONT);
#elif defined(OS_MACOSX)
  virtual bool loadFont(
      NSFont* src_font,
      CGFontRef* container,
      uint32* font_id);
#elif defined(OS_POSIX)
  virtual void getFontFamilyForCharacters(
      const WebKit::WebUChar* characters,
      size_t numCharacters,
      const char* preferred_locale,
      WebKit::WebFontFamily* family);
  virtual void getRenderStyleForStrike(
      const char* family, int sizeAndStyle, WebKit::WebFontRenderStyle* out);

 private:
  // WebKit likes to ask us for the correct font family to use for a set of
  // unicode code points. It needs this information frequently so we cache it
  // here. The key in this map is an array of 16-bit UTF16 values from WebKit.
  // The value is a string containing the correct font family.
  base::Lock unicode_font_families_mutex_;
  std::map<string16, WebKit::WebFontFamily> unicode_font_families_;
#endif
};
#endif  // defined(OS_ANDROID)

//------------------------------------------------------------------------------

RendererWebKitPlatformSupportImpl::RendererWebKitPlatformSupportImpl()
    : clipboard_client_(new RendererClipboardClient),
      clipboard_(new webkit_glue::WebClipboardImpl(clipboard_client_.get())),
      mime_registry_(new RendererWebKitPlatformSupportImpl::MimeRegistry),
      hyphenator_(new RendererWebKitPlatformSupportImpl::Hyphenator),
      sudden_termination_disables_(0),
      plugin_refresh_allowed_(true),
      shared_worker_repository_(new WebSharedWorkerRepositoryImpl) {
  if (g_sandbox_enabled && sandboxEnabled()) {
    sandbox_support_.reset(
        new RendererWebKitPlatformSupportImpl::SandboxSupport);
  } else {
    DVLOG(1) << "Disabling sandbox support for testing.";
  }

  // ChildThread may not exist in some tests.
  if (ChildThread::current())
    thread_safe_sender_ = ChildThread::current()->thread_safe_sender();
}

RendererWebKitPlatformSupportImpl::~RendererWebKitPlatformSupportImpl() {
}

//------------------------------------------------------------------------------

WebKit::WebClipboard* RendererWebKitPlatformSupportImpl::clipboard() {
  WebKit::WebClipboard* clipboard =
      GetContentClient()->renderer()->OverrideWebClipboard();
  if (clipboard)
    return clipboard;
  return clipboard_.get();
}

WebKit::WebMimeRegistry* RendererWebKitPlatformSupportImpl::mimeRegistry() {
  WebKit::WebMimeRegistry* mime_registry =
      GetContentClient()->renderer()->OverrideWebMimeRegistry();
  if (mime_registry)
    return mime_registry;
  return mime_registry_.get();
}

WebKit::WebFileUtilities*
RendererWebKitPlatformSupportImpl::fileUtilities() {
  if (!file_utilities_.get()) {
    file_utilities_.reset(new FileUtilities(thread_safe_sender_));
    file_utilities_->set_sandbox_enabled(sandboxEnabled());
  }
  return file_utilities_.get();
}

WebKit::WebSandboxSupport* RendererWebKitPlatformSupportImpl::sandboxSupport() {
#if defined(OS_ANDROID)
  // WebKit doesn't use WebSandboxSupport on android.
  return NULL;
#else
  return sandbox_support_.get();
#endif
}

WebKit::WebCookieJar* RendererWebKitPlatformSupportImpl::cookieJar() {
  NOTREACHED() << "Use WebFrameClient::cookieJar() instead!";
  return NULL;
}

WebKit::WebThemeEngine* RendererWebKitPlatformSupportImpl::themeEngine() {
  WebKit::WebThemeEngine* theme_engine =
      GetContentClient()->renderer()->OverrideThemeEngine();
  if (theme_engine)
    return theme_engine;
  return WebKitPlatformSupportImpl::themeEngine();
}

bool RendererWebKitPlatformSupportImpl::sandboxEnabled() {
  // As explained in Platform.h, this function is used to decide
  // whether to allow file system operations to come out of WebKit or not.
  // Even if the sandbox is disabled, there's no reason why the code should
  // act any differently...unless we're in single process mode.  In which
  // case, we have no other choice.  Platform.h discourages using
  // this switch unless absolutely necessary, so hopefully we won't end up
  // with too many code paths being different in single-process mode.
  return !CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess);
}

unsigned long long RendererWebKitPlatformSupportImpl::visitedLinkHash(
    const char* canonical_url,
    size_t length) {
  return GetContentClient()->renderer()->VisitedLinkHash(canonical_url, length);
}

bool RendererWebKitPlatformSupportImpl::isLinkVisited(
    unsigned long long link_hash) {
  return GetContentClient()->renderer()->IsLinkVisited(link_hash);
}

WebKit::WebMessagePortChannel*
RendererWebKitPlatformSupportImpl::createMessagePortChannel() {
  return new WebMessagePortChannelImpl();
}

void RendererWebKitPlatformSupportImpl::prefetchHostName(
    const WebString& hostname) {
  if (hostname.isEmpty())
    return;

  std::string hostname_utf8;
  UTF16ToUTF8(hostname.data(), hostname.length(), &hostname_utf8);
  GetContentClient()->renderer()->PrefetchHostName(
      hostname_utf8.data(), hostname_utf8.length());
}

bool
RendererWebKitPlatformSupportImpl::CheckPreparsedJsCachingEnabled() const {
  static bool checked = false;
  static bool result = false;
  if (!checked) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    result = command_line.HasSwitch(switches::kEnablePreparsedJsCaching);
    checked = true;
  }
  return result;
}

void RendererWebKitPlatformSupportImpl::cacheMetadata(
    const WebKit::WebURL& url,
    double response_time,
    const char* data,
    size_t size) {
  if (!CheckPreparsedJsCachingEnabled())
    return;

  // Let the browser know we generated cacheable metadata for this resource. The
  // browser may cache it and return it on subsequent responses to speed
  // the processing of this resource.
  std::vector<char> copy(data, data + size);
  RenderThread::Get()->Send(
      new ViewHostMsg_DidGenerateCacheableMetadata(url, response_time, copy));
}

WebString RendererWebKitPlatformSupportImpl::defaultLocale() {
  return ASCIIToUTF16(RenderThread::Get()->GetLocale());
}

void RendererWebKitPlatformSupportImpl::suddenTerminationChanged(bool enabled) {
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
    thread->Send(new ViewHostMsg_SuddenTerminationChanged(enabled));
}

WebStorageNamespace*
RendererWebKitPlatformSupportImpl::createLocalStorageNamespace(
    const WebString& path, unsigned quota) {
  return new WebStorageNamespaceImpl();
}


//------------------------------------------------------------------------------

WebIDBFactory* RendererWebKitPlatformSupportImpl::idbFactory() {
  if (!web_idb_factory_.get()) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
      web_idb_factory_.reset(WebIDBFactory::create());
    else
      web_idb_factory_.reset(new RendererWebIDBFactoryImpl());
  }
  return web_idb_factory_.get();
}

//------------------------------------------------------------------------------

WebFileSystem* RendererWebKitPlatformSupportImpl::fileSystem() {
  if (!web_file_system_.get())
    web_file_system_.reset(new WebFileSystemImpl());
  return web_file_system_.get();
}

//------------------------------------------------------------------------------

WebString
RendererWebKitPlatformSupportImpl::MimeRegistry::mimeTypeForExtension(
    const WebString& file_extension) {
  if (IsPluginProcess())
    return SimpleWebMimeRegistryImpl::mimeTypeForExtension(file_extension);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  std::string mime_type;
  RenderThread::Get()->Send(
      new MimeRegistryMsg_GetMimeTypeFromExtension(
          webkit_base::WebStringToFilePathString(file_extension), &mime_type));
  return ASCIIToUTF16(mime_type);
}

WebString RendererWebKitPlatformSupportImpl::MimeRegistry::mimeTypeFromFile(
    const WebString& file_path) {
  if (IsPluginProcess())
    return SimpleWebMimeRegistryImpl::mimeTypeFromFile(file_path);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  std::string mime_type;
  RenderThread::Get()->Send(new MimeRegistryMsg_GetMimeTypeFromFile(
      base::FilePath(webkit_base::WebStringToFilePathString(file_path)),
      &mime_type));
  return ASCIIToUTF16(mime_type);
}

WebString
RendererWebKitPlatformSupportImpl::MimeRegistry::preferredExtensionForMIMEType(
    const WebString& mime_type) {
  if (IsPluginProcess())
    return SimpleWebMimeRegistryImpl::preferredExtensionForMIMEType(mime_type);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  base::FilePath::StringType file_extension;
  RenderThread::Get()->Send(
      new MimeRegistryMsg_GetPreferredExtensionForMimeType(
          UTF16ToASCII(mime_type), &file_extension));
  return webkit_base::FilePathStringToWebString(file_extension);
}

//------------------------------------------------------------------------------

bool RendererWebKitPlatformSupportImpl::FileUtilities::getFileInfo(
    const WebString& path,
    WebFileInfo& web_file_info) {
  base::PlatformFileInfo file_info;
  base::PlatformFileError status;
  if (!SendSyncMessageFromAnyThread(new FileUtilitiesMsg_GetFileInfo(
           webkit_base::WebStringToFilePath(path), &file_info, &status)) ||
      status != base::PLATFORM_FILE_OK) {
    return false;
  }
  webkit_glue::PlatformFileInfoToWebFileInfo(file_info, &web_file_info);
  web_file_info.platformPath = path;
  return true;
}

base::PlatformFile RendererWebKitPlatformSupportImpl::FileUtilities::openFile(
    const WebString& path,
    int mode) {
  IPC::PlatformFileForTransit handle = IPC::InvalidPlatformFileForTransit();
  SendSyncMessageFromAnyThread(new FileUtilitiesMsg_OpenFile(
      webkit_base::WebStringToFilePath(path), mode, &handle));
  return IPC::PlatformFileForTransitToPlatformFile(handle);
}

bool RendererWebKitPlatformSupportImpl::FileUtilities::
SendSyncMessageFromAnyThread(IPC::SyncMessage* msg) const {
  base::TimeTicks begin = base::TimeTicks::Now();
  const bool success = thread_safe_sender_->Send(msg);
  base::TimeDelta delta = base::TimeTicks::Now() - begin;
  UMA_HISTOGRAM_TIMES("RendererSyncIPC.ElapsedTime", delta);
  return success;
}

//------------------------------------------------------------------------------

RendererWebKitPlatformSupportImpl::Hyphenator::Hyphenator() {}

RendererWebKitPlatformSupportImpl::Hyphenator::~Hyphenator() {}

bool RendererWebKitPlatformSupportImpl::Hyphenator::canHyphenate(
    const WebKit::WebString& locale) {
  // Return false unless WebKit asks for US English dictionaries because WebKit
  // can currently hyphenate only English words.
  if (!locale.isEmpty() && !locale.equals("en-US"))
    return false;

  // Create a hyphenator object and attach it to the render thread so it can
  // receive a dictionary file opened by a browser.
  if (!hyphenator_.get()) {
    hyphenator_.reset(new content::Hyphenator(base::kInvalidPlatformFileValue));
    if (!hyphenator_.get())
      return false;
    return hyphenator_->Attach(RenderThreadImpl::current(), locale);
  }
  return hyphenator_->CanHyphenate(locale);
}

size_t RendererWebKitPlatformSupportImpl::Hyphenator::computeLastHyphenLocation(
    const char16* characters,
    size_t length,
    size_t before_index,
    const WebKit::WebString& locale) {
  // Crash if WebKit calls this function when canHyphenate returns false.
  DCHECK(locale.isEmpty() || locale.equals("en-US"));
  DCHECK(hyphenator_.get());
  return hyphenator_->ComputeLastHyphenLocation(string16(characters, length),
                                                before_index);
}

//------------------------------------------------------------------------------

#if defined(OS_WIN)

bool RendererWebKitPlatformSupportImpl::SandboxSupport::ensureFontLoaded(
    HFONT font) {
  LOGFONT logfont;
  GetObject(font, sizeof(LOGFONT), &logfont);
  RenderThread::Get()->PreCacheFont(logfont);
  return true;
}

#elif defined(OS_MACOSX)

bool RendererWebKitPlatformSupportImpl::SandboxSupport::loadFont(
    NSFont* src_font, CGFontRef* out, uint32* font_id) {
  uint32 font_data_size;
  FontDescriptor src_font_descriptor(src_font);
  base::SharedMemoryHandle font_data;
  if (!RenderThread::Get()->Send(new ViewHostMsg_LoadFont(
        src_font_descriptor, &font_data_size, &font_data, font_id))) {
    *out = NULL;
    *font_id = 0;
    return false;
  }

  if (font_data_size == 0 || font_data == base::SharedMemory::NULLHandle() ||
      *font_id == 0) {
    LOG(ERROR) << "Bad response from ViewHostMsg_LoadFont() for " <<
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

#elif defined(OS_ANDROID)

// WebKit doesn't use WebSandboxSupport on android so we don't need to
// implement anything here. This is cleaner to support than excluding the
// whole class for android.

#elif defined(OS_POSIX)

void
RendererWebKitPlatformSupportImpl::SandboxSupport::getFontFamilyForCharacters(
    const WebKit::WebUChar* characters,
    size_t num_characters,
    const char* preferred_locale,
    WebKit::WebFontFamily* family) {
  base::AutoLock lock(unicode_font_families_mutex_);
  const string16 key(characters, num_characters);
  const std::map<string16, WebKit::WebFontFamily>::const_iterator iter =
      unicode_font_families_.find(key);
  if (iter != unicode_font_families_.end()) {
    family->name = iter->second.name;
    family->isBold = iter->second.isBold;
    family->isItalic = iter->second.isItalic;
    return;
  }

  GetFontFamilyForCharacters(
      characters,
      num_characters,
      preferred_locale,
      family);
  unicode_font_families_.insert(make_pair(key, *family));
}

void
RendererWebKitPlatformSupportImpl::SandboxSupport::getRenderStyleForStrike(
    const char* family, int sizeAndStyle, WebKit::WebFontRenderStyle* out) {
  GetRenderStyleForStrike(family, sizeAndStyle, out);
}

#endif

//------------------------------------------------------------------------------

Platform::FileHandle
RendererWebKitPlatformSupportImpl::databaseOpenFile(
    const WebString& vfs_file_name, int desired_flags) {
  return DatabaseUtil::DatabaseOpenFile(vfs_file_name, desired_flags);
}

int RendererWebKitPlatformSupportImpl::databaseDeleteFile(
    const WebString& vfs_file_name, bool sync_dir) {
  return DatabaseUtil::DatabaseDeleteFile(vfs_file_name, sync_dir);
}

long RendererWebKitPlatformSupportImpl::databaseGetFileAttributes(
    const WebString& vfs_file_name) {
  return DatabaseUtil::DatabaseGetFileAttributes(vfs_file_name);
}

long long RendererWebKitPlatformSupportImpl::databaseGetFileSize(
    const WebString& vfs_file_name) {
  return DatabaseUtil::DatabaseGetFileSize(vfs_file_name);
}

long long RendererWebKitPlatformSupportImpl::databaseGetSpaceAvailableForOrigin(
    const WebString& origin_identifier) {
  return DatabaseUtil::DatabaseGetSpaceAvailable(origin_identifier);
}

WebKit::WebSharedWorkerRepository*
RendererWebKitPlatformSupportImpl::sharedWorkerRepository() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSharedWorkers)) {
    return shared_worker_repository_.get();
  } else {
    return NULL;
  }
}

bool RendererWebKitPlatformSupportImpl::canAccelerate2dCanvas() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  GpuChannelHost* host = thread->EstablishGpuChannelSync(
      CAUSE_FOR_GPU_LAUNCH_CANVAS_2D);
  if (!host)
    return false;

  const GPUInfo& gpu_info = host->gpu_info();
  if (gpu_info.can_lose_context || gpu_info.software_rendering)
    return false;

  return true;
}

bool RendererWebKitPlatformSupportImpl::isThreadedCompositingEnabled() {
  return !!RenderThreadImpl::current()->compositor_message_loop_proxy();
}

double RendererWebKitPlatformSupportImpl::audioHardwareSampleRate() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread->GetAudioHardwareConfig()->GetOutputSampleRate();
}

size_t RendererWebKitPlatformSupportImpl::audioHardwareBufferSize() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread->GetAudioHardwareConfig()->GetOutputBufferSize();
}

unsigned RendererWebKitPlatformSupportImpl::audioHardwareOutputChannels() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread->GetAudioHardwareConfig()->GetOutputChannels();
}

// TODO(crogers): remove deprecated API as soon as WebKit calls new API.
WebAudioDevice*
RendererWebKitPlatformSupportImpl::createAudioDevice(
    size_t buffer_size,
    unsigned channels,
    double sample_rate,
    WebAudioDevice::RenderCallback* callback) {
  return createAudioDevice(
      buffer_size, 0, channels, sample_rate, callback, "default");
}

// TODO(crogers): remove deprecated API as soon as WebKit calls new API.
WebAudioDevice*
RendererWebKitPlatformSupportImpl::createAudioDevice(
    size_t buffer_size,
    unsigned input_channels,
    unsigned channels,
    double sample_rate,
    WebAudioDevice::RenderCallback* callback) {
  return createAudioDevice(
      buffer_size, input_channels, channels, sample_rate, callback, "default");
}

WebAudioDevice*
RendererWebKitPlatformSupportImpl::createAudioDevice(
    size_t buffer_size,
    unsigned input_channels,
    unsigned channels,
    double sample_rate,
    WebAudioDevice::RenderCallback* callback,
    const WebKit::WebString& input_device_id) {
  if (input_device_id != "default") {
    // Only allow audio input if we know for sure that WebKit is giving us the
    // "default" input device.
    // TODO(crogers): add support for non-default audio input devices when
    // using synchronized audio I/O in WebAudio.
    if (input_channels > 0)
      DLOG(WARNING) << "createAudioDevice(): request for audio input ignored";
    input_channels = 0;
  }

  // The |channels| does not exactly identify the channel layout of the
  // device. The switch statement below assigns a best guess to the channel
  // layout based on number of channels.
  // TODO(crogers): WebKit should give the channel layout instead of the hard
  // channel count.
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
      layout = media::CHANNEL_LAYOUT_STEREO;
  }

  media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      layout, input_channels,
      static_cast<int>(sample_rate), 16, buffer_size);

  return new RendererWebAudioDeviceImpl(params, callback);
}

//------------------------------------------------------------------------------

WebKit::WebString
RendererWebKitPlatformSupportImpl::signedPublicKeyAndChallengeString(
    unsigned key_size_index,
    const WebKit::WebString& challenge,
    const WebKit::WebURL& url) {
  std::string signed_public_key;
  RenderThread::Get()->Send(new ViewHostMsg_Keygen(
      static_cast<uint32>(key_size_index),
      challenge.utf8(),
      GURL(url),
      &signed_public_key));
  return WebString::fromUTF8(signed_public_key);
}

//------------------------------------------------------------------------------

void RendererWebKitPlatformSupportImpl::screenColorProfile(
    WebVector<char>* to_profile) {
  std::vector<char> profile;
  RenderThread::Get()->Send(
      new ViewHostMsg_GetMonitorColorProfile(&profile));
  *to_profile = profile;
}

//------------------------------------------------------------------------------

WebBlobRegistry* RendererWebKitPlatformSupportImpl::blobRegistry() {
  // thread_safe_sender_ can be NULL when running some tests.
  if (!blob_registry_.get() && thread_safe_sender_.get())
    blob_registry_.reset(new WebBlobRegistryImpl(thread_safe_sender_));
  return blob_registry_.get();
}

//------------------------------------------------------------------------------

void RendererWebKitPlatformSupportImpl::sampleGamepads(WebGamepads& gamepads) {
  if (g_test_gamepads == 0) {
    if (!gamepad_shared_memory_reader_.get())
      gamepad_shared_memory_reader_.reset(new GamepadSharedMemoryReader);
    gamepad_shared_memory_reader_->SampleGamepads(gamepads);
  } else {
    gamepads = g_test_gamepads.Get();
    return;
  }
}

WebKit::WebString RendererWebKitPlatformSupportImpl::userAgent(
    const WebKit::WebURL& url) {
  return WebKitPlatformSupportImpl::userAgent(url);
}

void RendererWebKitPlatformSupportImpl::GetPlugins(
    bool refresh, std::vector<webkit::WebPluginInfo>* plugins) {
#if defined(ENABLE_PLUGINS)
  if (!plugin_refresh_allowed_)
    refresh = false;
  RenderThread::Get()->Send(
      new ViewHostMsg_GetPlugins(refresh, plugins));
#endif
}

//------------------------------------------------------------------------------

WebRTCPeerConnectionHandler*
RendererWebKitPlatformSupportImpl::createRTCPeerConnectionHandler(
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

  MediaStreamDependencyFactory* rtc_dependency_factory =
      render_thread->GetMediaStreamDependencyFactory();
  return rtc_dependency_factory->CreateRTCPeerConnectionHandler(client);
#else
  return NULL;
#endif  // defined(ENABLE_WEBRTC)
}

//------------------------------------------------------------------------------

WebMediaStreamCenter*
RendererWebKitPlatformSupportImpl::createMediaStreamCenter(
    WebMediaStreamCenterClient* client) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread);
  if (!render_thread)
    return NULL;
  return render_thread->CreateMediaStreamCenter(client);
}

// static
bool RendererWebKitPlatformSupportImpl::SetSandboxEnabledForTesting(
    bool enable) {
  bool was_enabled = g_sandbox_enabled;
  g_sandbox_enabled = enable;
  return was_enabled;
}

// static
void RendererWebKitPlatformSupportImpl::SetMockGamepadsForTesting(
    const WebGamepads& pads) {
  g_test_gamepads.Get() = pads;
}

//------------------------------------------------------------------------------

WebKit::WebHyphenator* RendererWebKitPlatformSupportImpl::hyphenator() {
  WebKit::WebHyphenator* hyphenator =
      GetContentClient()->renderer()->OverrideWebHyphenator();
  if (hyphenator)
    return hyphenator;
  return hyphenator_.get();
}

//------------------------------------------------------------------------------

bool RendererWebKitPlatformSupportImpl::processMemorySizesInBytes(
    size_t* private_bytes, size_t* shared_bytes) {
  content::RenderThread::Get()->Send(
      new ViewHostMsg_GetProcessMemorySizes(private_bytes, shared_bytes));
  return true;
}

//------------------------------------------------------------------------------

WebKit::WebGraphicsContext3D*
RendererWebKitPlatformSupportImpl::createOffscreenGraphicsContext3D(
    const WebKit::WebGraphicsContext3D::Attributes& attributes) {
  return WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
      RenderThreadImpl::current(),
      attributes,
      GURL(attributes.topDocumentURL));
}

//------------------------------------------------------------------------------

WebKit::WebGraphicsContext3D* RendererWebKitPlatformSupportImpl::
    sharedOffscreenGraphicsContext3D() {
  if (!shared_offscreen_context_ ||
      shared_offscreen_context_->DestroyedOnMainThread()) {
    shared_offscreen_context_ =
        RenderThreadImpl::current()->OffscreenContextProviderForMainThread();
  }
  if (!shared_offscreen_context_)
    return NULL;
  return shared_offscreen_context_->Context3d();
}

//------------------------------------------------------------------------------

GrContext* RendererWebKitPlatformSupportImpl::sharedOffscreenGrContext() {
  if (!shared_offscreen_context_)
    return NULL;
  return shared_offscreen_context_->GrContext();
}


}  // namespace content
