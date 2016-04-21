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
#include "base/thread_task_runner_handle.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/scheduler/renderer/renderer_scheduler_impl.h"
#include "components/scheduler/renderer/webthread_impl_for_renderer_scheduler.h"
#include "components/scheduler/test/lazy_scheduler_message_loop_delegate_for_tests.h"
#include "content/test/mock_webclipboard_impl.h"
#include "content/test/web_gesture_curve_mock.h"
#include "media/base/media.h"
#include "net/cookies/cookie_monster.h"
#include "storage/browser/database/vfs_backend.h"
#include "third_party/WebKit/public/platform/WebConnectionType.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebPluginListBuilder.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebNetworkStateNotifier.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "v8/include/v8.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
#include "gin/v8_initializer.h"
#endif

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
  renderer_scheduler_ = base::WrapUnique(new scheduler::RendererSchedulerImpl(
      scheduler::LazySchedulerMessageLoopDelegateForTests::Create()));
  web_thread_ = renderer_scheduler_->CreateMainThread();

  // Set up a FeatureList instance, so that code using that API will not hit a
  // an error that it's not set. Cleared by ClearInstanceForTesting() below.
  base::FeatureList::SetInstance(base::WrapUnique(new base::FeatureList));

  blink::initialize(this);
  blink::setLayoutTestMode(true);
  blink::WebRuntimeFeatures::enableApplicationCache(true);
  blink::WebRuntimeFeatures::enableDatabase(true);
  blink::WebRuntimeFeatures::enableNotifications(true);
  blink::WebRuntimeFeatures::enableTouch(true);

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
    DCHECK(file_system_root_.path().empty());
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
  blink::shutdown();

  // Clear the FeatureList that was registered in the constructor.
  base::FeatureList::ClearInstanceForTesting();
}

blink::WebBlobRegistry* TestBlinkWebUnitTestSupport::blobRegistry() {
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

blink::WebMimeRegistry* TestBlinkWebUnitTestSupport::mimeRegistry() {
  return &mime_registry_;
}

blink::WebURLLoader* TestBlinkWebUnitTestSupport::createURLLoader() {
  return url_loader_factory_->createURLLoader(
      BlinkPlatformImpl::createURLLoader());
}

blink::WebString TestBlinkWebUnitTestSupport::userAgent() {
  return blink::WebString::fromUTF8("test_runner/0.0.0.0");
}

blink::WebString TestBlinkWebUnitTestSupport::queryLocalizedString(
    blink::WebLocalizedString::Name name) {
  // Returns placeholder strings to check if they are correctly localized.
  switch (name) {
    case blink::WebLocalizedString::OtherDateLabel:
      return base::ASCIIToUTF16("<<OtherDateLabel>>");
    case blink::WebLocalizedString::OtherMonthLabel:
      return base::ASCIIToUTF16("<<OtherMonthLabel>>");
    case blink::WebLocalizedString::OtherTimeLabel:
      return base::ASCIIToUTF16("<<OtherTimeLabel>>");
    case blink::WebLocalizedString::OtherWeekLabel:
      return base::ASCIIToUTF16("<<OtherWeekLabel>>");
    case blink::WebLocalizedString::CalendarClear:
      return base::ASCIIToUTF16("<<CalendarClear>>");
    case blink::WebLocalizedString::CalendarToday:
      return base::ASCIIToUTF16("<<CalendarToday>>");
    case blink::WebLocalizedString::ThisMonthButtonLabel:
      return base::ASCIIToUTF16("<<ThisMonthLabel>>");
    case blink::WebLocalizedString::ThisWeekButtonLabel:
      return base::ASCIIToUTF16("<<ThisWeekLabel>>");
    case blink::WebLocalizedString::ValidationValueMissing:
      return base::ASCIIToUTF16("<<ValidationValueMissing>>");
    case blink::WebLocalizedString::ValidationValueMissingForSelect:
      return base::ASCIIToUTF16("<<ValidationValueMissingForSelect>>");
    case blink::WebLocalizedString::WeekFormatTemplate:
      return base::ASCIIToUTF16("Week $2, $1");
    default:
      return blink::WebString();
  }
}

blink::WebString TestBlinkWebUnitTestSupport::queryLocalizedString(
    blink::WebLocalizedString::Name name,
    const blink::WebString& value) {
  if (name == blink::WebLocalizedString::ValidationRangeUnderflow)
    return base::ASCIIToUTF16("range underflow");
  if (name == blink::WebLocalizedString::ValidationRangeOverflow)
    return base::ASCIIToUTF16("range overflow");
  if (name == blink::WebLocalizedString::SelectMenuListText)
    return base::ASCIIToUTF16("$1 selected");
  return BlinkPlatformImpl::queryLocalizedString(name, value);
}

blink::WebString TestBlinkWebUnitTestSupport::queryLocalizedString(
    blink::WebLocalizedString::Name name,
    const blink::WebString& value1,
    const blink::WebString& value2) {
  if (name == blink::WebLocalizedString::ValidationTooLong)
    return base::ASCIIToUTF16("too long");
  if (name == blink::WebLocalizedString::ValidationStepMismatch)
    return base::ASCIIToUTF16("step mismatch");
  return BlinkPlatformImpl::queryLocalizedString(name, value1, value2);
}

blink::WebString TestBlinkWebUnitTestSupport::defaultLocale() {
  return base::ASCIIToUTF16("en-US");
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
    bool refresh, blink::WebPluginListBuilder* builder) {
  builder->addPlugin("pdf", "pdf", "pdf-files");
  builder->addMediaTypeToLastPlugin("application/pdf", "pdf");
}

}  // namespace content
