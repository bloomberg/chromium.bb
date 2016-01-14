// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_BLINK_PLATFORM_IMPL_H_
#define CONTENT_RENDERER_RENDERER_BLINK_PLATFORM_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "cc/blink/web_compositor_support_impl.h"
#include "content/child/blink_platform_impl.h"
#include "content/common/content_export.h"
#include "content/renderer/webpublicsuffixlist_impl.h"
#include "device/vibration/vibration_manager.mojom.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBFactory.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationType.h"

namespace cc {
class ContextProvider;
}

namespace IPC {
class SyncMessageFilter;
}

namespace blink {
class WebBatteryStatus;
class WebCanvasCaptureHandler;
class WebDeviceMotionData;
class WebDeviceOrientationData;
class WebGraphicsContext3DProvider;
class WebMediaRecorderHandler;
class WebMediaStream;
class WebServiceWorkerCacheStorage;
}

namespace scheduler {
class RendererScheduler;
class WebThreadImplForRendererScheduler;
}

namespace content {
class BatteryStatusDispatcher;
class DeviceLightEventPump;
class DeviceMotionEventPump;
class DeviceOrientationEventPump;
class PlatformEventObserverBase;
class QuotaMessageFilter;
class RendererClipboardDelegate;
class RenderView;
class ThreadSafeSender;
class WebClipboardImpl;
class WebDatabaseObserverImpl;
class WebFileSystemImpl;

class CONTENT_EXPORT RendererBlinkPlatformImpl : public BlinkPlatformImpl {
 public:
  explicit RendererBlinkPlatformImpl(
      scheduler::RendererScheduler* renderer_scheduler);
  ~RendererBlinkPlatformImpl() override;

  // Shutdown must be called just prior to shutting down blink.
  void Shutdown();

  void set_plugin_refresh_allowed(bool plugin_refresh_allowed) {
    plugin_refresh_allowed_ = plugin_refresh_allowed;
  }
  // Platform methods:
  blink::WebClipboard* clipboard() override;
  blink::WebMimeRegistry* mimeRegistry() override;
  blink::WebFileUtilities* fileUtilities() override;
  blink::WebSandboxSupport* sandboxSupport() override;
  blink::WebCookieJar* cookieJar() override;
  blink::WebThemeEngine* themeEngine() override;
  blink::WebSpeechSynthesizer* createSpeechSynthesizer(
      blink::WebSpeechSynthesizerClient* client) override;
  virtual bool sandboxEnabled();
  unsigned long long visitedLinkHash(const char* canonicalURL,
                                     size_t length) override;
  bool isLinkVisited(unsigned long long linkHash) override;
  void createMessageChannel(blink::WebMessagePortChannel** channel1,
                            blink::WebMessagePortChannel** channel2) override;
  blink::WebPrescientNetworking* prescientNetworking() override;
  void cacheMetadata(const blink::WebURL&,
                     int64_t,
                     const char*,
                     size_t) override;
  blink::WebString defaultLocale() override;
  void suddenTerminationChanged(bool enabled) override;
  blink::WebStorageNamespace* createLocalStorageNamespace() override;
  blink::Platform::FileHandle databaseOpenFile(
      const blink::WebString& vfs_file_name,
      int desired_flags) override;
  int databaseDeleteFile(const blink::WebString& vfs_file_name,
                         bool sync_dir) override;
  long databaseGetFileAttributes(
      const blink::WebString& vfs_file_name) override;
  long long databaseGetFileSize(const blink::WebString& vfs_file_name) override;
  long long databaseGetSpaceAvailableForOrigin(
      const blink::WebString& origin_identifier) override;
  bool databaseSetFileSize(const blink::WebString& vfs_file_name,
                           long long size) override;
  blink::WebString signedPublicKeyAndChallengeString(
      unsigned key_size_index,
      const blink::WebString& challenge,
      const blink::WebURL& url,
      const blink::WebURL& top_origin) override;
  void getPluginList(bool refresh,
                     blink::WebPluginListBuilder* builder) override;
  blink::WebPublicSuffixList* publicSuffixList() override;
  void screenColorProfile(blink::WebVector<char>* to_profile) override;
  blink::WebScrollbarBehavior* scrollbarBehavior() override;
  blink::WebIDBFactory* idbFactory() override;
  blink::WebServiceWorkerCacheStorage* cacheStorage(
      const blink::WebString& origin_identifier) override;
  blink::WebFileSystem* fileSystem() override;
  bool canAccelerate2dCanvas() override;
  bool isThreadedCompositingEnabled() override;
  bool isThreadedAnimationEnabled() override;
  double audioHardwareSampleRate() override;
  size_t audioHardwareBufferSize() override;
  unsigned audioHardwareOutputChannels() override;
  blink::WebDatabaseObserver* databaseObserver() override;

  blink::WebAudioDevice* createAudioDevice(
      size_t buffer_size,
      unsigned input_channels,
      unsigned channels,
      double sample_rate,
      blink::WebAudioDevice::RenderCallback* callback,
      const blink::WebString& input_device_id) override;

  bool loadAudioResource(blink::WebAudioBus* destination_bus,
                         const char* audio_file_data,
                         size_t data_size) override;

  blink::WebMIDIAccessor* createMIDIAccessor(
      blink::WebMIDIAccessorClient* client) override;

  blink::WebBlobRegistry* blobRegistry() override;
  void sampleGamepads(blink::WebGamepads&) override;
  blink::WebRTCPeerConnectionHandler* createRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client) override;
  blink::WebRTCCertificateGenerator* createRTCCertificateGenerator() override;
  blink::WebMediaRecorderHandler* createMediaRecorderHandler() override;
  blink::WebMediaStreamCenter* createMediaStreamCenter(
      blink::WebMediaStreamCenterClient* client) override;
  blink::WebCanvasCaptureHandler* createCanvasCaptureHandler(
      const blink::WebSize& size,
      double frame_rate,
      blink::WebMediaStreamTrack* track) override;
  blink::WebGraphicsContext3D* createOffscreenGraphicsContext3D(
      const blink::WebGraphicsContext3D::Attributes& attributes) override;
  blink::WebGraphicsContext3D* createOffscreenGraphicsContext3D(
      const blink::WebGraphicsContext3D::Attributes& attributes,
      blink::WebGraphicsContext3D* share_context) override;
  blink::WebGraphicsContext3D* createOffscreenGraphicsContext3D(
      const blink::WebGraphicsContext3D::Attributes& attributes,
      blink::WebGraphicsContext3D* share_context,
      blink::WebGraphicsContext3D::WebGraphicsInfo* gl_info) override;
  blink::WebGraphicsContext3DProvider*
  createSharedOffscreenGraphicsContext3DProvider() override;
  blink::WebCompositorSupport* compositorSupport() override;
  blink::WebString convertIDNToUnicode(
      const blink::WebString& host,
      const blink::WebString& languages) override;
  void startListening(blink::WebPlatformEventType,
                      blink::WebPlatformEventListener*) override;
  void stopListening(blink::WebPlatformEventType) override;
  void queryStorageUsageAndQuota(const blink::WebURL& storage_partition,
                                 blink::WebStorageQuotaType,
                                 blink::WebStorageQuotaCallbacks) override;
  void vibrate(unsigned int milliseconds) override;
  void cancelVibration() override;
  blink::WebThread* currentThread() override;
  void recordRappor(const char* metric,
                    const blink::WebString& sample) override;
  void recordRapporURL(const char* metric, const blink::WebURL& url) override;
  double currentTimeSeconds() override;
  double monotonicallyIncreasingTimeSeconds() override;

  // Set the PlatformEventObserverBase in |platform_event_observers_| associated
  // with |type| to |observer|. If there was already an observer associated to
  // the given |type|, it will be replaced.
  // Note that |observer| will be owned by this object after the call.
  void SetPlatformEventObserverForTesting(
      blink::WebPlatformEventType type,
      scoped_ptr<PlatformEventObserverBase> observer);

  // Disables the WebSandboxSupport implementation for testing.
  // Tests that do not set up a full sandbox environment should call
  // SetSandboxEnabledForTesting(false) _before_ creating any instances
  // of this class, to ensure that we don't attempt to use sandbox-related
  // file descriptors or other resources.
  //
  // Returns the previous |enable| value.
  static bool SetSandboxEnabledForTesting(bool enable);

  //  Set a double to return when setDeviceLightListener is invoked.
  static void SetMockDeviceLightDataForTesting(double data);
  // Set WebDeviceMotionData to return when setDeviceMotionListener is invoked.
  static void SetMockDeviceMotionDataForTesting(
      const blink::WebDeviceMotionData& data);
  // Set WebDeviceOrientationData to return when setDeviceOrientationListener
  // is invoked.
  static void SetMockDeviceOrientationDataForTesting(
      const blink::WebDeviceOrientationData& data);

  // Notifies blink::WebBatteryStatusListener that battery status has changed.
  void MockBatteryStatusChangedForTesting(
      const blink::WebBatteryStatus& status);

  WebDatabaseObserverImpl* web_database_observer_impl() {
    return web_database_observer_impl_.get();
  }

  blink::WebURLLoader* createURLLoader() override;

 private:
  bool CheckPreparsedJsCachingEnabled() const;

  // Factory that takes a type and return PlatformEventObserverBase that matches
  // it.
  static PlatformEventObserverBase* CreatePlatformEventObserverFromType(
      blink::WebPlatformEventType type);

  // Use the data previously set via SetMockDevice...DataForTesting() and send
  // them to the registered listener.
  void SendFakeDeviceEventDataForTesting(blink::WebPlatformEventType type);
  device::VibrationManagerPtr& GetConnectedVibrationManagerService();

  scoped_ptr<blink::WebThread> main_thread_;

  scoped_ptr<RendererClipboardDelegate> clipboard_delegate_;
  scoped_ptr<WebClipboardImpl> clipboard_;

  class FileUtilities;
  scoped_ptr<FileUtilities> file_utilities_;

  class MimeRegistry;
  scoped_ptr<MimeRegistry> mime_registry_;

#if !defined(OS_ANDROID) && !defined(OS_WIN)
  class SandboxSupport;
  scoped_ptr<SandboxSupport> sandbox_support_;
#endif

  // This counter keeps track of the number of times sudden termination is
  // enabled or disabled. It starts at 0 (enabled) and for every disable
  // increments by 1, for every enable decrements by 1. When it reaches 0,
  // we tell the browser to enable fast termination.
  int sudden_termination_disables_;

  // If true, then a GetPlugins call is allowed to rescan the disk.
  bool plugin_refresh_allowed_;

  scoped_ptr<blink::WebIDBFactory> web_idb_factory_;

  scoped_ptr<blink::WebBlobRegistry> blob_registry_;

  WebPublicSuffixListImpl public_suffix_list_;

  scoped_ptr<DeviceLightEventPump> device_light_event_pump_;
  scoped_ptr<DeviceMotionEventPump> device_motion_event_pump_;
  scoped_ptr<DeviceOrientationEventPump> device_orientation_event_pump_;

  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner_;
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<QuotaMessageFilter> quota_message_filter_;

  scoped_ptr<WebDatabaseObserverImpl> web_database_observer_impl_;

  cc_blink::WebCompositorSupportImpl compositor_support_;

  scoped_ptr<blink::WebScrollbarBehavior> web_scrollbar_behavior_;

  scoped_ptr<BatteryStatusDispatcher> battery_status_dispatcher_;

  // Handle to the Vibration mojo service.
  device::VibrationManagerPtr vibration_manager_;

  IDMap<PlatformEventObserverBase, IDMapOwnPointer> platform_event_observers_;

  scheduler::RendererScheduler* renderer_scheduler_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(RendererBlinkPlatformImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_BLINK_PLATFORM_IMPL_H_
