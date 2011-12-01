// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webkitplatformsupport_impl.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/platform_file.h"
#include "base/shared_memory.h"
#include "base/utf_string_conversions.h"
#include "content/common/database_util.h"
#include "content/common/file_system/webfilesystem_impl.h"
#include "content/common/file_utilities_messages.h"
#include "content/common/mime_registry_messages.h"
#include "content/common/npobject_util.h"
#include "content/common/view_messages.h"
#include "content/common/webblobregistry_impl.h"
#include "content/common/webmessageportchannel_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/gamepad_shared_memory_reader.h"
#include "content/renderer/gpu/webgraphicscontext3d_command_buffer_impl.h"
#include "content/renderer/media/audio_device.h"
#include "content/renderer/media/audio_hardware.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_clipboard_client.h"
#include "content/renderer/renderer_webaudiodevice_impl.h"
#include "content/renderer/renderer_webidbfactory_impl.h"
#include "content/renderer/renderer_webstoragenamespace_impl.h"
#include "content/renderer/websharedworkerrepository_impl.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_sync_message_filter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBlobRegistry.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGamepads.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKey.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKeyPath.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSerializedScriptValue.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageEventDispatcher.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webclipboard_impl.h"
#include "webkit/glue/webfileutilities_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_impl.h"

#if defined(OS_WIN)
#include "content/common/child_process_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/win/WebSandboxSupport.h"
#endif

#if defined(OS_MACOSX)
#include "content/common/mac/font_descriptor.h"
#include "content/common/mac/font_loader.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/mac/WebSandboxSupport.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <string>
#include <map>

#include "base/synchronization/lock.h"
#include "content/common/child_process_sandbox_support_impl_linux.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/linux/WebFontFamily.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/linux/WebSandboxSupport.h"
#endif

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

using WebKit::WebAudioDevice;
using WebKit::WebBlobRegistry;
using WebKit::WebFileSystem;
using WebKit::WebFrame;
using WebKit::WebGamepads;
using WebKit::WebIDBFactory;
using WebKit::WebIDBKey;
using WebKit::WebIDBKeyPath;
using WebKit::WebKitPlatformSupport;
using WebKit::WebSerializedScriptValue;
using WebKit::WebStorageArea;
using WebKit::WebStorageEventDispatcher;
using WebKit::WebStorageNamespace;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebVector;

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
  virtual void revealFolderInOS(const WebKit::WebString& path);
  virtual bool getFileSize(const WebKit::WebString& path, long long& result);
  virtual bool getFileModificationTime(const WebKit::WebString& path,
                                       double& result);
  virtual base::PlatformFile openFile(const WebKit::WebString& path,
                                      int mode);
};

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

//------------------------------------------------------------------------------

RendererWebKitPlatformSupportImpl::RendererWebKitPlatformSupportImpl()
    : clipboard_client_(new RendererClipboardClient),
      clipboard_(new webkit_glue::WebClipboardImpl(clipboard_client_.get())),
      mime_registry_(new RendererWebKitPlatformSupportImpl::MimeRegistry),
      sandbox_support_(new RendererWebKitPlatformSupportImpl::SandboxSupport),
      sudden_termination_disables_(0),
      shared_worker_repository_(new WebSharedWorkerRepositoryImpl) {
}

RendererWebKitPlatformSupportImpl::~RendererWebKitPlatformSupportImpl() {
}

//------------------------------------------------------------------------------

WebKit::WebClipboard* RendererWebKitPlatformSupportImpl::clipboard() {
  return clipboard_.get();
}

WebKit::WebMimeRegistry* RendererWebKitPlatformSupportImpl::mimeRegistry() {
  return mime_registry_.get();
}

WebKit::WebFileUtilities*
RendererWebKitPlatformSupportImpl::fileUtilities() {
  if (!file_utilities_.get()) {
    file_utilities_.reset(new FileUtilities);
    file_utilities_->set_sandbox_enabled(sandboxEnabled());
  }
  return file_utilities_.get();
}

WebKit::WebSandboxSupport* RendererWebKitPlatformSupportImpl::sandboxSupport() {
  return sandbox_support_.get();
}

WebKit::WebCookieJar* RendererWebKitPlatformSupportImpl::cookieJar() {
  NOTREACHED() << "Use WebFrameClient::cookieJar() instead!";
  return NULL;
}

bool RendererWebKitPlatformSupportImpl::sandboxEnabled() {
  // As explained in WebKitPlatformSupport.h, this function is used to decide
  // whether to allow file system operations to come out of WebKit or not.
  // Even if the sandbox is disabled, there's no reason why the code should
  // act any differently...unless we're in single process mode.  In which
  // case, we have no other choice.  WebKitPlatformSupport.h discourages using
  // this switch unless absolutely necessary, so hopefully we won't end up
  // with too many code paths being different in single-process mode.
  return !CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess);
}

bool RendererWebKitPlatformSupportImpl::SendSyncMessageFromAnyThread(
    IPC::SyncMessage* msg) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  if (render_thread)
    return render_thread->Send(msg);

  scoped_refptr<IPC::SyncMessageFilter> sync_msg_filter(
      ChildThread::current()->sync_message_filter());
  return sync_msg_filter->Send(msg);
}

unsigned long long RendererWebKitPlatformSupportImpl::visitedLinkHash(
    const char* canonical_url,
    size_t length) {
  return content::GetContentClient()->renderer()->VisitedLinkHash(
      canonical_url, length);
}

bool RendererWebKitPlatformSupportImpl::isLinkVisited(
    unsigned long long link_hash) {
  return content::GetContentClient()->renderer()->IsLinkVisited(link_hash);
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
  content::GetContentClient()->renderer()->PrefetchHostName(
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
  RenderThreadImpl::current()->Send(
      new ViewHostMsg_DidGenerateCacheableMetadata(url, response_time, copy));
}

WebString RendererWebKitPlatformSupportImpl::defaultLocale() {
  return ASCIIToUTF16(RenderThreadImpl::Get()->GetLocale());
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

  RenderThreadImpl* thread = RenderThreadImpl::current();
  if (thread)  // NULL in unittests.
    thread->Send(new ViewHostMsg_SuddenTerminationChanged(enabled));
}

WebStorageNamespace*
RendererWebKitPlatformSupportImpl::createLocalStorageNamespace(
    const WebString& path, unsigned quota) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return WebStorageNamespace::createLocalStorageNamespace(path, quota);
  return new RendererWebStorageNamespaceImpl(DOM_STORAGE_LOCAL);
}

void RendererWebKitPlatformSupportImpl::dispatchStorageEvent(
    const WebString& key, const WebString& old_value,
    const WebString& new_value, const WebString& origin,
    const WebKit::WebURL& url, bool is_local_storage) {
  DCHECK(CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess));
  // Inefficient, but only used in single process mode.
  scoped_ptr<WebStorageEventDispatcher> event_dispatcher(
      WebStorageEventDispatcher::create());
  event_dispatcher->dispatchStorageEvent(key, old_value, new_value, origin,
                                         url, is_local_storage);
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

void RendererWebKitPlatformSupportImpl::createIDBKeysFromSerializedValuesAndKeyPath(
    const WebVector<WebSerializedScriptValue>& values,
    const WebString& keyPath,
    WebVector<WebIDBKey>& keys_out) {
  DCHECK(CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess));
  WebVector<WebIDBKey> keys(values.size());
  for (size_t i = 0; i < values.size(); ++i) {
    keys[i] = WebIDBKey::createFromValueAndKeyPath(
        values[i], WebIDBKeyPath::create(keyPath));
  }
  keys_out.swap(keys);
}

WebSerializedScriptValue
RendererWebKitPlatformSupportImpl::injectIDBKeyIntoSerializedValue(
    const WebIDBKey& key,
    const WebSerializedScriptValue& value,
    const WebString& keyPath) {
  DCHECK(CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess));
  return WebIDBKey::injectIDBKeyIntoSerializedValue(
      key, value, WebIDBKeyPath::create(keyPath));
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
  RenderThreadImpl::current()->Send(
      new MimeRegistryMsg_GetMimeTypeFromExtension(
          webkit_glue::WebStringToFilePathString(file_extension), &mime_type));
  return ASCIIToUTF16(mime_type);

}

WebString RendererWebKitPlatformSupportImpl::MimeRegistry::mimeTypeFromFile(
    const WebString& file_path) {
  if (IsPluginProcess())
    return SimpleWebMimeRegistryImpl::mimeTypeFromFile(file_path);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  std::string mime_type;
  RenderThreadImpl::current()->Send(new MimeRegistryMsg_GetMimeTypeFromFile(
      FilePath(webkit_glue::WebStringToFilePathString(file_path)),
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
  FilePath::StringType file_extension;
  RenderThreadImpl::current()->Send(
      new MimeRegistryMsg_GetPreferredExtensionForMimeType(
          UTF16ToASCII(mime_type), &file_extension));
  return webkit_glue::FilePathStringToWebString(file_extension);
}

//------------------------------------------------------------------------------

bool RendererWebKitPlatformSupportImpl::FileUtilities::getFileSize(
    const WebString& path, long long& result) {
  if (SendSyncMessageFromAnyThread(new FileUtilitiesMsg_GetFileSize(
          webkit_glue::WebStringToFilePath(path),
          reinterpret_cast<int64*>(&result)))) {
    return result >= 0;
  }

  result = -1;
  return false;
}

void RendererWebKitPlatformSupportImpl::FileUtilities::revealFolderInOS(
    const WebString& path) {
  FilePath file_path(webkit_glue::WebStringToFilePath(path));
  file_util::AbsolutePath(&file_path);
  RenderThreadImpl::current()->Send(
      new ViewHostMsg_RevealFolderInOS(file_path));
}

bool RendererWebKitPlatformSupportImpl::FileUtilities::getFileModificationTime(
    const WebString& path,
    double& result) {
  base::Time time;
  if (SendSyncMessageFromAnyThread(new FileUtilitiesMsg_GetFileModificationTime(
          webkit_glue::WebStringToFilePath(path), &time))) {
    result = time.ToDoubleT();
    return !time.is_null();
  }

  result = 0;
  return false;
}

base::PlatformFile RendererWebKitPlatformSupportImpl::FileUtilities::openFile(
    const WebString& path,
    int mode) {
  IPC::PlatformFileForTransit handle = IPC::InvalidPlatformFileForTransit();
  SendSyncMessageFromAnyThread(new FileUtilitiesMsg_OpenFile(
      webkit_glue::WebStringToFilePath(path), mode, &handle));
  return IPC::PlatformFileForTransitToPlatformFile(handle);
}

//------------------------------------------------------------------------------

#if defined(OS_WIN)

bool RendererWebKitPlatformSupportImpl::SandboxSupport::ensureFontLoaded(
    HFONT font) {
  LOGFONT logfont;
  GetObject(font, sizeof(LOGFONT), &logfont);
  RenderThreadImpl::current()->PreCacheFont(logfont);
  return true;
}

#elif defined(OS_MACOSX)

bool RendererWebKitPlatformSupportImpl::SandboxSupport::loadFont(
    NSFont* src_font, CGFontRef* out, uint32* font_id) {
  uint32 font_data_size;
  FontDescriptor src_font_descriptor(src_font);
  base::SharedMemoryHandle font_data;
  if (!RenderThreadImpl::current()->Send(new ViewHostMsg_LoadFont(
        src_font_descriptor, &font_data_size, &font_data, font_id))) {
    *out = NULL;
    *font_id = 0;
    return false;
  }

  if (font_data_size == 0 || font_data == base::SharedMemory::NULLHandle() ||
      *font_id == 0) {
    NOTREACHED() << "Bad response from ViewHostMsg_LoadFont() for " <<
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

  content::GetFontFamilyForCharacters(
      characters,
      num_characters,
      preferred_locale,
      family);
  unicode_font_families_.insert(make_pair(key, *family));
}

void
RendererWebKitPlatformSupportImpl::SandboxSupport::getRenderStyleForStrike(
    const char* family, int sizeAndStyle, WebKit::WebFontRenderStyle* out) {
  content::GetRenderStyleForStrike(family, sizeAndStyle, out);
}

#endif

//------------------------------------------------------------------------------

WebKitPlatformSupport::FileHandle
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

WebKit::WebGraphicsContext3D*
RendererWebKitPlatformSupportImpl::createGraphicsContext3D() {
  // The WebGraphicsContext3DInProcessImpl code path is used for
  // layout tests (though not through this code) as well as for
  // debugging and bringing up new ports.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kInProcessWebGL)) {
    return new webkit::gpu::WebGraphicsContext3DInProcessImpl(
        gfx::kNullPluginWindow, NULL);
  } else {
#if defined(ENABLE_GPU)
    return new WebGraphicsContext3DCommandBufferImpl();
#else
    return NULL;
#endif
  }
}

double RendererWebKitPlatformSupportImpl::audioHardwareSampleRate() {
  return audio_hardware::GetOutputSampleRate();
}

size_t RendererWebKitPlatformSupportImpl::audioHardwareBufferSize() {
  return audio_hardware::GetOutputBufferSize();
}

WebAudioDevice*
RendererWebKitPlatformSupportImpl::createAudioDevice(
    size_t buffer_size,
    unsigned channels,
    double sample_rate,
    WebAudioDevice::RenderCallback* callback) {
  return new RendererWebAudioDeviceImpl(buffer_size,
                                        channels,
                                        sample_rate,
                                        callback);
}

//------------------------------------------------------------------------------

WebKit::WebString
RendererWebKitPlatformSupportImpl::signedPublicKeyAndChallengeString(
    unsigned key_size_index,
    const WebKit::WebString& challenge,
    const WebKit::WebURL& url) {
  std::string signed_public_key;
  RenderThreadImpl::current()->Send(new ViewHostMsg_Keygen(
      static_cast<uint32>(key_size_index),
      challenge.utf8(),
      GURL(url),
      &signed_public_key));
  return WebString::fromUTF8(signed_public_key);
}

//------------------------------------------------------------------------------

WebBlobRegistry* RendererWebKitPlatformSupportImpl::blobRegistry() {
  // ChildThread::current can be NULL when running some tests.
  if (!blob_registry_.get() && ChildThread::current()) {
    blob_registry_.reset(new WebBlobRegistryImpl(ChildThread::current()));
  }
  return blob_registry_.get();
}

//------------------------------------------------------------------------------

void RendererWebKitPlatformSupportImpl::sampleGamepads(WebGamepads& gamepads) {
  if (!gamepad_shared_memory_reader_.get())
    gamepad_shared_memory_reader_.reset(new content::GamepadSharedMemoryReader);
  gamepad_shared_memory_reader_->SampleGamepads(gamepads);
}

WebKit::WebString RendererWebKitPlatformSupportImpl::userAgent(
    const WebKit::WebURL& url) {
 return WebKitPlatformSupportImpl::userAgent(url);
}

void RendererWebKitPlatformSupportImpl::GetPlugins(
    bool refresh, std::vector<webkit::WebPluginInfo>* plugins) {
  if (!RenderThreadImpl::current()->plugin_refresh_allowed())
    refresh = false;
  RenderThreadImpl::current()->Send(
      new ViewHostMsg_GetPlugins(refresh, plugins));
}
