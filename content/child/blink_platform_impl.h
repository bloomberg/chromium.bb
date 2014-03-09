// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_
#define CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_

#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/platform_file.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "ui/base/layout.h"
#include "webkit/child/resource_loader_bridge.h"

namespace base {
class MessageLoop;
}

namespace blink {
class WebSocketStreamHandle;
}

namespace content {

class WebSocketStreamHandleDelegate;
class WebSocketStreamHandleBridge;

class CONTENT_EXPORT BlinkPlatformImpl
    : NON_EXPORTED_BASE(public blink::Platform) {
 public:
  BlinkPlatformImpl();
  virtual ~BlinkPlatformImpl();

  // Platform methods (partial implementation):
  virtual base::PlatformFile databaseOpenFile(
      const blink::WebString& vfs_file_name, int desired_flags);
  virtual int databaseDeleteFile(const blink::WebString& vfs_file_name,
                                 bool sync_dir);
  virtual long databaseGetFileAttributes(
      const blink::WebString& vfs_file_name);
  virtual long long databaseGetFileSize(const blink::WebString& vfs_file_name);
  virtual long long databaseGetSpaceAvailableForOrigin(
      const blink::WebString& origin_identifier);
  virtual blink::WebString signedPublicKeyAndChallengeString(
      unsigned key_size_index, const blink::WebString& challenge,
      const blink::WebURL& url);
  virtual size_t memoryUsageMB();
  virtual size_t actualMemoryUsageMB();
  virtual size_t physicalMemoryMB();
  virtual size_t numberOfProcessors();

  virtual void startHeapProfiling(const blink::WebString& prefix);
  virtual void stopHeapProfiling() OVERRIDE;
  virtual void dumpHeapProfiling(const blink::WebString& reason);
  virtual blink::WebString getHeapProfile() OVERRIDE;

  virtual bool processMemorySizesInBytes(size_t* private_bytes,
                                         size_t* shared_bytes);
  virtual bool memoryAllocatorWasteInBytes(size_t* size);
  virtual size_t maxDecodedImageBytes() OVERRIDE;
  virtual blink::WebURLLoader* createURLLoader();
  virtual blink::WebSocketStreamHandle* createSocketStreamHandle();
  virtual blink::WebString userAgent();
  // TODO(jam): remove this after Blink is updated
  virtual blink::WebString userAgent(const blink::WebURL& url);
  virtual blink::WebData parseDataURL(
      const blink::WebURL& url, blink::WebString& mimetype,
      blink::WebString& charset);
  virtual blink::WebURLError cancelledError(const blink::WebURL& url) const;
  virtual void decrementStatsCounter(const char* name);
  virtual void incrementStatsCounter(const char* name);
  virtual void histogramCustomCounts(
    const char* name, int sample, int min, int max, int bucket_count);
  virtual void histogramEnumeration(
    const char* name, int sample, int boundary_value);
  virtual void histogramSparse(const char* name, int sample);
  virtual const unsigned char* getTraceCategoryEnabledFlag(
      const char* category_name);
  virtual long* getTraceSamplingState(const unsigned thread_bucket);
  virtual TraceEventHandle addTraceEvent(
      char phase,
      const unsigned char* category_group_enabled,
      const char* name,
      unsigned long long id,
      int num_args,
      const char** arg_names,
      const unsigned char* arg_types,
      const unsigned long long* arg_values,
      unsigned char flags);
  virtual void updateTraceEventDuration(
      const unsigned char* category_group_enabled,
      const char* name,
      TraceEventHandle);
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
  virtual void cryptographicallyRandomValues(
      unsigned char* buffer, size_t length);
  virtual void setSharedTimerFiredFunction(void (*func)());
  virtual void setSharedTimerFireInterval(double interval_seconds);
  virtual void stopSharedTimer();
  virtual void callOnMainThread(void (*func)(void*), void* context);


  // Embedder functions. The following are not implemented by the glue layer and
  // need to be specialized by the embedder.

  // Gets a localized string given a message id.  Returns an empty string if the
  // message id is not found.
  virtual base::string16 GetLocalizedString(int message_id) = 0;

  // Returns the raw data for a resource.  This resource must have been
  // specified as BINDATA in the relevant .rc file.
  virtual base::StringPiece GetDataResource(int resource_id,
                                            ui::ScaleFactor scale_factor) = 0;

  // Creates a ResourceLoaderBridge.
  virtual webkit_glue::ResourceLoaderBridge* CreateResourceLoader(
      const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info) = 0;
  // Creates a WebSocketStreamHandleBridge.
  virtual WebSocketStreamHandleBridge* CreateWebSocketStreamBridge(
      blink::WebSocketStreamHandle* handle,
      WebSocketStreamHandleDelegate* delegate) = 0;

  void SuspendSharedTimer();
  void ResumeSharedTimer();
  virtual void OnStartSharedTimer(base::TimeDelta delay) {}

 private:
  void DoTimeout() {
    if (shared_timer_func_ && !shared_timer_suspended_)
      shared_timer_func_();
  }

  base::MessageLoop* main_loop_;
  base::OneShotTimer<BlinkPlatformImpl> shared_timer_;
  void (*shared_timer_func_)();
  double shared_timer_fire_time_;
  bool shared_timer_fire_time_was_set_while_suspended_;
  int shared_timer_suspended_;  // counter
};

}  // namespace content

#endif  // CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_
