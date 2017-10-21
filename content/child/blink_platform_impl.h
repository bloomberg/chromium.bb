// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_
#define CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_local_storage.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/webcrypto/webcrypto_impl.h"
#include "content/child/webfallbackthemeengine_impl.h"
#include "content/common/content_export.h"
#include "media/blink/webmediacapabilitiesclient_impl.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebGestureDevice.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/public_features.h"
#include "ui/base/layout.h"

#if BUILDFLAG(USE_DEFAULT_RENDER_THEME)
#include "content/child/webthemeengine_impl_default.h"
#elif defined(OS_WIN)
#include "content/child/webthemeengine_impl_win.h"
#elif defined(OS_MACOSX)
#include "content/child/webthemeengine_impl_mac.h"
#elif defined(OS_ANDROID)
#include "content/child/webthemeengine_impl_android.h"
#endif

namespace base {
class WaitableEvent;
}

namespace blink {
namespace scheduler {
class WebThreadBase;
}
}

namespace content {

class WebCryptoImpl;

class CONTENT_EXPORT BlinkPlatformImpl : public blink::Platform {
 public:
  BlinkPlatformImpl();
  explicit BlinkPlatformImpl(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);
  ~BlinkPlatformImpl() override;

  // Platform methods (partial implementation):
  blink::WebThemeEngine* ThemeEngine() override;
  blink::WebFallbackThemeEngine* FallbackThemeEngine() override;
  blink::Platform::FileHandle DatabaseOpenFile(
      const blink::WebString& vfs_file_name,
      int desired_flags) override;
  int DatabaseDeleteFile(const blink::WebString& vfs_file_name,
                         bool sync_dir) override;
  long DatabaseGetFileAttributes(
      const blink::WebString& vfs_file_name) override;
  long long DatabaseGetFileSize(const blink::WebString& vfs_file_name) override;
  long long DatabaseGetSpaceAvailableForOrigin(
      const blink::WebSecurityOrigin& origin) override;
  bool DatabaseSetFileSize(const blink::WebString& vfs_file_name,
                           long long size) override;
  size_t ActualMemoryUsageMB() override;
  size_t NumberOfProcessors() override;

  size_t MaxDecodedImageBytes() override;
  bool IsLowEndDevice() override;
  uint32_t GetUniqueIdForProcess() override;
  blink::WebString UserAgent() override;
  std::unique_ptr<blink::WebThread> CreateThread(const char* name) override;
  std::unique_ptr<blink::WebThread> CreateWebAudioThread() override;
  blink::WebThread* CurrentThread() override;
  void RecordAction(const blink::UserMetricsAction&) override;

  blink::WebData GetDataResource(const char* name) override;
  blink::WebString QueryLocalizedString(
      blink::WebLocalizedString::Name name) override;
  virtual blink::WebString queryLocalizedString(
      blink::WebLocalizedString::Name name,
      int numeric_value);
  blink::WebString QueryLocalizedString(blink::WebLocalizedString::Name name,
                                        const blink::WebString& value) override;
  blink::WebString QueryLocalizedString(
      blink::WebLocalizedString::Name name,
      const blink::WebString& value1,
      const blink::WebString& value2) override;
  void SuddenTerminationChanged(bool enabled) override {}
  blink::WebThread* CompositorThread() const override;
  std::unique_ptr<blink::WebGestureCurve> CreateFlingAnimationCurve(
      blink::WebGestureDevice device_source,
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulative_scroll) override;
  void DidStartWorkerThread() override;
  void WillStopWorkerThread() override;
  bool AllowScriptExtensionForServiceWorker(
      const blink::WebURL& script_url) override;
  blink::WebCrypto* Crypto() override;
  const char* GetBrowserServiceName() const override;
  blink::WebMediaCapabilitiesClient* MediaCapabilitiesClient() override;

  blink::WebString DomCodeStringFromEnum(int dom_code) override;
  int DomEnumFromCodeString(const blink::WebString& codeString) override;
  blink::WebString DomKeyStringFromEnum(int dom_key) override;
  int DomKeyEnumFromString(const blink::WebString& key_string) override;
  bool IsDomKeyForModifier(int dom_key) override;

  // This class does *not* own the compositor thread. It is the responsibility
  // of the caller to ensure that the compositor thread is cleared before it is
  // destructed.
  void SetCompositorThread(blink::scheduler::WebThreadBase* compositor_thread);

  std::unique_ptr<blink::WebFeaturePolicy> CreateFeaturePolicy(
      const blink::WebFeaturePolicy* parentPolicy,
      const blink::WebParsedFeaturePolicy& containerPolicy,
      const blink::WebParsedFeaturePolicy& policyHeader,
      const blink::WebSecurityOrigin& origin) override;
  std::unique_ptr<blink::WebFeaturePolicy> DuplicateFeaturePolicyWithOrigin(
      const blink::WebFeaturePolicy& policy,
      const blink::WebSecurityOrigin& new_origin) override;

 private:
  void WaitUntilWebThreadTLSUpdate(blink::scheduler::WebThreadBase* thread);
  void UpdateWebThreadTLS(blink::WebThread* thread, base::WaitableEvent* event);

  bool IsMainThread() const;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  WebThemeEngineImpl native_theme_engine_;
  WebFallbackThemeEngineImpl fallback_theme_engine_;
  base::ThreadLocalStorage::Slot current_thread_slot_;
  webcrypto::WebCryptoImpl web_crypto_;
  media::WebMediaCapabilitiesClientImpl media_capabilities_client_;

  blink::scheduler::WebThreadBase* compositor_thread_;
};

}  // namespace content

#endif  // CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_
