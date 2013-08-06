// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_WEBKITPLATFORMSUPPORT_IMPL_H_
#define CONTENT_RENDERER_RENDERER_WEBKITPLATFORMSUPPORT_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "content/child/webkitplatformsupport_impl.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebSharedWorkerRepository.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebIDBFactory.h"
#include "webkit/renderer/compositor_bindings/web_compositor_support_impl.h"

namespace base {
class MessageLoopProxy;
}

namespace cc {
class ContextProvider;
}

namespace IPC {
class SyncMessageFilter;
}

namespace WebKit {
class WebDeviceMotionData;
class WebGraphicsContext3DProvider;
}

namespace content {
class DeviceMotionEventPump;
class DeviceOrientationEventPump;
class QuotaMessageFilter;
class RendererClipboardClient;
class ThreadSafeSender;
class WebClipboardImpl;
class WebCryptoImpl;
class WebFileSystemImpl;
class WebSharedWorkerRepositoryImpl;

class CONTENT_EXPORT RendererWebKitPlatformSupportImpl
    : public WebKitPlatformSupportImpl {
 public:
  RendererWebKitPlatformSupportImpl();
  virtual ~RendererWebKitPlatformSupportImpl();

  void set_plugin_refresh_allowed(bool plugin_refresh_allowed) {
    plugin_refresh_allowed_ = plugin_refresh_allowed;
  }
  // Platform methods:
  virtual WebKit::WebClipboard* clipboard();
  virtual WebKit::WebMimeRegistry* mimeRegistry();
  virtual WebKit::WebFileUtilities* fileUtilities();
  virtual WebKit::WebSandboxSupport* sandboxSupport();
  virtual WebKit::WebCookieJar* cookieJar();
  virtual WebKit::WebThemeEngine* themeEngine();
  virtual WebKit::WebSpeechSynthesizer* createSpeechSynthesizer(
      WebKit::WebSpeechSynthesizerClient* client);
  virtual bool sandboxEnabled();
  virtual unsigned long long visitedLinkHash(
      const char* canonicalURL, size_t length);
  virtual bool isLinkVisited(unsigned long long linkHash);
  virtual WebKit::WebMessagePortChannel* createMessagePortChannel();
  virtual WebKit::WebPrescientNetworking* prescientNetworking();
  virtual void cacheMetadata(
      const WebKit::WebURL&, double, const char*, size_t);
  virtual WebKit::WebString defaultLocale();
  virtual void suddenTerminationChanged(bool enabled);
  virtual WebKit::WebStorageNamespace* createLocalStorageNamespace();
  virtual WebKit::Platform::FileHandle databaseOpenFile(
      const WebKit::WebString& vfs_file_name, int desired_flags);
  virtual int databaseDeleteFile(const WebKit::WebString& vfs_file_name,
                                 bool sync_dir);
  virtual long databaseGetFileAttributes(
      const WebKit::WebString& vfs_file_name);
  virtual long long databaseGetFileSize(
      const WebKit::WebString& vfs_file_name);
  virtual long long databaseGetSpaceAvailableForOrigin(
      const WebKit::WebString& origin_identifier);
  virtual WebKit::WebString signedPublicKeyAndChallengeString(
      unsigned key_size_index,
      const WebKit::WebString& challenge,
      const WebKit::WebURL& url);
  virtual void getPluginList(bool refresh,
                             WebKit::WebPluginListBuilder* builder);
  virtual void screenColorProfile(WebKit::WebVector<char>* to_profile);
  virtual WebKit::WebIDBFactory* idbFactory();
  virtual WebKit::WebFileSystem* fileSystem();
  virtual WebKit::WebSharedWorkerRepository* sharedWorkerRepository();
  virtual bool canAccelerate2dCanvas();
  virtual bool isThreadedCompositingEnabled();
  virtual double audioHardwareSampleRate();
  virtual size_t audioHardwareBufferSize();
  virtual unsigned audioHardwareOutputChannels();

  // TODO(crogers): remove deprecated API as soon as WebKit calls new API.
  virtual WebKit::WebAudioDevice* createAudioDevice(
      size_t buffer_size, unsigned channels, double sample_rate,
      WebKit::WebAudioDevice::RenderCallback* callback);
  // TODO(crogers): remove deprecated API as soon as WebKit calls new API.
  virtual WebKit::WebAudioDevice* createAudioDevice(
      size_t buffer_size, unsigned input_channels, unsigned channels,
      double sample_rate, WebKit::WebAudioDevice::RenderCallback* callback);

  virtual WebKit::WebAudioDevice* createAudioDevice(
      size_t buffer_size, unsigned input_channels, unsigned channels,
      double sample_rate, WebKit::WebAudioDevice::RenderCallback* callback,
      const WebKit::WebString& input_device_id);

  virtual bool loadAudioResource(
      WebKit::WebAudioBus* destination_bus, const char* audio_file_data,
      size_t data_size, double sample_rate);

  virtual WebKit::WebContentDecryptionModule* createContentDecryptionModule(
      const WebKit::WebString& key_system);
  virtual WebKit::WebMIDIAccessor*
      createMIDIAccessor(WebKit::WebMIDIAccessorClient* client);

  virtual WebKit::WebBlobRegistry* blobRegistry();
  virtual void sampleGamepads(WebKit::WebGamepads&);
  virtual WebKit::WebString userAgent(const WebKit::WebURL& url);
  virtual WebKit::WebRTCPeerConnectionHandler* createRTCPeerConnectionHandler(
      WebKit::WebRTCPeerConnectionHandlerClient* client);
  virtual WebKit::WebMediaStreamCenter* createMediaStreamCenter(
      WebKit::WebMediaStreamCenterClient* client);
  virtual bool processMemorySizesInBytes(
      size_t* private_bytes, size_t* shared_bytes);
  virtual WebKit::WebGraphicsContext3D* createOffscreenGraphicsContext3D(
      const WebKit::WebGraphicsContext3D::Attributes& attributes);
  virtual WebKit::WebGraphicsContext3DProvider*
      createSharedOffscreenGraphicsContext3DProvider();
  virtual WebKit::WebCompositorSupport* compositorSupport();
  virtual WebKit::WebString convertIDNToUnicode(
      const WebKit::WebString& host, const WebKit::WebString& languages);
  virtual void setDeviceMotionListener(
      WebKit::WebDeviceMotionListener* listener) OVERRIDE;
  virtual void setDeviceOrientationListener(
      WebKit::WebDeviceOrientationListener* listener) OVERRIDE;
  virtual WebKit::WebCrypto* crypto() OVERRIDE;
  virtual void queryStorageUsageAndQuota(
      const WebKit::WebURL& storage_partition,
      WebKit::WebStorageQuotaType,
      WebKit::WebStorageQuotaCallbacks*) OVERRIDE;

#if defined(OS_ANDROID)
  virtual void vibrate(unsigned int milliseconds);
  virtual void cancelVibration();
#endif  // defined(OS_ANDROID)

  // Disables the WebSandboxSupport implementation for testing.
  // Tests that do not set up a full sandbox environment should call
  // SetSandboxEnabledForTesting(false) _before_ creating any instances
  // of this class, to ensure that we don't attempt to use sandbox-related
  // file descriptors or other resources.
  //
  // Returns the previous |enable| value.
  static bool SetSandboxEnabledForTesting(bool enable);

  // Set WebGamepads to return when sampleGamepads() is invoked.
  static void SetMockGamepadsForTesting(const WebKit::WebGamepads& pads);
  // Set WebDeviceMotionData to return when setDeviceMotionListener is invoked.
  static void SetMockDeviceMotionDataForTesting(
      const WebKit::WebDeviceMotionData& data);

 private:
  bool CheckPreparsedJsCachingEnabled() const;

  scoped_ptr<RendererClipboardClient> clipboard_client_;
  scoped_ptr<WebClipboardImpl> clipboard_;

  class FileUtilities;
  scoped_ptr<FileUtilities> file_utilities_;

  class MimeRegistry;
  scoped_ptr<MimeRegistry> mime_registry_;

  class SandboxSupport;
  scoped_ptr<SandboxSupport> sandbox_support_;

  // This counter keeps track of the number of times sudden termination is
  // enabled or disabled. It starts at 0 (enabled) and for every disable
  // increments by 1, for every enable decrements by 1. When it reaches 0,
  // we tell the browser to enable fast termination.
  int sudden_termination_disables_;

  // If true, then a GetPlugins call is allowed to rescan the disk.
  bool plugin_refresh_allowed_;

  // Implementation of the WebSharedWorkerRepository APIs (provides an interface
  // to WorkerService on the browser thread.
  scoped_ptr<WebSharedWorkerRepositoryImpl> shared_worker_repository_;

  scoped_ptr<WebKit::WebIDBFactory> web_idb_factory_;

  scoped_ptr<WebFileSystemImpl> web_file_system_;

  scoped_ptr<WebKit::WebBlobRegistry> blob_registry_;

  scoped_ptr<DeviceMotionEventPump> device_motion_event_pump_;
  scoped_ptr<DeviceOrientationEventPump> device_orientation_event_pump_;

  scoped_refptr<base::MessageLoopProxy> child_thread_loop_;
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<QuotaMessageFilter> quota_message_filter_;

  scoped_refptr<cc::ContextProvider> shared_offscreen_context_;

  webkit::WebCompositorSupportImpl compositor_support_;

  scoped_ptr<WebCryptoImpl> web_crypto_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_WEBKITPLATFORMSUPPORT_IMPL_H_
