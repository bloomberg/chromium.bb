// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/layout_test/blink_test_runner.h"

#include <stddef.h>
#include <algorithm>
#include <clocale>
#include <cmath>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/debugger.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/plugins/renderer/plugin_placeholder.h"
#include "components/test_runner/app_banner_client.h"
#include "components/test_runner/gamepad_controller.h"
#include "components/test_runner/layout_dump.h"
#include "components/test_runner/layout_dump_flags.h"
#include "components/test_runner/mock_screen_orientation_client.h"
#include "components/test_runner/test_interfaces.h"
#include "components/test_runner/web_task.h"
#include "components/test_runner/web_test_interfaces.h"
#include "components/test_runner/web_test_proxy.h"
#include "components/test_runner/web_test_runner.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "content/public/renderer/media_stream_api.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/public/renderer/renderer_gamepad_provider.h"
#include "content/public/test/layouttest_support.h"
#include "content/shell/common/layout_test/layout_test_messages.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/layout_test/blink_test_helpers.h"
#include "content/shell/renderer/layout_test/layout_test_render_process_observer.h"
#include "content/shell/renderer/layout_test/leak_detector.h"
#include "media/audio/audio_parameters.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebTaskRunner.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/platform/modules/app_banner/WebAppBannerPromptReply.h"
#include "third_party/WebKit/public/web/WebArrayBufferView.h"
#include "third_party/WebKit/public/web/WebContextMenuData.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDevToolsAgent.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLeakDetector.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebTestingSupport.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/geometry/rect.h"

using blink::Platform;
using blink::WebArrayBufferView;
using blink::WebContextMenuData;
using blink::WebDevToolsAgent;
using blink::WebDeviceMotionData;
using blink::WebDeviceOrientationData;
using blink::WebElement;
using blink::WebLocalFrame;
using blink::WebHistoryItem;
using blink::WebLocalFrame;
using blink::WebPoint;
using blink::WebRect;
using blink::WebScriptSource;
using blink::WebSize;
using blink::WebString;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebScreenOrientationType;
using blink::WebTestingSupport;
using blink::WebTraceLocation;
using blink::WebThread;
using blink::WebVector;
using blink::WebView;

namespace content {

namespace {

class InvokeTaskHelper : public blink::WebTaskRunner::Task {
 public:
  InvokeTaskHelper(scoped_ptr<test_runner::WebTask> task)
      : task_(std::move(task)) {}

  // WebThread::Task implementation:
  void run() override { task_->run(); }

 private:
  scoped_ptr<test_runner::WebTask> task_;
};

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

class ProxyToRenderViewVisitor : public RenderViewVisitor {
 public:
  explicit ProxyToRenderViewVisitor(test_runner::WebTestProxyBase* proxy)
      : proxy_(proxy),
        render_view_(NULL) {
  }
  ~ProxyToRenderViewVisitor() override {}

  RenderView* render_view() const { return render_view_; }

  bool Visit(RenderView* render_view) override {
    BlinkTestRunner* test_runner = BlinkTestRunner::Get(render_view);
    if (!test_runner) {
      NOTREACHED();
      return true;
    }
    if (test_runner->proxy() == proxy_) {
      render_view_ = render_view;
      return false;
    }
    return true;
  }

 private:
  test_runner::WebTestProxyBase* proxy_;
  RenderView* render_view_;

  DISALLOW_COPY_AND_ASSIGN(ProxyToRenderViewVisitor);
};

class NavigateAwayVisitor : public RenderViewVisitor {
 public:
  explicit NavigateAwayVisitor(RenderView* main_render_view)
      : main_render_view_(main_render_view) {}
  ~NavigateAwayVisitor() override {}

  bool Visit(RenderView* render_view) override {
    if (render_view == main_render_view_)
      return true;
    render_view->GetWebView()->mainFrame()->loadRequest(
        WebURLRequest(GURL(url::kAboutBlankURL)));
    return true;
  }

 private:
  RenderView* main_render_view_;

  DISALLOW_COPY_AND_ASSIGN(NavigateAwayVisitor);
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
  void SampleGamepads(blink::WebGamepads& gamepads) override {
    controller_->SampleGamepads(gamepads);
  }
  void Start(blink::WebPlatformEventListener* listener) override {
    controller_->SetListener(static_cast<blink::WebGamepadListener*>(listener));
    RendererGamepadProvider::Start(listener);
  }
  void SendStartMessage() override {}
  void SendStopMessage() override {}

 private:
  scoped_ptr<test_runner::GamepadController> controller_;

  DISALLOW_COPY_AND_ASSIGN(MockGamepadProvider);
};

class MockVideoCapturerSource : public media::VideoCapturerSource {
 public:
  MockVideoCapturerSource() = default;
  ~MockVideoCapturerSource() override {}

  void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height,
      double max_requested_frame_rate,
      const VideoCaptureDeviceFormatsCB& callback) override {}
  void StartCapture(
      const media::VideoCaptureParams& params,
      const VideoCaptureDeliverFrameCB& new_frame_callback,
      const RunningCallback& running_callback) override {}
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

}  // namespace

BlinkTestRunner::BlinkTestRunner(RenderView* render_view)
    : RenderViewObserver(render_view),
      RenderViewObserverTracker<BlinkTestRunner>(render_view),
      proxy_(NULL),
      focused_view_(NULL),
      is_main_window_(false),
      focus_on_next_commit_(false),
      leak_detector_(new LeakDetector(this)) {
}

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
  scoped_ptr<MockGamepadProvider> provider(new MockGamepadProvider(controller));
  SetMockGamepadProvider(std::move(provider));
}

void BlinkTestRunner::SetDeviceLightData(const double data) {
  SetMockDeviceLightData(data);
}

void BlinkTestRunner::SetDeviceMotionData(const WebDeviceMotionData& data) {
  SetMockDeviceMotionData(data);
}

void BlinkTestRunner::SetDeviceOrientationData(
    const WebDeviceOrientationData& data) {
  SetMockDeviceOrientationData(data);
}

void BlinkTestRunner::SetScreenOrientation(
    const WebScreenOrientationType& orientation) {
  test_runner::MockScreenOrientationClient* mock_client =
      proxy()->GetScreenOrientationClientMock();
  mock_client->UpdateDeviceOrientation(
      render_view()->GetWebView()->mainFrame()->toWebLocalFrame(), orientation);
}

void BlinkTestRunner::ResetScreenOrientation() {
  test_runner::MockScreenOrientationClient* mock_client =
      proxy()->GetScreenOrientationClientMock();
  mock_client->ResetData();
}

void BlinkTestRunner::DidChangeBatteryStatus(
    const blink::WebBatteryStatus& status) {
  MockBatteryStatusChanged(status);
}

void BlinkTestRunner::PrintMessage(const std::string& message) {
  Send(new ShellViewHostMsg_PrintMessage(routing_id(), message));
}

void BlinkTestRunner::PostTask(test_runner::WebTask* task) {
  Platform::current()->currentThread()->taskRunner()->postTask(
      WebTraceLocation(__FUNCTION__, __FILE__),
      new InvokeTaskHelper(make_scoped_ptr(task)));
}

void BlinkTestRunner::PostDelayedTask(test_runner::WebTask* task,
                                      long long ms) {
  Platform::current()->currentThread()->taskRunner()->postDelayedTask(
      WebTraceLocation(__FUNCTION__, __FILE__),
      new InvokeTaskHelper(make_scoped_ptr(task)), ms);
}

WebString BlinkTestRunner::RegisterIsolatedFileSystem(
    const blink::WebVector<blink::WebString>& absolute_filenames) {
  std::vector<base::FilePath> files;
  for (size_t i = 0; i < absolute_filenames.size(); ++i)
    files.push_back(base::FilePath::FromUTF16Unsafe(absolute_filenames[i]));
  std::string filesystem_id;
  Send(new LayoutTestHostMsg_RegisterIsolatedFileSystem(
      routing_id(), files, &filesystem_id));
  return WebString::fromUTF8(filesystem_id);
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
        net::FilePathToFileURL(test_config_.current_working_directory.Append(
            FILE_PATH_LITERAL("foo")));
    net::FileURLToFilePath(base_url.Resolve(utf8_path), &path);
  }
  return path.AsUTF16Unsafe();
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

WebURL BlinkTestRunner::RewriteLayoutTestsURL(const std::string& utf8_url) {
  const char kPrefix[] = "file:///tmp/LayoutTests/";
  const int kPrefixLen = arraysize(kPrefix) - 1;

  if (utf8_url.compare(0, kPrefixLen, kPrefix, kPrefixLen))
    return WebURL(GURL(utf8_url));

  base::FilePath replace_path =
      LayoutTestRenderProcessObserver::GetInstance()->webkit_source_dir()
          .Append(FILE_PATH_LITERAL("LayoutTests/"));
#if defined(OS_WIN)
  std::string utf8_path = base::WideToUTF8(replace_path.value());
#else
  std::string utf8_path =
      base::WideToUTF8(base::SysNativeMBToWide(replace_path.value()));
#endif
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

std::string BlinkTestRunner::makeURLErrorDescription(const WebURLError& error) {
  std::string domain = error.domain.utf8();
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
      domain.c_str(), code, error.unreachableURL.string().utf8().data());
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
  if (!new_size.isEmpty())
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
  WebDevToolsAgent* agent =
      render_view()->GetMainRenderFrame()->GetWebFrame()->devToolsAgent();
  if (agent)
    agent->detach();
}

void BlinkTestRunner::EvaluateInWebInspector(long call_id,
                                             const std::string& script) {
  WebDevToolsAgent* agent =
      render_view()->GetMainRenderFrame()->GetWebFrame()->devToolsAgent();
  if (agent)
    agent->evaluateInWebInspector(call_id, WebString::fromUTF8(script));
}

std::string BlinkTestRunner::EvaluateInWebInspectorOverlay(
    const std::string& script) {
  WebDevToolsAgent* agent =
      render_view()->GetMainRenderFrame()->GetWebFrame()->devToolsAgent();
  if (!agent)
    return std::string();

  return agent->evaluateInWebInspectorOverlay(
      WebString::fromUTF8(script)).utf8();
}

void BlinkTestRunner::ClearAllDatabases() {
  Send(new LayoutTestHostMsg_ClearAllDatabases(routing_id()));
}

void BlinkTestRunner::SetDatabaseQuota(int quota) {
  Send(new LayoutTestHostMsg_SetDatabaseQuota(routing_id(), quota));
}

void BlinkTestRunner::SimulateWebNotificationClick(const std::string& title,
                                                   int action_index) {
  Send(new LayoutTestHostMsg_SimulateWebNotificationClick(routing_id(), title,
                                                          action_index));
}

void BlinkTestRunner::SetDeviceScaleFactor(float factor) {
  content::SetDeviceScaleFactor(render_view(), factor);
}

void BlinkTestRunner::EnableUseZoomForDSF() {
  base::CommandLine::ForCurrentProcess()->
      AppendSwitch(switches::kEnableUseZoomForDSF);
}

void BlinkTestRunner::SetDeviceColorProfile(const std::string& name) {
  content::SetDeviceColorProfile(render_view(), name);
}

void BlinkTestRunner::SetBluetoothMockDataSet(const std::string& name) {
  Send(new LayoutTestHostMsg_SetBluetoothAdapter(name));
  // Auto-reset the chooser type so we don't get order dependence when some
  // tests forget to do it explicitly.
  Send(new ShellViewHostMsg_SetBluetoothManualChooser(routing_id(), false));
}

void BlinkTestRunner::SetBluetoothManualChooser() {
  Send(new ShellViewHostMsg_SetBluetoothManualChooser(routing_id(), true));
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

void BlinkTestRunner::SetGeofencingMockProvider(bool service_available) {
  content::SetGeofencingMockProvider(service_available);
}

void BlinkTestRunner::ClearGeofencingMockProvider() {
  content::ClearGeofencingMockProvider();
}

void BlinkTestRunner::SetGeofencingMockPosition(double latitude,
                                                double longitude) {
  content::SetGeofencingMockPosition(latitude, longitude);
}

void BlinkTestRunner::SetFocus(test_runner::WebTestProxyBase* proxy,
                               bool focus) {
  ProxyToRenderViewVisitor visitor(proxy);
  RenderView::ForEach(&visitor);
  if (!visitor.render_view()) {
    NOTREACHED();
    return;
  }

  // Check whether the focused view was closed meanwhile.
  if (!BlinkTestRunner::Get(focused_view_))
    focused_view_ = NULL;

  if (focus) {
    if (focused_view_ != visitor.render_view()) {
      if (focused_view_)
        SetFocusAndActivate(focused_view_, false);
      SetFocusAndActivate(visitor.render_view(), true);
      focused_view_ = visitor.render_view();
    }
  } else {
    if (focused_view_ == visitor.render_view()) {
      SetFocusAndActivate(visitor.render_view(), false);
      focused_view_ = NULL;
    }
  }
}

void BlinkTestRunner::SetAcceptAllCookies(bool accept) {
  Send(new LayoutTestHostMsg_AcceptAllCookies(routing_id(), accept));
}

std::string BlinkTestRunner::PathToLocalResource(const std::string& resource) {
#if defined(OS_WIN)
  if (resource.find("/tmp/") == 0) {
    // We want a temp file.
    GURL base_url = net::FilePathToFileURL(test_config_.temp_path);
    return base_url.Resolve(resource.substr(strlen("/tmp/"))).spec();
  }
#endif

  // Some layout tests use file://// which we resolve as a UNC path. Normalize
  // them to just file:///.
  std::string result = resource;
  while (base::ToLowerASCII(result).find("file:////") == 0) {
    result = result.substr(0, strlen("file:///")) +
             result.substr(strlen("file:////"));
  }
  return RewriteLayoutTestsURL(result).string().utf8();
}

void BlinkTestRunner::SetLocale(const std::string& locale) {
  setlocale(LC_ALL, locale.c_str());
}

void BlinkTestRunner::TestFinished() {
  if (!is_main_window_) {
    Send(new ShellViewHostMsg_TestFinishedInSecondaryWindow(routing_id()));
    return;
  }
  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderProcessObserver::GetInstance()->test_interfaces();
  interfaces->SetTestIsRunning(false);
  if (interfaces->TestRunner()->ShouldDumpBackForwardList()) {
    SyncNavigationStateVisitor visitor;
    RenderView::ForEach(&visitor);
    Send(new ShellViewHostMsg_CaptureSessionHistory(routing_id()));
  } else {
    CaptureDump();
  }
}

void BlinkTestRunner::CloseRemainingWindows() {
  NavigateAwayVisitor visitor(render_view());
  RenderView::ForEach(&visitor);
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
  return test_config_.allow_external_pages;
}

std::string BlinkTestRunner::DumpHistoryForWindow(
    test_runner::WebTestProxyBase* proxy) {
  size_t pos = 0;
  std::vector<int>::iterator id;
  for (id = routing_ids_.begin(); id != routing_ids_.end(); ++id, ++pos) {
    RenderView* render_view = RenderView::FromRoutingID(*id);
    if (!render_view) {
      NOTREACHED();
      continue;
    }
    if (BlinkTestRunner::Get(render_view)->proxy() == proxy)
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
  content::PermissionStatus status;
  if (value == "granted")
    status = PermissionStatus::GRANTED;
  else if (value == "prompt")
    status = PermissionStatus::ASK;
  else if (value == "denied")
    status = PermissionStatus::DENIED;
  else {
    NOTREACHED();
    status = PermissionStatus::DENIED;
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
    int request_id,
    const std::vector<std::string>& event_platforms,
    const base::Callback<void(bool)>& callback) {
  // Send the event to the frame.
  blink::WebAppBannerPromptReply reply;
  std::vector<blink::WebString> blink_web_strings;
  for (const auto& platform : event_platforms)
    blink_web_strings.push_back(blink::WebString::fromUTF8(platform));
  blink::WebVector<blink::WebString> blink_event_platforms(blink_web_strings);

  WebLocalFrame* main_frame =
      render_view()->GetWebView()->mainFrame()->toWebLocalFrame();
  main_frame->willShowInstallBannerPrompt(request_id, blink_event_platforms,
                                          &reply);

  callback.Run(reply == blink::WebAppBannerPromptReply::Cancel);
}

void BlinkTestRunner::ResolveBeforeInstallPromptPromise(
      int request_id, const std::string& platform) {
  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderProcessObserver::GetInstance()->test_interfaces();
  if (interfaces->GetAppBannerClient())
    interfaces->GetAppBannerClient()->ResolvePromise(request_id, platform);
}

blink::WebPlugin* BlinkTestRunner::CreatePluginPlaceholder(
    blink::WebLocalFrame* frame, const blink::WebPluginParams& params) {
  if (params.mimeType != "application/x-plugin-placeholder-test")
    return nullptr;

  plugins::PluginPlaceholder* placeholder =
      new plugins::PluginPlaceholder(render_view()->GetMainRenderFrame(), frame,
                                     params, "<div>Test content</div>");
  return placeholder->plugin();
}

void BlinkTestRunner::OnWebTestProxyBaseDestroy(
    test_runner::WebTestProxyBase* proxy) {
}

blink::WebPoint BlinkTestRunner::ConvertDIPToNative(
    const blink::WebPoint& point_in_dip) const {
  float scale = render_view()->GetDeviceScaleFactorForTest();
  return blink::WebPoint(point_in_dip.x * scale,
                         point_in_dip.y * scale);
}

bool BlinkTestRunner::AddMediaStreamVideoSourceAndTrack(
    blink::WebMediaStream* stream) {
  DCHECK(stream);
#if defined(ENABLE_WEBRTC)
  return AddVideoTrackToMediaStream(
      make_scoped_ptr(new MockVideoCapturerSource()),
      false /* is_remote */, false /* is_readonly */, stream);
#else
  return false;
#endif
}

bool BlinkTestRunner::AddMediaStreamAudioSourceAndTrack(
    blink::WebMediaStream* stream) {
  DCHECK(stream);
#if defined(ENABLE_WEBRTC)
  return AddAudioTrackToMediaStream(
      make_scoped_refptr(new MockAudioCapturerSource()),
      false /* is_remote */, false /* is_readonly */, stream);
#else
  return false;
#endif
}

// RenderViewObserver  --------------------------------------------------------

void BlinkTestRunner::DidClearWindowObject(WebLocalFrame* frame) {
  WebTestingSupport::injectInternalsObject(frame);
  LayoutTestRenderProcessObserver::GetInstance()->test_interfaces()->BindTo(
      frame);
}

bool BlinkTestRunner::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BlinkTestRunner, message)
    IPC_MESSAGE_HANDLER(ShellViewMsg_SetTestConfiguration,
                        OnSetTestConfiguration)
    IPC_MESSAGE_HANDLER(ShellViewMsg_SessionHistory, OnSessionHistory)
    IPC_MESSAGE_HANDLER(ShellViewMsg_Reset, OnReset)
    IPC_MESSAGE_HANDLER(ShellViewMsg_NotifyDone, OnNotifyDone)
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
  if (!is_main_window_ &&
      LayoutTestRenderProcessObserver::GetInstance()->main_test_runner() ==
          this) {
    test_runner::WebTestInterfaces* interfaces =
        LayoutTestRenderProcessObserver::GetInstance()->test_interfaces();
    interfaces->SetTestIsRunning(true);
    interfaces->ConfigureForTestWithURL(GURL(), false);
    ForceResizeRenderView(render_view(), WebSize(800, 600));
  }
}

void BlinkTestRunner::DidCommitProvisionalLoad(WebLocalFrame* frame,
                                               bool is_new_navigation) {
  if (!focus_on_next_commit_)
    return;
  focus_on_next_commit_ = false;
  render_view()->GetWebView()->setFocusedFrame(frame);
}

void BlinkTestRunner::DidFailProvisionalLoad(WebLocalFrame* frame,
                                             const WebURLError& error) {
  focus_on_next_commit_ = false;
}

// Public methods - -----------------------------------------------------------

void BlinkTestRunner::Reset() {
  // The proxy_ is always non-NULL, it is set right after construction.
  proxy_->set_widget(render_view()->GetWebView());
  proxy_->Reset();
  prefs_.Reset();
  routing_ids_.clear();
  session_histories_.clear();
  current_entry_indexes_.clear();

  render_view()->ClearEditCommands();
  if (render_view()->GetWebView()->mainFrame()->isWebLocalFrame())
    render_view()->GetWebView()->mainFrame()->setName(WebString());
  render_view()->GetWebView()->mainFrame()->clearOpener();

  // Resetting the internals object also overrides the WebPreferences, so we
  // have to sync them to WebKit again.
  if (render_view()->GetWebView()->mainFrame()->isWebLocalFrame()) {
    WebTestingSupport::resetInternalsObject(
        render_view()->GetWebView()->mainFrame()->toWebLocalFrame());
    render_view()->SetWebkitPreferences(render_view()->GetWebkitPreferences());
  }
}

// Private methods  -----------------------------------------------------------

void BlinkTestRunner::CaptureDump() {
  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderProcessObserver::GetInstance()->test_interfaces();
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

  test_runner::LayoutDumpFlags layout_dump_flags =
      interfaces->TestRunner()->GetLayoutDumpFlags();
  layout_dump_flags.dump_line_box_trees =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDumpLineBoxTrees);

  if (!layout_dump_flags.dump_child_frames) {
    std::string layout_dump = DumpLayout(
        render_view()->GetMainRenderFrame()->GetWebFrame(), layout_dump_flags);
    OnLayoutDumpCompleted(layout_dump);
    return;
  }

  Send(
      new ShellViewHostMsg_InitiateLayoutDump(routing_id(), layout_dump_flags));
  // OnLayoutDumpCompleted will be eventually called by an IPC from the browser.
}

void BlinkTestRunner::OnLayoutDumpCompleted(std::string completed_layout_dump) {
  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderProcessObserver::GetInstance()->test_interfaces();
  if (interfaces->TestRunner()->ShouldDumpBackForwardList()) {
    completed_layout_dump.append(proxy()->DumpBackForwardLists());
  }

  Send(new ShellViewHostMsg_TextDump(routing_id(), completed_layout_dump));

  CaptureDumpContinued();
}

void BlinkTestRunner::CaptureDumpContinued() {
  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderProcessObserver::GetInstance()->test_interfaces();
  if (test_config_.enable_pixel_dumping &&
      interfaces->TestRunner()->ShouldGeneratePixelResults() &&
      !interfaces->TestRunner()->ShouldDumpAsAudio()) {
    CHECK(render_view()->GetWebView()->isAcceleratedCompositingActive());
    proxy()->CapturePixelsAsync(base::Bind(
        &BlinkTestRunner::OnPixelsDumpCompleted, base::Unretained(this)));
    return;
  }

#ifndef NDEBUG
  // Force a layout/paint by the end of the test to ensure test coverage of
  // incremental painting.
  proxy()->LayoutAndPaintAsyncThen(base::Bind(
      &BlinkTestRunner::CaptureDumpComplete, base::Unretained(this)));
#else
  CaptureDumpComplete();
#endif
}

void BlinkTestRunner::OnPixelsDumpCompleted(const SkBitmap& snapshot) {
  DCHECK_NE(0, snapshot.info().width());
  DCHECK_NE(0, snapshot.info().height());

  SkAutoLockPixels snapshot_lock(snapshot);
  // The snapshot arrives from the GPU process via shared memory. Because MSan
  // can't track initializedness across processes, we must assure it that the
  // pixels are in fact initialized.
  MSAN_UNPOISON(snapshot.getPixels(), snapshot.getSize());
  base::MD5Digest digest;
  base::MD5Sum(snapshot.getPixels(), snapshot.getSize(), &digest);
  std::string actual_pixel_hash = base::MD5DigestToBase16(digest);

  if (actual_pixel_hash == test_config_.expected_pixel_hash) {
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
  render_view()->GetWebView()->mainFrame()->stopLoading();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(base::IgnoreResult(&BlinkTestRunner::Send),
                            base::Unretained(this),
                            new ShellViewHostMsg_TestFinished(routing_id())));
}

void BlinkTestRunner::OnSetTestConfiguration(
    const ShellTestConfiguration& params) {
  test_config_ = params;
  is_main_window_ = true;

  ForceResizeRenderView(
      render_view(),
      WebSize(params.initial_size.width(), params.initial_size.height()));
  SetFocus(proxy_, true);

  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderProcessObserver::GetInstance()->test_interfaces();
  interfaces->SetTestIsRunning(true);
  interfaces->ConfigureForTestWithURL(params.test_url,
                                      params.enable_pixel_dumping);
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
  LayoutTestRenderProcessObserver::GetInstance()->test_interfaces()->ResetAll();
  Reset();
  // Navigating to about:blank will make sure that no new loads are initiated
  // by the renderer.
  render_view()->GetWebView()->mainFrame()->loadRequest(
      WebURLRequest(GURL(url::kAboutBlankURL)));
  Send(new ShellViewHostMsg_ResetDone(routing_id()));
}

void BlinkTestRunner::OnNotifyDone() {
  render_view()->GetWebView()->mainFrame()->executeScript(
      WebScriptSource(WebString::fromUTF8("testRunner.notifyDone();")));
}

void BlinkTestRunner::OnTryLeakDetection() {
  WebLocalFrame* main_frame =
      render_view()->GetWebView()->mainFrame()->toWebLocalFrame();
  DCHECK_EQ(GURL(url::kAboutBlankURL), GURL(main_frame->document().url()));
  DCHECK(!main_frame->isLoading());

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

}  // namespace content
