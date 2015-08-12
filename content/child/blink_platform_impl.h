// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_
#define CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_

#include "base/compiler_specific.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/threading/thread_local_storage.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "components/webcrypto/webcrypto_impl.h"
#include "content/child/webfallbackthemeengine_impl.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebGestureDevice.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "ui/base/layout.h"

#if defined(USE_DEFAULT_RENDER_THEME)
#include "content/child/webthemeengine_impl_default.h"
#elif defined(OS_WIN)
#include "content/child/webthemeengine_impl_win.h"
#elif defined(OS_MACOSX)
#include "content/child/webthemeengine_impl_mac.h"
#elif defined(OS_ANDROID)
#include "content/child/webthemeengine_impl_android.h"
#endif

namespace base {
class MessageLoop;
}

namespace content {
class BackgroundSyncProvider;
class FlingCurveConfiguration;
class NotificationDispatcher;
class PermissionDispatcher;
class PushDispatcher;
class ThreadSafeSender;
class WebCryptoImpl;
class WebGeofencingProviderImpl;
class WebMemoryDumpProviderAdapter;

class CONTENT_EXPORT BlinkPlatformImpl
    : NON_EXPORTED_BASE(public blink::Platform) {
 public:
  BlinkPlatformImpl();
  explicit BlinkPlatformImpl(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);
  virtual ~BlinkPlatformImpl();

  // Platform methods (partial implementation):
  virtual blink::WebThemeEngine* themeEngine();
  virtual blink::WebFallbackThemeEngine* fallbackThemeEngine();
  virtual blink::Platform::FileHandle databaseOpenFile(
      const blink::WebString& vfs_file_name, int desired_flags);
  virtual int databaseDeleteFile(const blink::WebString& vfs_file_name,
                                 bool sync_dir);
  virtual long databaseGetFileAttributes(
      const blink::WebString& vfs_file_name);
  virtual long long databaseGetFileSize(const blink::WebString& vfs_file_name);
  virtual long long databaseGetSpaceAvailableForOrigin(
      const blink::WebString& origin_identifier);
  virtual bool databaseSetFileSize(
      const blink::WebString& vfs_file_name, long long size);
  virtual blink::WebString signedPublicKeyAndChallengeString(
      unsigned key_size_index, const blink::WebString& challenge,
      const blink::WebURL& url);
  virtual size_t memoryUsageMB();
  virtual size_t actualMemoryUsageMB();
  virtual size_t physicalMemoryMB();
  virtual size_t virtualMemoryLimitMB();
  virtual bool isLowEndDeviceMode();
  virtual size_t numberOfProcessors();

  virtual bool processMemorySizesInBytes(size_t* private_bytes,
                                         size_t* shared_bytes);
  virtual bool memoryAllocatorWasteInBytes(size_t* size);
  virtual blink::WebDiscardableMemory* allocateAndLockDiscardableMemory(
      size_t bytes);
  virtual size_t maxDecodedImageBytes();
  virtual uint32_t getUniqueIdForProcess();
  virtual blink::WebURLLoader* createURLLoader();
  virtual blink::WebSocketHandle* createWebSocketHandle();
  virtual blink::WebString userAgent();
  virtual blink::WebData parseDataURL(
      const blink::WebURL& url, blink::WebString& mimetype,
      blink::WebString& charset);
  virtual blink::WebURLError cancelledError(const blink::WebURL& url) const;
  virtual bool isReservedIPAddress(const blink::WebString& host) const;
  virtual bool portAllowed(const blink::WebURL& url) const;
  virtual blink::WebThread* createThread(const char* name);
  virtual blink::WebThread* currentThread();
  virtual void yieldCurrentThread();
  virtual blink::WebWaitableEvent* createWaitableEvent(
      blink::WebWaitableEvent::ResetPolicy policy,
      blink::WebWaitableEvent::InitialState state);
  virtual blink::WebWaitableEvent* waitMultipleEvents(
      const blink::WebVector<blink::WebWaitableEvent*>& events);
  virtual void decrementStatsCounter(const char* name);
  virtual void incrementStatsCounter(const char* name);
  virtual void histogramCustomCounts(
    const char* name, int sample, int min, int max, int bucket_count);
  virtual void histogramEnumeration(
    const char* name, int sample, int boundary_value);
  virtual void histogramSparse(const char* name, int sample);
  virtual const unsigned char* getTraceCategoryEnabledFlag(
      const char* category_name);
  virtual TraceEventAPIAtomicWord* getTraceSamplingState(
      const unsigned thread_bucket);
  virtual TraceEventHandle addTraceEvent(
      char phase,
      const unsigned char* category_group_enabled,
      const char* name,
      unsigned long long id,
      double timestamp,
      int num_args,
      const char** arg_names,
      const unsigned char* arg_types,
      const unsigned long long* arg_values,
      unsigned char flags);
  virtual TraceEventHandle addTraceEvent(
      char phase,
      const unsigned char* category_group_enabled,
      const char* name,
      unsigned long long id,
      double timestamp,
      int num_args,
      const char** arg_names,
      const unsigned char* arg_types,
      const unsigned long long* arg_values,
      blink::WebConvertableToTraceFormat* convertable_values,
      unsigned char flags);
  virtual void updateTraceEventDuration(
      const unsigned char* category_group_enabled,
      const char* name,
      TraceEventHandle);
  virtual void registerMemoryDumpProvider(blink::WebMemoryDumpProvider* wmdp);
  virtual void unregisterMemoryDumpProvider(blink::WebMemoryDumpProvider* wmdp);
  virtual blink::WebProcessMemoryDump* createProcessMemoryDump();
  virtual blink::Platform::WebMemoryAllocatorDumpGuid
  createWebMemoryAllocatorDumpGuid(const blink::WebString& guidStr);

  virtual blink::WebData loadResource(const char* name);
  virtual blink::WebString queryLocalizedString(
      blink::WebLocalizedString::Name name);
  virtual blink::WebString queryLocalizedString(
      blink::WebLocalizedString::Name name, int numeric_value);
  virtual blink::WebString queryLocalizedString(
      blink::WebLocalizedString::Name name, const blink::WebString& value);
  virtual blink::WebString queryLocalizedString(
      blink::WebLocalizedString::Name name,
      const blink::WebString& value1, const blink::WebString& value2);
  virtual void suddenTerminationChanged(bool enabled) { }
  virtual double currentTime();
  virtual double monotonicallyIncreasingTime();
  virtual double systemTraceTime();
  virtual void cryptographicallyRandomValues(
      unsigned char* buffer, size_t length);
  virtual blink::WebGestureCurve* createFlingAnimationCurve(
      blink::WebGestureDevice device_source,
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulative_scroll);
  virtual void didStartWorkerRunLoop();
  virtual void didStopWorkerRunLoop();
  virtual blink::WebCrypto* crypto();
  virtual blink::WebGeofencingProvider* geofencingProvider();
  virtual blink::WebNotificationManager* notificationManager();
  virtual blink::WebPushProvider* pushProvider();
  virtual blink::WebServicePortProvider* createServicePortProvider(
      blink::WebServicePortProviderClient*);
  virtual blink::WebPermissionClient* permissionClient();
  virtual blink::WebSyncProvider* backgroundSyncProvider();

  virtual blink::WebString domCodeStringFromEnum(int dom_code);
  virtual int domEnumFromCodeString(const blink::WebString& codeString);

 private:
  void InternalInit();
  void UpdateWebThreadTLS(blink::WebThread* thread);

  bool IsMainThread() const;

  scoped_refptr<base::SingleThreadTaskRunner> MainTaskRunnerForCurrentThread();

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  WebThemeEngineImpl native_theme_engine_;
  WebFallbackThemeEngineImpl fallback_theme_engine_;
  base::ThreadLocalStorage::Slot current_thread_slot_;
  webcrypto::WebCryptoImpl web_crypto_;
  scoped_ptr<WebGeofencingProviderImpl> geofencing_provider_;
  base::ScopedPtrHashMap<blink::WebMemoryDumpProvider*,
                         scoped_ptr<WebMemoryDumpProviderAdapter>>
      memory_dump_providers_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<NotificationDispatcher> notification_dispatcher_;
  scoped_refptr<PushDispatcher> push_dispatcher_;
  scoped_ptr<PermissionDispatcher> permission_client_;
  scoped_ptr<BackgroundSyncProvider> sync_provider_;
};

}  // namespace content

#endif  // CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_
