// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_blink_web_unit_test_support.h"

#include "base/callback.h"
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
#include "third_party/WebKit/public/platform/WebConnectionType.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebNetworkStateNotifier.h"
#include "third_party/WebKit/public/platform/WebPluginListBuilder.h"
#include "third_party/WebKit/public/platform/WebRTCCertificateGenerator.h"
#include "third_party/WebKit/public/platform/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/scheduler/renderer/renderer_scheduler.h"
#include "third_party/WebKit/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/WebKit/public/web/WebKit.h"
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
#include "third_party/webrtc/rtc_base/rtccertificate.h"  // nogncheck
#endif

using blink::WebString;

namespace {

class DummyTaskRunner : public base::SingleThreadTaskRunner {
 public:
  DummyTaskRunner() : thread_id_(base::PlatformThread::CurrentId()) {}

  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    // Drop the delayed task.
    return false;
  }

  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    // Drop the delayed task.
    return false;
  }

  bool RunsTasksInCurrentSequence() const override {
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

  url_loader_factory_ = blink::WebURLLoaderMockFactory::Create();
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

  blink::Initialize(this);
  blink::SetLayoutTestMode(true);
  blink::WebRuntimeFeatures::EnableDatabase(true);
  blink::WebRuntimeFeatures::EnableNotifications(true);
  blink::WebRuntimeFeatures::EnableTouchEventFeatureDetection(true);

  // Initialize NetworkStateNotifier.
  blink::WebNetworkStateNotifier::SetWebConnection(
      blink::WebConnectionType::kWebConnectionTypeUnknown,
      std::numeric_limits<double>::infinity());

  // Initialize libraries for media.
  media::InitializeMediaLibrary();

  file_utilities_.set_sandbox_enabled(false);

  if (!file_system_root_.CreateUniqueTempDir()) {
    LOG(WARNING) << "Failed to create a temp dir for the filesystem."
                    "FileSystem feature will be disabled.";
    DCHECK(file_system_root_.GetPath().empty());
  }

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

blink::WebBlobRegistry* TestBlinkWebUnitTestSupport::GetBlobRegistry() {
  return &blob_registry_;
}

blink::WebClipboard* TestBlinkWebUnitTestSupport::Clipboard() {
  // Mock out clipboard calls so that tests don't mess
  // with each other's copies/pastes when running in parallel.
  return mock_clipboard_.get();
}

blink::WebFileUtilities* TestBlinkWebUnitTestSupport::GetFileUtilities() {
  return &file_utilities_;
}

blink::WebIDBFactory* TestBlinkWebUnitTestSupport::IdbFactory() {
  NOTREACHED() <<
      "IndexedDB cannot be tested with in-process harnesses.";
  return NULL;
}

std::unique_ptr<blink::WebURLLoader>
TestBlinkWebUnitTestSupport::CreateURLLoader(
    const blink::WebURLRequest& request,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  // This loader should be used only for process-local resources such as
  // data URLs.
  auto default_loader =
      base::MakeUnique<WebURLLoaderImpl>(nullptr, task_runner, nullptr);
  return url_loader_factory_->CreateURLLoader(std::move(default_loader));
}

blink::WebString TestBlinkWebUnitTestSupport::UserAgent() {
  return blink::WebString::FromUTF8("test_runner/0.0.0.0");
}

std::unique_ptr<viz::SharedBitmap>
TestBlinkWebUnitTestSupport::AllocateSharedBitmap(const blink::WebSize& size) {
  return shared_bitmap_manager_
      ->AllocateSharedBitmap(gfx::Size(size.width, size.height));
}

blink::WebString TestBlinkWebUnitTestSupport::QueryLocalizedString(
    blink::WebLocalizedString::Name name) {
  // Returns placeholder strings to check if they are correctly localized.
  switch (name) {
    case blink::WebLocalizedString::kOtherDateLabel:
      return WebString::FromASCII("<<OtherDateLabel>>");
    case blink::WebLocalizedString::kOtherMonthLabel:
      return WebString::FromASCII("<<OtherMonthLabel>>");
    case blink::WebLocalizedString::kOtherWeekLabel:
      return WebString::FromASCII("<<OtherWeekLabel>>");
    case blink::WebLocalizedString::kCalendarClear:
      return WebString::FromASCII("<<CalendarClear>>");
    case blink::WebLocalizedString::kCalendarToday:
      return WebString::FromASCII("<<CalendarToday>>");
    case blink::WebLocalizedString::kThisMonthButtonLabel:
      return WebString::FromASCII("<<ThisMonthLabel>>");
    case blink::WebLocalizedString::kThisWeekButtonLabel:
      return WebString::FromASCII("<<ThisWeekLabel>>");
    case blink::WebLocalizedString::kValidationValueMissing:
      return WebString::FromASCII("<<ValidationValueMissing>>");
    case blink::WebLocalizedString::kValidationValueMissingForSelect:
      return WebString::FromASCII("<<ValidationValueMissingForSelect>>");
    case blink::WebLocalizedString::kWeekFormatTemplate:
      return WebString::FromASCII("Week $2, $1");
    default:
      return blink::WebString();
  }
}

blink::WebString TestBlinkWebUnitTestSupport::QueryLocalizedString(
    blink::WebLocalizedString::Name name,
    const blink::WebString& value) {
  if (name == blink::WebLocalizedString::kValidationRangeUnderflow)
    return blink::WebString::FromASCII("range underflow");
  if (name == blink::WebLocalizedString::kValidationRangeOverflow)
    return blink::WebString::FromASCII("range overflow");
  if (name == blink::WebLocalizedString::kSelectMenuListText)
    return blink::WebString::FromASCII("$1 selected");
  return BlinkPlatformImpl::QueryLocalizedString(name, value);
}

blink::WebString TestBlinkWebUnitTestSupport::QueryLocalizedString(
    blink::WebLocalizedString::Name name,
    const blink::WebString& value1,
    const blink::WebString& value2) {
  if (name == blink::WebLocalizedString::kValidationTooLong)
    return blink::WebString::FromASCII("too long");
  if (name == blink::WebLocalizedString::kValidationStepMismatch)
    return blink::WebString::FromASCII("step mismatch");
  return BlinkPlatformImpl::QueryLocalizedString(name, value1, value2);
}

blink::WebString TestBlinkWebUnitTestSupport::DefaultLocale() {
  return blink::WebString::FromASCII("en-US");
}

blink::WebCompositorSupport* TestBlinkWebUnitTestSupport::CompositorSupport() {
  return &compositor_support_;
}

std::unique_ptr<blink::WebGestureCurve>
TestBlinkWebUnitTestSupport::CreateFlingAnimationCurve(
    blink::WebGestureDevice device_source,
    const blink::WebFloatPoint& velocity,
    const blink::WebSize& cumulative_scroll) {
  return base::MakeUnique<WebGestureCurveMock>(velocity, cumulative_scroll);
}

blink::WebURLLoaderMockFactory*
TestBlinkWebUnitTestSupport::GetURLLoaderMockFactory() {
  return url_loader_factory_.get();
}

blink::WebThread* TestBlinkWebUnitTestSupport::CurrentThread() {
  if (web_thread_ && web_thread_->IsCurrentThread())
    return web_thread_.get();
  return BlinkPlatformImpl::CurrentThread();
}

void TestBlinkWebUnitTestSupport::GetPluginList(
    bool refresh,
    const blink::WebSecurityOrigin& mainFrameOrigin,
    blink::WebPluginListBuilder* builder) {
  builder->AddPlugin("pdf", "pdf", "pdf-files");
  builder->AddMediaTypeToLastPlugin("application/pdf", "pdf");
}

#if BUILDFLAG(ENABLE_WEBRTC)
namespace {

class TestWebRTCCertificateGenerator
    : public blink::WebRTCCertificateGenerator {
  void GenerateCertificate(
      const blink::WebRTCKeyParams& key_params,
      std::unique_ptr<blink::WebRTCCertificateCallback> callback) override {
    NOTIMPLEMENTED();
  }
  void GenerateCertificateWithExpiration(
      const blink::WebRTCKeyParams& key_params,
      uint64_t expires_ms,
      std::unique_ptr<blink::WebRTCCertificateCallback> callback) override {
    NOTIMPLEMENTED();
  }
  bool IsSupportedKeyParams(const blink::WebRTCKeyParams& key_params) override {
    return false;
  }
  std::unique_ptr<blink::WebRTCCertificate> FromPEM(
      blink::WebString pem_private_key,
      blink::WebString pem_certificate) override {
    rtc::scoped_refptr<rtc::RTCCertificate> certificate =
        rtc::RTCCertificate::FromPEM(rtc::RTCCertificatePEM(
            pem_private_key.Utf8(), pem_certificate.Utf8()));
    if (!certificate)
      return nullptr;
    return base::MakeUnique<RTCCertificate>(certificate);
  }
};

}  // namespace
#endif  // BUILDFLAG(ENABLE_WEBRTC)

std::unique_ptr<blink::WebRTCCertificateGenerator>
TestBlinkWebUnitTestSupport::CreateRTCCertificateGenerator() {
#if BUILDFLAG(ENABLE_WEBRTC)
  return base::MakeUnique<TestWebRTCCertificateGenerator>();
#else
  return nullptr;
#endif
}

}  // namespace content
