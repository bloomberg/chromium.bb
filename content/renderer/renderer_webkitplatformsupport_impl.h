// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_WEBKITPLATFORMSUPPORT_IMPL_H_
#define CONTENT_RENDERER_RENDERER_WEBKITPLATFORMSUPPORT_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "content/common/content_export.h"
#include "content/common/webkitplatformsupport_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSharedWorkerRepository.h"

namespace webkit_glue {
class WebClipboardImpl;
}

namespace content {
class GamepadSharedMemoryReader;
class Hyphenator;
class RendererClipboardClient;
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
  virtual bool sandboxEnabled();
  virtual unsigned long long visitedLinkHash(
      const char* canonicalURL, size_t length);
  virtual bool isLinkVisited(unsigned long long linkHash);
  virtual WebKit::WebMessagePortChannel* createMessagePortChannel();
  virtual void prefetchHostName(const WebKit::WebString&);
  virtual void cacheMetadata(
      const WebKit::WebURL&, double, const char*, size_t);
  virtual WebKit::WebString defaultLocale();
  virtual void suddenTerminationChanged(bool enabled);
  virtual WebKit::WebStorageNamespace* createLocalStorageNamespace(
      const WebKit::WebString& path, unsigned quota);
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
  virtual void screenColorProfile(WebKit::WebVector<char>* to_profile);
  virtual WebKit::WebIDBFactory* idbFactory();
  virtual WebKit::WebFileSystem* fileSystem();
  virtual WebKit::WebSharedWorkerRepository* sharedWorkerRepository();
  virtual bool canAccelerate2dCanvas();
  virtual double audioHardwareSampleRate();
  virtual size_t audioHardwareBufferSize();

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

  virtual WebKit::WebBlobRegistry* blobRegistry();
  virtual void sampleGamepads(WebKit::WebGamepads&);
  virtual WebKit::WebString userAgent(const WebKit::WebURL& url);
  virtual void GetPlugins(bool refresh,
                          std::vector<webkit::WebPluginInfo>* plugins) OVERRIDE;
  virtual WebKit::WebRTCPeerConnectionHandler* createRTCPeerConnectionHandler(
      WebKit::WebRTCPeerConnectionHandlerClient* client);
  virtual WebKit::WebMediaStreamCenter* createMediaStreamCenter(
      WebKit::WebMediaStreamCenterClient* client);
  virtual bool canHyphenate(const WebKit::WebString& locale);
  virtual size_t computeLastHyphenLocation(const char16* characters,
      size_t length,
      size_t before_index,
      const WebKit::WebString& locale);
  virtual bool processMemorySizesInBytes(
      size_t* private_bytes, size_t* shared_bytes);

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

 protected:
  virtual GpuChannelHostFactory* GetGpuChannelHostFactory() OVERRIDE;

 private:
  bool CheckPreparsedJsCachingEnabled() const;

  scoped_ptr<RendererClipboardClient> clipboard_client_;
  scoped_ptr<webkit_glue::WebClipboardImpl> clipboard_;

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

  scoped_ptr<GamepadSharedMemoryReader> gamepad_shared_memory_reader_;

  scoped_ptr<content::Hyphenator> hyphenator_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_WEBKITPLATFORMSUPPORT_IMPL_H_
