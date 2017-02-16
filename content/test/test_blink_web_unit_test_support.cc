// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_blink_web_unit_test_support.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/layer_tree_settings.h"
#include "content/app/mojo/mojo_init.h"
#include "content/child/web_url_loader_impl.h"
#include "content/test/mock_webclipboard_impl.h"
#include "content/test/web_gesture_curve_mock.h"
#include "media/base/media.h"
#include "media/media_features.h"
#include "net/cookies/cookie_monster.h"
#include "storage/browser/database/vfs_backend.h"
#include "third_party/WebKit/public/platform/WebConnectionType.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebPluginListBuilder.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/scheduler/renderer/renderer_scheduler.h"
#include "third_party/WebKit/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebNetworkStateNotifier.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "v8/include/v8.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
#include "gin/v8_initializer.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
#include "content/renderer/media/rtc_certificate.h"
#include "third_party/WebKit/public/platform/WebRTCCertificateGenerator.h"
#include "third_party/webrtc/base/rtccertificate.h"  // nogncheck
#endif

using blink::WebString;

namespace {

class DummyTaskRunner : public base::SingleThreadTaskRunner {
 public:
  DummyTaskRunner() : thread_id_(base::PlatformThread::CurrentId()) {}

  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override {
    // Drop the delayed task.
    return false;
  }

  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override {
    // Drop the delayed task.
    return false;
  }

  bool RunsTasksOnCurrentThread() const override {
    return thread_id_ == base::PlatformThread::CurrentId();
  }

 protected:
  ~DummyTaskRunner() override {}

  base::PlatformThreadId thread_id_;

  DISALLOW_COPY_AND_ASSIGN(DummyTaskRunner);
};

}  // namespace

namespace content {

TestBlinkWebUnitTestSupport::TestBlinkWebUnitTestSupport() {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif

  url_loader_factory_ = blink::WebURLLoaderMockFactory::create();
  mock_clipboard_.reset(new MockWebClipboardImpl());

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
  gin::V8Initializer::LoadV8Natives();
#endif

  scoped_refptr<base::SingleThreadTaskRunner> dummy_task_runner;
  std::unique_ptr<base::ThreadTaskRunnerHandle> dummy_task_runner_handle;
  if (!base::ThreadTaskRunnerHandle::IsSet()) {
    // Dummy task runner is initialized here because the blink::initialize
    // creates IsolateHolder which needs the current task runner handle. There
    // should be no task posted to this task runner. The message loop is not
    // created before this initialization because some tests need specific kinds
    // of message loops, and their types are not known upfront. Some tests also
    // create their own thread bundles or message loops, and doing the same in
    // TestBlinkWebUnitTestSupport would introduce a conflict.
    dummy_task_runner = make_scoped_refptr(new DummyTaskRunner());
    dummy_task_runner_handle.reset(
        new base::ThreadTaskRunnerHandle(dummy_task_runner));
  }
  renderer_scheduler_ = blink::scheduler::CreateRendererSchedulerForTests();
  web_thread_ = renderer_scheduler_->CreateMainThread();
  shared_bitmap_manager_.reset(new cc::TestSharedBitmapManager);

  // Set up a FeatureList instance, so that code using that API will not hit a
  // an error that it's not set. Cleared by ClearInstanceForTesting() below.
  base::FeatureList::SetInstance(base::WrapUnique(new base::FeatureList));

  // Initialize mojo firstly to enable Blink initialization to use it.
  InitializeMojo();

  blink::initialize(this);
  blink::setLayoutTestMode(true);
  blink::WebRuntimeFeatures::enableDatabase(true);
  blink::WebRuntimeFeatures::enableNotifications(true);
  blink::WebRuntimeFeatures::enableTouchEventFeatureDetection(true);

  // Initialize NetworkStateNotifier.
  blink::WebNetworkStateNotifier::setWebConnection(
      blink::WebConnectionType::WebConnectionTypeUnknown,
      std::numeric_limits<double>::infinity());

  // Initialize libraries for media.
  media::InitializeMediaLibrary();

  file_utilities_.set_sandbox_enabled(false);

  if (!file_system_root_.CreateUniqueTempDir()) {
    LOG(WARNING) << "Failed to create a temp dir for the filesystem."
                    "FileSystem feature will be disabled.";
    DCHECK(file_system_root_.GetPath().empty());
  }

#if defined(OS_WIN)
  // Ensure we pick up the default theme engine.
  SetThemeEngine(NULL);
#endif

  // Test shell always exposes the GC.
  std::string flags("--expose-gc");
  v8::V8::SetFlagsFromString(flags.c_str(), static_cast<int>(flags.size()));
}

TestBlinkWebUnitTestSupport::~TestBlinkWebUnitTestSupport() {
  url_loader_factory_.reset();
  mock_clipboard_.reset();
  if (renderer_scheduler_)
    renderer_scheduler_->Shutdown();

  // Clear the FeatureList that was registered in the constructor.
  base::FeatureList::ClearInstanceForTesting();
}

blink::WebBlobRegistry* TestBlinkWebUnitTestSupport::getBlobRegistry() {
  return &blob_registry_;
}

blink::WebClipboard* TestBlinkWebUnitTestSupport::clipboard() {
  // Mock out clipboard calls so that tests don't mess
  // with each other's copies/pastes when running in parallel.
  return mock_clipboard_.get();
}

blink::WebFileUtilities* TestBlinkWebUnitTestSupport::fileUtilities() {
  return &file_utilities_;
}

blink::WebIDBFactory* TestBlinkWebUnitTestSupport::idbFactory() {
  NOTREACHED() <<
      "IndexedDB cannot be tested with in-process harnesses.";
  return NULL;
}

blink::WebURLLoader* TestBlinkWebUnitTestSupport::createURLLoader() {
  // This loader should be used only for process-local resources such as
  // data URLs.
  blink::WebURLLoader* default_loader = new WebURLLoaderImpl(nullptr, nullptr);
  return url_loader_factory_->createURLLoader(default_loader);
}

blink::WebString TestBlinkWebUnitTestSupport::userAgent() {
  return blink::WebString::fromUTF8("test_runner/0.0.0.0");
}

std::unique_ptr<cc::SharedBitmap>
TestBlinkWebUnitTestSupport::allocateSharedBitmap(
    const blink::WebSize& size) {
  return shared_bitmap_manager_
      ->AllocateSharedBitmap(gfx::Size(size.width, size.height));
}

blink::WebString TestBlinkWebUnitTestSupport::queryLocalizedString(
    blink::WebLocalizedString::Name name) {
  // Returns placeholder strings to check if they are correctly localized.
  switch (name) {
    case blink::WebLocalizedString::OtherDateLabel:
      return WebString::fromASCII("<<OtherDateLabel>>");
    case blink::WebLocalizedString::OtherMonthLabel:
      return WebString::fromASCII("<<OtherMonthLabel>>");
    case blink::WebLocalizedString::OtherTimeLabel:
      return WebString::fromASCII("<<OtherTimeLabel>>");
    case blink::WebLocalizedString::OtherWeekLabel:
      return WebString::fromASCII("<<OtherWeekLabel>>");
    case blink::WebLocalizedString::CalendarClear:
      return WebString::fromASCII("<<CalendarClear>>");
    case blink::WebLocalizedString::CalendarToday:
      return WebString::fromASCII("<<CalendarToday>>");
    case blink::WebLocalizedString::ThisMonthButtonLabel:
      return WebString::fromASCII("<<ThisMonthLabel>>");
    case blink::WebLocalizedString::ThisWeekButtonLabel:
      return WebString::fromASCII("<<ThisWeekLabel>>");
    case blink::WebLocalizedString::ValidationValueMissing:
      return WebString::fromASCII("<<ValidationValueMissing>>");
    case blink::WebLocalizedString::ValidationValueMissingForSelect:
      return WebString::fromASCII("<<ValidationValueMissingForSelect>>");
    case blink::WebLocalizedString::WeekFormatTemplate:
      return WebString::fromASCII("Week $2, $1");
    default:
      return blink::WebString();
  }
}

blink::WebString TestBlinkWebUnitTestSupport::queryLocalizedString(
    blink::WebLocalizedString::Name name,
    const blink::WebString& value) {
  if (name == blink::WebLocalizedString::ValidationRangeUnderflow)
    return blink::WebString::fromASCII("range underflow");
  if (name == blink::WebLocalizedString::ValidationRangeOverflow)
    return blink::WebString::fromASCII("range overflow");
  if (name == blink::WebLocalizedString::SelectMenuListText)
    return blink::WebString::fromASCII("$1 selected");
  return BlinkPlatformImpl::queryLocalizedString(name, value);
}

blink::WebString TestBlinkWebUnitTestSupport::queryLocalizedString(
    blink::WebLocalizedString::Name name,
    const blink::WebString& value1,
    const blink::WebString& value2) {
  if (name == blink::WebLocalizedString::ValidationTooLong)
    return blink::WebString::fromASCII("too long");
  if (name == blink::WebLocalizedString::ValidationStepMismatch)
    return blink::WebString::fromASCII("step mismatch");
  return BlinkPlatformImpl::queryLocalizedString(name, value1, value2);
}

blink::WebString TestBlinkWebUnitTestSupport::defaultLocale() {
  return blink::WebString::fromASCII("en-US");
}

#if defined(OS_WIN) || defined(OS_MACOSX)
void TestBlinkWebUnitTestSupport::SetThemeEngine(
    blink::WebThemeEngine* engine) {
  active_theme_engine_ = engine ? engine : BlinkPlatformImpl::themeEngine();
}

blink::WebThemeEngine* TestBlinkWebUnitTestSupport::themeEngine() {
  return active_theme_engine_;
}
#endif

blink::WebCompositorSupport* TestBlinkWebUnitTestSupport::compositorSupport() {
  return &compositor_support_;
}

blink::WebGestureCurve* TestBlinkWebUnitTestSupport::createFlingAnimationCurve(
    blink::WebGestureDevice device_source,
    const blink::WebFloatPoint& velocity,
    const blink::WebSize& cumulative_scroll) {
  // Caller will retain and release.
  return new WebGestureCurveMock(velocity, cumulative_scroll);
}

blink::WebURLLoaderMockFactory*
TestBlinkWebUnitTestSupport::getURLLoaderMockFactory() {
  return url_loader_factory_.get();
}

blink::WebThread* TestBlinkWebUnitTestSupport::currentThread() {
  if (web_thread_ && web_thread_->isCurrentThread())
    return web_thread_.get();
  return BlinkPlatformImpl::currentThread();
}

void TestBlinkWebUnitTestSupport::getPluginList(
    bool refresh,
    const blink::WebSecurityOrigin& mainFrameOrigin,
    blink::WebPluginListBuilder* builder) {
  builder->addPlugin("pdf", "pdf", "pdf-files");
  builder->addMediaTypeToLastPlugin("application/pdf", "pdf");
}

#if BUILDFLAG(ENABLE_WEBRTC)
namespace {

class TestWebRTCCertificateGenerator
    : public blink::WebRTCCertificateGenerator {
  void generateCertificate(
      const blink::WebRTCKeyParams& key_params,
      std::unique_ptr<blink::WebRTCCertificateCallback> callback) override {
    NOTIMPLEMENTED();
  }
  void generateCertificateWithExpiration(
      const blink::WebRTCKeyParams& key_params,
      uint64_t expires_ms,
      std::unique_ptr<blink::WebRTCCertificateCallback> callback) override {
    NOTIMPLEMENTED();
  }
  bool isSupportedKeyParams(const blink::WebRTCKeyParams& key_params) override {
    return false;
  }
  std::unique_ptr<blink::WebRTCCertificate> fromPEM(
      blink::WebString pem_private_key,
      blink::WebString pem_certificate) override {
    rtc::scoped_refptr<rtc::RTCCertificate> certificate =
        rtc::RTCCertificate::FromPEM(rtc::RTCCertificatePEM(
            pem_private_key.utf8(), pem_certificate.utf8()));
    if (!certificate)
      return nullptr;
    return base::MakeUnique<RTCCertificate>(certificate);
  }
};

}  // namespace
#endif  // BUILDFLAG(ENABLE_WEBRTC)

blink::WebRTCCertificateGenerator*
TestBlinkWebUnitTestSupport::createRTCCertificateGenerator() {
#if BUILDFLAG(ENABLE_WEBRTC)
  return new TestWebRTCCertificateGenerator();
#else
  return nullptr;
#endif
}

}  // namespace content
