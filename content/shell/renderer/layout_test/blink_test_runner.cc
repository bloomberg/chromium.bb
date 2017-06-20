// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/layout_test/blink_test_runner.h"

#include <stddef.h>

#include <algorithm>
#include <clocale>
#include <cmath>
#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/debugger.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/md5.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/plugins/renderer/plugin_placeholder.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "content/public/renderer/media_stream_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/public/renderer/renderer_gamepad_provider.h"
#include "content/public/test/layouttest_support.h"
#include "content/shell/common/layout_test/layout_test_messages.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/layout_test/blink_test_helpers.h"
#include "content/shell/renderer/layout_test/layout_test_render_thread_observer.h"
#include "content/shell/renderer/layout_test/leak_detector.h"
#include "content/shell/test_runner/app_banner_service.h"
#include "content/shell/test_runner/gamepad_controller.h"
#include "content/shell/test_runner/layout_and_paint_async_then.h"
#include "content/shell/test_runner/pixel_dump.h"
#include "content/shell/test_runner/web_test_interfaces.h"
#include "content/shell/test_runner/web_test_runner.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "media/capture/video_capturer_source.h"
#include "media/media_features.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/FilePathConversion.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/platform/modules/app_banner/app_banner.mojom.h"
#include "third_party/WebKit/public/web/WebArrayBufferView.h"
#include "third_party/WebKit/public/web/WebContextMenuData.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDevToolsAgent.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLeakDetector.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebTestingSupport.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/icc_profile.h"

using blink::Platform;
using blink::WebArrayBufferView;
using blink::WebContextMenuData;
using blink::WebDevToolsAgent;
using device::MotionData;
using device::OrientationData;
using blink::WebElement;
using blink::WebLocalFrame;
using blink::WebHistoryItem;
using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebPoint;
using blink::WebRect;
using blink::WebScriptSource;
using blink::WebSize;
using blink::WebString;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebTestingSupport;
using blink::WebTraceLocation;
using blink::WebThread;
using blink::WebVector;
using blink::WebView;

namespace content {

namespace {

class SyncNavigationStateVisitor : public RenderViewVisitor {
 public:
  SyncNavigationStateVisitor() {}
  ~SyncNavigationStateVisitor() override {}

  bool Visit(RenderView* render_view) override {
    SyncNavigationState(render_view);
    return true;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(SyncNavigationStateVisitor);
};

class UseSynchronousResizeModeVisitor : public RenderViewVisitor {
 public:
  explicit UseSynchronousResizeModeVisitor(bool enable) : enable_(enable) {}
  ~UseSynchronousResizeModeVisitor() override {}

  bool Visit(RenderView* render_view) override {
    UseSynchronousResizeMode(render_view, enable_);
    return true;
  }

 private:
  bool enable_;
};

class MockGamepadProvider : public RendererGamepadProvider {
 public:
  explicit MockGamepadProvider(test_runner::GamepadController* controller)
      : RendererGamepadProvider(nullptr), controller_(controller) {}
  ~MockGamepadProvider() override {
    StopIfObserving();
  }

  // RendererGamepadProvider implementation.
  void SampleGamepads(device::Gamepads& gamepads) override {
    controller_->SampleGamepads(gamepads);
  }
  void Start(blink::WebPlatformEventListener* listener) override {
    controller_->SetListener(static_cast<blink::WebGamepadListener*>(listener));
    RendererGamepadProvider::Start(listener);
  }
  void SendStartMessage() override {}
  void SendStopMessage() override {}

 private:
  std::unique_ptr<test_runner::GamepadController> controller_;

  DISALLOW_COPY_AND_ASSIGN(MockGamepadProvider);
};

class MockVideoCapturerSource : public media::VideoCapturerSource {
 public:
  MockVideoCapturerSource() = default;
  ~MockVideoCapturerSource() override {}

  media::VideoCaptureFormats GetPreferredFormats() override {
    const int supported_width = 640;
    const int supported_height = 480;
    const float supported_framerate = 60.0;
    return media::VideoCaptureFormats(
        1, media::VideoCaptureFormat(
               gfx::Size(supported_width, supported_height),
               supported_framerate, media::PIXEL_FORMAT_I420));
  }
  void StartCapture(const media::VideoCaptureParams& params,
                    const VideoCaptureDeliverFrameCB& new_frame_callback,
                    const RunningCallback& running_callback) override {
    running_callback.Run(true);
  }
  void StopCapture() override {}
};

class MockAudioCapturerSource : public media::AudioCapturerSource {
 public:
  MockAudioCapturerSource() = default;

  void Initialize(const media::AudioParameters& params,
                  CaptureCallback* callback,
                  int session_id) override {}
  void Start() override {}
  void Stop() override {}
  void SetVolume(double volume) override {}
  void SetAutomaticGainControl(bool enable) override {}

 protected:
  ~MockAudioCapturerSource() override {}
};

// Tests in csswg-test use absolute path links such as
//   <script src="/resources/testharness.js">.
// Because we load the tests as local files, such links don't work.
// This function fixes this issue by rewriting file: URLs which were produced
// from such links so that they point actual files in LayoutTests/resources/.
//
// Note that this isn't applied to external/wpt because tests in external/wpt
// are accessed via http.
WebURL RewriteAbsolutePathInCsswgTest(const std::string& utf8_url) {
  const char kFileScheme[] = "file:///";
  const int kFileSchemeLen = arraysize(kFileScheme) - 1;
  if (utf8_url.compare(0, kFileSchemeLen, kFileScheme, kFileSchemeLen) != 0)
    return WebURL();
  if (utf8_url.find("/LayoutTests/") != std::string::npos)
    return WebURL();
#if defined(OS_WIN)
  // +3 for a drive letter, :, and /.
  const int kFileSchemeAndDriveLen = kFileSchemeLen + 3;
  if (utf8_url.size() <= kFileSchemeAndDriveLen)
    return WebURL();
  std::string path = utf8_url.substr(kFileSchemeAndDriveLen);
#else
  std::string path = utf8_url.substr(kFileSchemeLen);
#endif
  base::FilePath new_path =
      LayoutTestRenderThreadObserver::GetInstance()
          ->webkit_source_dir()
          .Append(FILE_PATH_LITERAL("LayoutTests/"))
          .AppendASCII(path);
  return WebURL(net::FilePathToFileURL(new_path));
}

}  // namespace

BlinkTestRunner::BlinkTestRunner(RenderView* render_view)
    : RenderViewObserver(render_view),
      RenderViewObserverTracker<BlinkTestRunner>(render_view),
      test_config_(mojom::ShellTestConfiguration::New()),
      is_main_window_(false),
      focus_on_next_commit_(false),
      leak_detector_(new LeakDetector(this)) {}

BlinkTestRunner::~BlinkTestRunner() {
}

// WebTestDelegate  -----------------------------------------------------------

void BlinkTestRunner::ClearEditCommand() {
  render_view()->ClearEditCommands();
}

void BlinkTestRunner::SetEditCommand(const std::string& name,
                                     const std::string& value) {
  render_view()->SetEditCommandForNextKeyEvent(name, value);
}

void BlinkTestRunner::SetGamepadProvider(
    test_runner::GamepadController* controller) {
  std::unique_ptr<MockGamepadProvider> provider(
      new MockGamepadProvider(controller));
  SetMockGamepadProvider(std::move(provider));
}

void BlinkTestRunner::SetDeviceMotionData(const MotionData& data) {
  SetMockDeviceMotionData(data);
}

void BlinkTestRunner::SetDeviceOrientationData(const OrientationData& data) {
  SetMockDeviceOrientationData(data);
}

void BlinkTestRunner::PrintMessageToStderr(const std::string& message) {
  Send(new ShellViewHostMsg_PrintMessageToStderr(routing_id(), message));
}

void BlinkTestRunner::PrintMessage(const std::string& message) {
  Send(new ShellViewHostMsg_PrintMessage(routing_id(), message));
}

void BlinkTestRunner::PostTask(const base::Closure& task) {
  Platform::Current()->CurrentThread()->GetSingleThreadTaskRunner()->PostTask(
      FROM_HERE, task);
}

void BlinkTestRunner::PostDelayedTask(const base::Closure& task, long long ms) {
  Platform::Current()
      ->CurrentThread()
      ->GetSingleThreadTaskRunner()
      ->PostDelayedTask(FROM_HERE, task, base::TimeDelta::FromMilliseconds(ms));
}

WebString BlinkTestRunner::RegisterIsolatedFileSystem(
    const blink::WebVector<blink::WebString>& absolute_filenames) {
  std::vector<base::FilePath> files;
  for (size_t i = 0; i < absolute_filenames.size(); ++i)
    files.push_back(blink::WebStringToFilePath(absolute_filenames[i]));
  std::string filesystem_id;
  Send(new LayoutTestHostMsg_RegisterIsolatedFileSystem(
      routing_id(), files, &filesystem_id));
  return WebString::FromUTF8(filesystem_id);
}

long long BlinkTestRunner::GetCurrentTimeInMillisecond() {
  return base::TimeDelta(base::Time::Now() -
                         base::Time::UnixEpoch()).ToInternalValue() /
         base::Time::kMicrosecondsPerMillisecond;
}

WebString BlinkTestRunner::GetAbsoluteWebStringFromUTF8Path(
    const std::string& utf8_path) {
  base::FilePath path = base::FilePath::FromUTF8Unsafe(utf8_path);
  if (!path.IsAbsolute()) {
    GURL base_url =
        net::FilePathToFileURL(test_config_->current_working_directory.Append(
            FILE_PATH_LITERAL("foo")));
    net::FileURLToFilePath(base_url.Resolve(utf8_path), &path);
  }
  return blink::FilePathToWebString(path);
}

WebURL BlinkTestRunner::LocalFileToDataURL(const WebURL& file_url) {
  base::FilePath local_path;
  if (!net::FileURLToFilePath(file_url, &local_path))
    return WebURL();

  std::string contents;
  Send(new LayoutTestHostMsg_ReadFileToString(
        routing_id(), local_path, &contents));

  std::string contents_base64;
  base::Base64Encode(contents, &contents_base64);

  const char data_url_prefix[] = "data:text/css:charset=utf-8;base64,";
  return WebURL(GURL(data_url_prefix + contents_base64));
}

WebURL BlinkTestRunner::RewriteLayoutTestsURL(const std::string& utf8_url,
                                              bool is_wpt_mode) {
  if (is_wpt_mode) {
    WebURL rewritten_url = RewriteAbsolutePathInCsswgTest(utf8_url);
    if (!rewritten_url.IsEmpty())
      return rewritten_url;
    return WebURL(GURL(utf8_url));
  }

  const char kGenPrefix[] = "file:///gen/";
  const int kGenPrefixLen = arraysize(kGenPrefix) - 1;

  // Map "file:///gen/" to "file://<build directory>/gen/".
  if (!utf8_url.compare(0, kGenPrefixLen, kGenPrefix, kGenPrefixLen)) {
    base::FilePath gen_directory_path =
        test_config_->build_directory.Append(FILE_PATH_LITERAL("gen/"));
    std::string new_url = std::string("file://") +
                          gen_directory_path.AsUTF8Unsafe() +
                          utf8_url.substr(kGenPrefixLen);
    return WebURL(GURL(new_url));
  }

  const char kPrefix[] = "file:///tmp/LayoutTests/";
  const int kPrefixLen = arraysize(kPrefix) - 1;

  if (utf8_url.compare(0, kPrefixLen, kPrefix, kPrefixLen))
    return WebURL(GURL(utf8_url));

  base::FilePath replace_path =
      LayoutTestRenderThreadObserver::GetInstance()->webkit_source_dir()
          .Append(FILE_PATH_LITERAL("LayoutTests/"));
  std::string utf8_path = replace_path.AsUTF8Unsafe();
  std::string new_url =
      std::string("file://") + utf8_path + utf8_url.substr(kPrefixLen);
  return WebURL(GURL(new_url));
}

test_runner::TestPreferences* BlinkTestRunner::Preferences() {
  return &prefs_;
}

void BlinkTestRunner::ApplyPreferences() {
  WebPreferences prefs = render_view()->GetWebkitPreferences();
  ExportLayoutTestSpecificPreferences(prefs_, &prefs);
  render_view()->SetWebkitPreferences(prefs);
  Send(new ShellViewHostMsg_OverridePreferences(routing_id(), prefs));
}

void BlinkTestRunner::SetPopupBlockingEnabled(bool block_popups) {
  Send(
      new ShellViewHostMsg_SetPopupBlockingEnabled(routing_id(), block_popups));
}

std::string BlinkTestRunner::makeURLErrorDescription(const WebURLError& error) {
  std::string domain = error.domain.Utf8();
  int code = error.reason;

  if (domain == net::kErrorDomain) {
    domain = "NSURLErrorDomain";
    switch (error.reason) {
    case net::ERR_ABORTED:
      code = -999;  // NSURLErrorCancelled
      break;
    case net::ERR_UNSAFE_PORT:
      // Our unsafe port checking happens at the network stack level, but we
      // make this translation here to match the behavior of stock WebKit.
      domain = "WebKitErrorDomain";
      code = 103;
      break;
    case net::ERR_ADDRESS_INVALID:
    case net::ERR_ADDRESS_UNREACHABLE:
    case net::ERR_NETWORK_ACCESS_DENIED:
      code = -1004;  // NSURLErrorCannotConnectToHost
      break;
    }
  } else {
    DLOG(WARNING) << "Unknown error domain";
  }

  return base::StringPrintf("<NSError domain %s, code %d, failing URL \"%s\">",
                            domain.c_str(), code,
                            error.unreachable_url.GetString().Utf8().data());
}

void BlinkTestRunner::UseUnfortunateSynchronousResizeMode(bool enable) {
  UseSynchronousResizeModeVisitor visitor(enable);
  RenderView::ForEach(&visitor);
}

void BlinkTestRunner::EnableAutoResizeMode(const WebSize& min_size,
                                           const WebSize& max_size) {
  content::EnableAutoResizeMode(render_view(), min_size, max_size);
}

void BlinkTestRunner::DisableAutoResizeMode(const WebSize& new_size) {
  content::DisableAutoResizeMode(render_view(), new_size);
  if (!new_size.IsEmpty())
    ForceResizeRenderView(render_view(), new_size);
}

void BlinkTestRunner::ClearDevToolsLocalStorage() {
  Send(new ShellViewHostMsg_ClearDevToolsLocalStorage(routing_id()));
}

void BlinkTestRunner::ShowDevTools(const std::string& settings,
                                   const std::string& frontend_url) {
  Send(new ShellViewHostMsg_ShowDevTools(
      routing_id(), settings, frontend_url));
}

void BlinkTestRunner::CloseDevTools() {
  Send(new ShellViewHostMsg_CloseDevTools(routing_id()));
  render_view()->GetMainRenderFrame()->DetachDevToolsForTest();
}

void BlinkTestRunner::EvaluateInWebInspector(int call_id,
                                             const std::string& script) {
  Send(new ShellViewHostMsg_EvaluateInDevTools(
      routing_id(), call_id, script));
}

std::string BlinkTestRunner::EvaluateInWebInspectorOverlay(
    const std::string& script) {
  WebDevToolsAgent* agent =
      render_view()->GetMainRenderFrame()->GetWebFrame()->DevToolsAgent();
  if (!agent)
    return std::string();

  return agent->EvaluateInWebInspectorOverlay(WebString::FromUTF8(script))
      .Utf8();
}

void BlinkTestRunner::ClearAllDatabases() {
  Send(new LayoutTestHostMsg_ClearAllDatabases(routing_id()));
}

void BlinkTestRunner::SetDatabaseQuota(int quota) {
  Send(new LayoutTestHostMsg_SetDatabaseQuota(routing_id(), quota));
}

void BlinkTestRunner::SimulateWebNotificationClick(
    const std::string& title,
    int action_index,
    const base::NullableString16& reply) {
  Send(new LayoutTestHostMsg_SimulateWebNotificationClick(routing_id(), title,
                                                          action_index, reply));
}

void BlinkTestRunner::SimulateWebNotificationClose(const std::string& title,
                                                   bool by_user) {
  Send(new LayoutTestHostMsg_SimulateWebNotificationClose(routing_id(), title,
                                                          by_user));
}

void BlinkTestRunner::SetDeviceScaleFactor(float factor) {
  content::SetDeviceScaleFactor(render_view(), factor);
}

float BlinkTestRunner::GetWindowToViewportScale() {
  return content::GetWindowToViewportScale(render_view());
}

std::unique_ptr<blink::WebInputEvent>
BlinkTestRunner::TransformScreenToWidgetCoordinates(
    test_runner::WebWidgetTestProxyBase* web_widget_test_proxy_base,
    const blink::WebInputEvent& event) {
  return content::TransformScreenToWidgetCoordinates(web_widget_test_proxy_base,
                                                     event);
}

test_runner::WebWidgetTestProxyBase* BlinkTestRunner::GetWebWidgetTestProxyBase(
    blink::WebLocalFrame* frame) {
  return content::GetWebWidgetTestProxyBase(frame);
}

void BlinkTestRunner::EnableUseZoomForDSF() {
  base::CommandLine::ForCurrentProcess()->
      AppendSwitch(switches::kEnableUseZoomForDSF);
}

bool BlinkTestRunner::IsUseZoomForDSFEnabled() {
  return content::IsUseZoomForDSFEnabled();
}

void BlinkTestRunner::SetDeviceColorProfile(const std::string& name) {
  content::SetDeviceColorProfile(render_view(), GetTestingICCProfile(name));
}

void BlinkTestRunner::SetBluetoothFakeAdapter(const std::string& adapter_name,
                                              const base::Closure& callback) {
  GetBluetoothFakeAdapterSetter().Set(adapter_name, callback);
}

void BlinkTestRunner::SetBluetoothManualChooser(bool enable) {
  Send(new ShellViewHostMsg_SetBluetoothManualChooser(routing_id(), enable));
}

void BlinkTestRunner::GetBluetoothManualChooserEvents(
    const base::Callback<void(const std::vector<std::string>&)>& callback) {
  get_bluetooth_events_callbacks_.push_back(callback);
  Send(new ShellViewHostMsg_GetBluetoothManualChooserEvents(routing_id()));
}

void BlinkTestRunner::SendBluetoothManualChooserEvent(
    const std::string& event,
    const std::string& argument) {
  Send(new ShellViewHostMsg_SendBluetoothManualChooserEvent(routing_id(), event,
                                                            argument));
}

void BlinkTestRunner::SetFocus(blink::WebView* web_view, bool focus) {
  RenderView* render_view = RenderView::FromWebView(web_view);
  if (render_view)  // Check whether |web_view| has been already closed.
    SetFocusAndActivate(render_view, focus);
}

void BlinkTestRunner::SetBlockThirdPartyCookies(bool block) {
  Send(new LayoutTestHostMsg_BlockThirdPartyCookies(routing_id(), block));
}

std::string BlinkTestRunner::PathToLocalResource(const std::string& resource) {
#if defined(OS_WIN)
  if (base::StartsWith(resource, "/tmp/", base::CompareCase::SENSITIVE)) {
    // We want a temp file.
    GURL base_url = net::FilePathToFileURL(test_config_->temp_path);
    return base_url.Resolve(resource.substr(sizeof("/tmp/") - 1)).spec();
  }
#endif

  // Some layout tests use file://// which we resolve as a UNC path. Normalize
  // them to just file:///.
  std::string result = resource;
  static const size_t kFileLen = sizeof("file:///") - 1;
  while (base::StartsWith(base::ToLowerASCII(result), "file:////",
                          base::CompareCase::SENSITIVE)) {
    result = result.substr(0, kFileLen) + result.substr(kFileLen + 1);
  }
  return RewriteLayoutTestsURL(result, false /* is_wpt_mode */)
      .GetString()
      .Utf8();
}

void BlinkTestRunner::SetLocale(const std::string& locale) {
  setlocale(LC_ALL, locale.c_str());
}

void BlinkTestRunner::OnLayoutTestRuntimeFlagsChanged(
    const base::DictionaryValue& changed_values) {
  // Ignore changes that happen before we got the initial, accumulated
  // layout flag changes in either OnReplicateTestConfiguration or
  // OnSetTestConfiguration.
  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderThreadObserver::GetInstance()->test_interfaces();
  if (!interfaces->TestIsRunning())
    return;

  RenderThread::Get()->Send(
      new LayoutTestHostMsg_LayoutTestRuntimeFlagsChanged(changed_values));
}

void BlinkTestRunner::TestFinished() {
  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderThreadObserver::GetInstance()->test_interfaces();
  interfaces->SetTestIsRunning(false);

  if (!is_main_window_ || !render_view()->GetMainRenderFrame()) {
    RenderThread::Get()->Send(
        new LayoutTestHostMsg_TestFinishedInSecondaryRenderer());
    return;
  }

  if (interfaces->TestRunner()->ShouldDumpBackForwardList()) {
    SyncNavigationStateVisitor visitor;
    RenderView::ForEach(&visitor);
    Send(new ShellViewHostMsg_CaptureSessionHistory(routing_id()));
  } else {
    // clean out the lifecycle if needed before capturing the layout tree
    // dump and pixels from the compositor.
    render_view()
        ->GetWebView()
        ->MainFrame()
        ->ToWebLocalFrame()
        ->FrameWidget()
        ->UpdateAllLifecyclePhases();
    CaptureDump();
  }
}

void BlinkTestRunner::CloseRemainingWindows() {
  Send(new ShellViewHostMsg_CloseRemainingWindows(routing_id()));
}

void BlinkTestRunner::DeleteAllCookies() {
  Send(new LayoutTestHostMsg_DeleteAllCookies(routing_id()));
}

int BlinkTestRunner::NavigationEntryCount() {
  return GetLocalSessionHistoryLength(render_view());
}

void BlinkTestRunner::GoToOffset(int offset) {
  Send(new ShellViewHostMsg_GoToOffset(routing_id(), offset));
}

void BlinkTestRunner::Reload() {
  Send(new ShellViewHostMsg_Reload(routing_id()));
}

void BlinkTestRunner::LoadURLForFrame(const WebURL& url,
                                      const std::string& frame_name) {
  Send(new ShellViewHostMsg_LoadURLForFrame(
      routing_id(), url, frame_name));
}

bool BlinkTestRunner::AllowExternalPages() {
  return test_config_->allow_external_pages;
}

std::string BlinkTestRunner::DumpHistoryForWindow(blink::WebView* web_view) {
  size_t pos = 0;
  std::vector<int>::iterator id;
  for (id = routing_ids_.begin(); id != routing_ids_.end(); ++id, ++pos) {
    RenderView* render_view = RenderView::FromRoutingID(*id);
    if (!render_view) {
      NOTREACHED();
      continue;
    }
    if (render_view->GetWebView() == web_view)
      break;
  }

  if (id == routing_ids_.end()) {
    NOTREACHED();
    return std::string();
  }
  return DumpBackForwardList(session_histories_[pos],
                             current_entry_indexes_[pos]);
}

void BlinkTestRunner::FetchManifest(
      blink::WebView* view,
      const GURL& url,
      const base::Callback<void(const blink::WebURLResponse& response,
                                const std::string& data)>& callback) {
  ::content::FetchManifest(view, url, callback);
}

void BlinkTestRunner::SetPermission(const std::string& name,
                                    const std::string& value,
                                    const GURL& origin,
                                    const GURL& embedding_origin) {
  blink::mojom::PermissionStatus status;
  if (value == "granted") {
    status = blink::mojom::PermissionStatus::GRANTED;
  } else if (value == "prompt") {
    status = blink::mojom::PermissionStatus::ASK;
  } else if (value == "denied") {
    status = blink::mojom::PermissionStatus::DENIED;
  } else {
    NOTREACHED();
    status = blink::mojom::PermissionStatus::DENIED;
  }

  Send(new LayoutTestHostMsg_SetPermission(
      routing_id(), name, status, origin, embedding_origin));
}

void BlinkTestRunner::ResetPermissions() {
  Send(new LayoutTestHostMsg_ResetPermissions(routing_id()));
}

cc::SharedBitmapManager* BlinkTestRunner::GetSharedBitmapManager() {
  return RenderThread::Get()->GetSharedBitmapManager();
}

void BlinkTestRunner::DispatchBeforeInstallPromptEvent(
    const std::vector<std::string>& event_platforms,
    const base::Callback<void(bool)>& callback) {
  app_banner_service_.reset(new test_runner::AppBannerService());

  service_manager::BinderRegistry* registry =
      render_view()->GetMainRenderFrame()->GetInterfaceRegistry();
  blink::mojom::AppBannerControllerRequest request =
      mojo::MakeRequest(&app_banner_service_->controller());
  registry->BindInterface(service_manager::BindSourceInfo(),
                          blink::mojom::AppBannerController::Name_,
                          request.PassMessagePipe());
  app_banner_service_->SendBannerPromptRequest(event_platforms, callback);
}

void BlinkTestRunner::ResolveBeforeInstallPromptPromise(
    const std::string& platform) {
  if (app_banner_service_) {
    app_banner_service_->ResolvePromise(platform);
    app_banner_service_.reset(nullptr);
  }
}

blink::WebPlugin* BlinkTestRunner::CreatePluginPlaceholder(
    const blink::WebPluginParams& params) {
  if (params.mime_type != "application/x-plugin-placeholder-test")
    return nullptr;

  plugins::PluginPlaceholder* placeholder = new plugins::PluginPlaceholder(
      render_view()->GetMainRenderFrame(), params, "<div>Test content</div>");
  return placeholder->plugin();
}

float BlinkTestRunner::GetDeviceScaleFactor() const {
  return render_view()->GetDeviceScaleFactor();
}

void BlinkTestRunner::RunIdleTasks(const base::Closure& callback) {
    SchedulerRunIdleTasks(callback);
}

void BlinkTestRunner::ForceTextInputStateUpdate(WebLocalFrame* frame) {
  ForceTextInputStateUpdateForRenderFrame(RenderFrame::FromWebFrame(frame));
}

bool BlinkTestRunner::IsNavigationInitiatedByRenderer(
    const WebURLRequest& request) {
  return content::IsNavigationInitiatedByRenderer(request);
}

bool BlinkTestRunner::AddMediaStreamVideoSourceAndTrack(
    blink::WebMediaStream* stream) {
  DCHECK(stream);
#if BUILDFLAG(ENABLE_WEBRTC)
  return AddVideoTrackToMediaStream(base::MakeUnique<MockVideoCapturerSource>(),
                                    false,  // is_remote
                                    stream);
#else
  return false;
#endif
}

bool BlinkTestRunner::AddMediaStreamAudioSourceAndTrack(
    blink::WebMediaStream* stream) {
  DCHECK(stream);
#if BUILDFLAG(ENABLE_WEBRTC)
  return AddAudioTrackToMediaStream(
      make_scoped_refptr(new MockAudioCapturerSource()),
      48000,  // sample rate
      media::CHANNEL_LAYOUT_STEREO,
      480,    // sample frames per buffer
      false,  // is_remote
      stream);
#else
  return false;
#endif
}

// RenderViewObserver  --------------------------------------------------------

void BlinkTestRunner::DidClearWindowObject(WebLocalFrame* frame) {
  WebTestingSupport::InjectInternalsObject(frame);
}

bool BlinkTestRunner::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BlinkTestRunner, message)
    IPC_MESSAGE_HANDLER(ShellViewMsg_SessionHistory, OnSessionHistory)
    IPC_MESSAGE_HANDLER(ShellViewMsg_Reset, OnReset)
    IPC_MESSAGE_HANDLER(ShellViewMsg_TestFinishedInSecondaryRenderer,
                        OnTestFinishedInSecondaryRenderer)
    IPC_MESSAGE_HANDLER(ShellViewMsg_TryLeakDetection, OnTryLeakDetection)
    IPC_MESSAGE_HANDLER(ShellViewMsg_ReplyBluetoothManualChooserEvents,
                        OnReplyBluetoothManualChooserEvents)
    IPC_MESSAGE_HANDLER(ShellViewMsg_LayoutDumpCompleted, OnLayoutDumpCompleted)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void BlinkTestRunner::Navigate(const GURL& url) {
  focus_on_next_commit_ = true;
}

void BlinkTestRunner::DidCommitProvisionalLoad(WebLocalFrame* frame,
                                               bool is_new_navigation) {
  if (!focus_on_next_commit_)
    return;
  focus_on_next_commit_ = false;
  render_view()->GetWebView()->SetFocusedFrame(frame);
}

void BlinkTestRunner::DidFailProvisionalLoad(WebLocalFrame* frame,
                                             const WebURLError& error) {
  focus_on_next_commit_ = false;
}

// Public methods - -----------------------------------------------------------

void BlinkTestRunner::Reset(bool for_new_test) {
  prefs_.Reset();
  routing_ids_.clear();
  session_histories_.clear();
  current_entry_indexes_.clear();

  render_view()->ClearEditCommands();
  if (for_new_test) {
    if (render_view()->GetWebView()->MainFrame()->IsWebLocalFrame())
      render_view()->GetWebView()->MainFrame()->SetName(WebString());
    render_view()->GetWebView()->MainFrame()->ClearOpener();
  }

  // Resetting the internals object also overrides the WebPreferences, so we
  // have to sync them to WebKit again.
  if (render_view()->GetWebView()->MainFrame()->IsWebLocalFrame()) {
    WebTestingSupport::ResetInternalsObject(
        render_view()->GetWebView()->MainFrame()->ToWebLocalFrame());
    render_view()->SetWebkitPreferences(render_view()->GetWebkitPreferences());
  }
}

// Private methods  -----------------------------------------------------------

void BlinkTestRunner::CaptureDump() {
  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderThreadObserver::GetInstance()->test_interfaces();
  TRACE_EVENT0("shell", "BlinkTestRunner::CaptureDump");

  if (interfaces->TestRunner()->ShouldDumpAsAudio()) {
    std::vector<unsigned char> vector_data;
    interfaces->TestRunner()->GetAudioData(&vector_data);
    Send(new ShellViewHostMsg_AudioDump(routing_id(), vector_data));
    CaptureDumpContinued();
    return;
  }

  std::string custom_text_dump;
  if (interfaces->TestRunner()->HasCustomTextDump(&custom_text_dump)) {
    Send(new ShellViewHostMsg_TextDump(routing_id(), custom_text_dump + "\n"));
    CaptureDumpContinued();
    return;
  }

  if (!interfaces->TestRunner()->IsRecursiveLayoutDumpRequested()) {
    std::string layout_dump = interfaces->TestRunner()->DumpLayout(
        render_view()->GetMainRenderFrame()->GetWebFrame());
    OnLayoutDumpCompleted(std::move(layout_dump));
    return;
  }

  Send(new ShellViewHostMsg_InitiateLayoutDump(routing_id()));
  // OnLayoutDumpCompleted will be eventually called by an IPC from the browser.
}

void BlinkTestRunner::OnLayoutDumpCompleted(std::string completed_layout_dump) {
  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderThreadObserver::GetInstance()->test_interfaces();
  if (interfaces->TestRunner()->ShouldDumpBackForwardList()) {
    for (WebView* web_view : interfaces->GetWindowList())
      completed_layout_dump.append(DumpHistoryForWindow(web_view));
  }

  Send(new ShellViewHostMsg_TextDump(routing_id(),
                                     std::move(completed_layout_dump)));

  CaptureDumpContinued();
}

void BlinkTestRunner::CaptureDumpContinued() {
  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderThreadObserver::GetInstance()->test_interfaces();
  if (test_config_->enable_pixel_dumping &&
      interfaces->TestRunner()->ShouldGeneratePixelResults() &&
      !interfaces->TestRunner()->ShouldDumpAsAudio()) {
    CHECK(render_view()->GetWebView()->IsAcceleratedCompositingActive());
    interfaces->TestRunner()->DumpPixelsAsync(
        render_view()->GetWebView(),
        base::Bind(&BlinkTestRunner::OnPixelsDumpCompleted,
                   base::Unretained(this)));
    return;
  }

  CaptureDumpComplete();
}

void BlinkTestRunner::OnPixelsDumpCompleted(const SkBitmap& snapshot) {
  DCHECK_NE(0, snapshot.info().width());
  DCHECK_NE(0, snapshot.info().height());

  // The snapshot arrives from the GPU process via shared memory. Because MSan
  // can't track initializedness across processes, we must assure it that the
  // pixels are in fact initialized.
  MSAN_UNPOISON(snapshot.getPixels(), snapshot.getSize());
  base::MD5Digest digest;
  base::MD5Sum(snapshot.getPixels(), snapshot.getSize(), &digest);
  std::string actual_pixel_hash = base::MD5DigestToBase16(digest);

  if (actual_pixel_hash == test_config_->expected_pixel_hash) {
    SkBitmap empty_image;
    Send(new ShellViewHostMsg_ImageDump(
        routing_id(), actual_pixel_hash, empty_image));
  } else {
    Send(new ShellViewHostMsg_ImageDump(
        routing_id(), actual_pixel_hash, snapshot));
  }

  CaptureDumpComplete();
}

void BlinkTestRunner::CaptureDumpComplete() {
  render_view()->GetWebView()->MainFrame()->StopLoading();

  Send(new ShellViewHostMsg_TestFinished(routing_id()));
}

mojom::LayoutTestBluetoothFakeAdapterSetter&
BlinkTestRunner::GetBluetoothFakeAdapterSetter() {
  if (!bluetooth_fake_adapter_setter_) {
    RenderThread::Get()->GetConnector()->BindInterface(
        mojom::kBrowserServiceName,
        mojo::MakeRequest(&bluetooth_fake_adapter_setter_));
  }
  return *bluetooth_fake_adapter_setter_;
}

void BlinkTestRunner::OnSetupSecondaryRenderer() {
  DCHECK(!is_main_window_);

  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderThreadObserver::GetInstance()->test_interfaces();
  interfaces->SetTestIsRunning(true);
  interfaces->ConfigureForTestWithURL(GURL(), false);
  ForceResizeRenderView(render_view(), WebSize(800, 600));
}

void BlinkTestRunner::OnReplicateTestConfiguration(
    mojom::ShellTestConfigurationPtr params) {
  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderThreadObserver::GetInstance()->test_interfaces();

  test_config_ = params.Clone();

  is_main_window_ = true;
  interfaces->SetMainView(render_view()->GetWebView());

  interfaces->SetTestIsRunning(true);
  interfaces->ConfigureForTestWithURL(params->test_url,
                                      params->enable_pixel_dumping);
}

void BlinkTestRunner::OnSetTestConfiguration(
    mojom::ShellTestConfigurationPtr params) {
  mojom::ShellTestConfigurationPtr local_params = params.Clone();
  OnReplicateTestConfiguration(std::move(params));

  ForceResizeRenderView(render_view(),
                        WebSize(local_params->initial_size.width(),
                                local_params->initial_size.height()));
  LayoutTestRenderThreadObserver::GetInstance()
      ->test_interfaces()
      ->TestRunner()
      ->SetFocus(render_view()->GetWebView(), true);
}

void BlinkTestRunner::OnSessionHistory(
    const std::vector<int>& routing_ids,
    const std::vector<std::vector<PageState>>& session_histories,
    const std::vector<unsigned>& current_entry_indexes) {
  routing_ids_ = routing_ids;
  session_histories_ = session_histories;
  current_entry_indexes_ = current_entry_indexes;
  CaptureDump();
}

void BlinkTestRunner::OnReset() {
  // ShellViewMsg_Reset should always be sent to the *current* view.
  DCHECK(render_view()->GetWebView()->MainFrame()->IsWebLocalFrame());
  WebLocalFrame* main_frame =
      render_view()->GetWebView()->MainFrame()->ToWebLocalFrame();

  LayoutTestRenderThreadObserver::GetInstance()->test_interfaces()->ResetAll();
  Reset(true /* for_new_test */);
  // Navigating to about:blank will make sure that no new loads are initiated
  // by the renderer.
  main_frame->LoadRequest(WebURLRequest(GURL(url::kAboutBlankURL)));
  Send(new ShellViewHostMsg_ResetDone(routing_id()));
}

void BlinkTestRunner::OnTestFinishedInSecondaryRenderer() {
  DCHECK(is_main_window_ && render_view()->GetMainRenderFrame());

  // Avoid a situation where TestFinished is called twice, because
  // of a racey test finish in 2 secondary renderers.
  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderThreadObserver::GetInstance()->test_interfaces();
  if (!interfaces->TestIsRunning())
    return;

  TestFinished();
}

void BlinkTestRunner::OnTryLeakDetection() {
  blink::WebFrame* main_frame = render_view()->GetWebView()->MainFrame();
  DCHECK_EQ(GURL(url::kAboutBlankURL), GURL(main_frame->GetDocument().Url()));
  DCHECK(!main_frame->IsLoading());

  leak_detector_->TryLeakDetection(main_frame);
}

void BlinkTestRunner::OnReplyBluetoothManualChooserEvents(
    const std::vector<std::string>& events) {
  DCHECK(!get_bluetooth_events_callbacks_.empty());
  base::Callback<void(const std::vector<std::string>&)> callback =
      get_bluetooth_events_callbacks_.front();
  get_bluetooth_events_callbacks_.pop_front();
  callback.Run(events);
}

void BlinkTestRunner::ReportLeakDetectionResult(
    const LeakDetectionResult& report) {
  Send(new ShellViewHostMsg_LeakDetectionDone(routing_id(), report));
}

void BlinkTestRunner::OnDestruct() {
  delete this;
}

}  // namespace content
