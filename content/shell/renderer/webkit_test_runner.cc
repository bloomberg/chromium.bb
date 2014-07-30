// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/webkit_test_runner.h"

#include <algorithm>
#include <clocale>
#include <cmath>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/files/file_path.h"
#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/public/test/layouttest_support.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/common/webkit_test_helpers.h"
#include "content/shell/renderer/gc_controller.h"
#include "content/shell/renderer/leak_detector.h"
#include "content/shell/renderer/shell_render_process_observer.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "content/shell/renderer/test_runner/WebTestInterfaces.h"
#include "content/shell/renderer/test_runner/mock_screen_orientation_client.h"
#include "content/shell/renderer/test_runner/web_test_proxy.h"
#include "content/shell/renderer/test_runner/web_test_runner.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
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
#include "ui/gfx/rect.h"

using blink::Platform;
using blink::WebArrayBufferView;
using blink::WebContextMenuData;
using blink::WebDevToolsAgent;
using blink::WebDeviceMotionData;
using blink::WebDeviceOrientationData;
using blink::WebElement;
using blink::WebLocalFrame;
using blink::WebGamepads;
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
using blink::WebVector;
using blink::WebView;

namespace content {

namespace {

void InvokeTaskHelper(void* context) {
  WebTask* task = reinterpret_cast<WebTask*>(context);
  task->run();
  delete task;
}

class SyncNavigationStateVisitor : public RenderViewVisitor {
 public:
  SyncNavigationStateVisitor() {}
  virtual ~SyncNavigationStateVisitor() {}

  virtual bool Visit(RenderView* render_view) OVERRIDE {
    SyncNavigationState(render_view);
    return true;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(SyncNavigationStateVisitor);
};

class ProxyToRenderViewVisitor : public RenderViewVisitor {
 public:
  explicit ProxyToRenderViewVisitor(WebTestProxyBase* proxy)
      : proxy_(proxy),
        render_view_(NULL) {
  }
  virtual ~ProxyToRenderViewVisitor() {}

  RenderView* render_view() const { return render_view_; }

  virtual bool Visit(RenderView* render_view) OVERRIDE {
    WebKitTestRunner* test_runner = WebKitTestRunner::Get(render_view);
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
  WebTestProxyBase* proxy_;
  RenderView* render_view_;

  DISALLOW_COPY_AND_ASSIGN(ProxyToRenderViewVisitor);
};

class NavigateAwayVisitor : public RenderViewVisitor {
 public:
  explicit NavigateAwayVisitor(RenderView* main_render_view)
      : main_render_view_(main_render_view) {}
  virtual ~NavigateAwayVisitor() {}

  virtual bool Visit(RenderView* render_view) OVERRIDE {
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
  virtual ~UseSynchronousResizeModeVisitor() {}

  virtual bool Visit(RenderView* render_view) OVERRIDE {
    UseSynchronousResizeMode(render_view, enable_);
    return true;
  }

 private:
  bool enable_;
};

}  // namespace

WebKitTestRunner::WebKitTestRunner(RenderView* render_view)
    : RenderViewObserver(render_view),
      RenderViewObserverTracker<WebKitTestRunner>(render_view),
      proxy_(NULL),
      focused_view_(NULL),
      is_main_window_(false),
      focus_on_next_commit_(false),
      leak_detector_(new LeakDetector(this)) {
}

WebKitTestRunner::~WebKitTestRunner() {
}

// WebTestDelegate  -----------------------------------------------------------

void WebKitTestRunner::clearEditCommand() {
  render_view()->ClearEditCommands();
}

void WebKitTestRunner::setEditCommand(const std::string& name,
                                      const std::string& value) {
  render_view()->SetEditCommandForNextKeyEvent(name, value);
}

void WebKitTestRunner::setGamepadProvider(
    RendererGamepadProvider* provider) {
  SetMockGamepadProvider(provider);
}

void WebKitTestRunner::setDeviceLightData(const double data) {
  SetMockDeviceLightData(data);
}

void WebKitTestRunner::setDeviceMotionData(const WebDeviceMotionData& data) {
  SetMockDeviceMotionData(data);
}

void WebKitTestRunner::setDeviceOrientationData(
    const WebDeviceOrientationData& data) {
  SetMockDeviceOrientationData(data);
}

void WebKitTestRunner::setScreenOrientation(
    const WebScreenOrientationType& orientation) {
  MockScreenOrientationClient* mock_client =
      proxy()->GetScreenOrientationClientMock();
  mock_client->UpdateDeviceOrientation(render_view()->GetWebView()->mainFrame(),
                                       orientation);
}

void WebKitTestRunner::resetScreenOrientation() {
  MockScreenOrientationClient* mock_client =
      proxy()->GetScreenOrientationClientMock();
  mock_client->ResetData();
}

void WebKitTestRunner::didChangeBatteryStatus(
    const blink::WebBatteryStatus& status) {
  MockBatteryStatusChanged(status);
}

void WebKitTestRunner::printMessage(const std::string& message) {
  Send(new ShellViewHostMsg_PrintMessage(routing_id(), message));
}

void WebKitTestRunner::postTask(WebTask* task) {
  Platform::current()->callOnMainThread(InvokeTaskHelper, task);
}

void WebKitTestRunner::postDelayedTask(WebTask* task, long long ms) {
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WebTask::run, base::Owned(task)),
      base::TimeDelta::FromMilliseconds(ms));
}

WebString WebKitTestRunner::registerIsolatedFileSystem(
    const blink::WebVector<blink::WebString>& absolute_filenames) {
  std::vector<base::FilePath> files;
  for (size_t i = 0; i < absolute_filenames.size(); ++i)
    files.push_back(base::FilePath::FromUTF16Unsafe(absolute_filenames[i]));
  std::string filesystem_id;
  Send(new ShellViewHostMsg_RegisterIsolatedFileSystem(
      routing_id(), files, &filesystem_id));
  return WebString::fromUTF8(filesystem_id);
}

long long WebKitTestRunner::getCurrentTimeInMillisecond() {
  return base::TimeDelta(base::Time::Now() -
                         base::Time::UnixEpoch()).ToInternalValue() /
         base::Time::kMicrosecondsPerMillisecond;
}

WebString WebKitTestRunner::getAbsoluteWebStringFromUTF8Path(
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

WebURL WebKitTestRunner::localFileToDataURL(const WebURL& file_url) {
  base::FilePath local_path;
  if (!net::FileURLToFilePath(file_url, &local_path))
    return WebURL();

  std::string contents;
  Send(new ShellViewHostMsg_ReadFileToString(
        routing_id(), local_path, &contents));

  std::string contents_base64;
  base::Base64Encode(contents, &contents_base64);

  const char data_url_prefix[] = "data:text/css:charset=utf-8;base64,";
  return WebURL(GURL(data_url_prefix + contents_base64));
}

WebURL WebKitTestRunner::rewriteLayoutTestsURL(const std::string& utf8_url) {
  const char kPrefix[] = "file:///tmp/LayoutTests/";
  const int kPrefixLen = arraysize(kPrefix) - 1;

  if (utf8_url.compare(0, kPrefixLen, kPrefix, kPrefixLen))
    return WebURL(GURL(utf8_url));

  base::FilePath replace_path =
      ShellRenderProcessObserver::GetInstance()->webkit_source_dir().Append(
          FILE_PATH_LITERAL("LayoutTests/"));
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

TestPreferences* WebKitTestRunner::preferences() {
  return &prefs_;
}

void WebKitTestRunner::applyPreferences() {
  WebPreferences prefs = render_view()->GetWebkitPreferences();
  ExportLayoutTestSpecificPreferences(prefs_, &prefs);
  render_view()->SetWebkitPreferences(prefs);
  Send(new ShellViewHostMsg_OverridePreferences(routing_id(), prefs));
}

std::string WebKitTestRunner::makeURLErrorDescription(
    const WebURLError& error) {
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
      domain.c_str(), code, error.unreachableURL.spec().data());
}

void WebKitTestRunner::useUnfortunateSynchronousResizeMode(bool enable) {
  UseSynchronousResizeModeVisitor visitor(enable);
  RenderView::ForEach(&visitor);
}

void WebKitTestRunner::enableAutoResizeMode(const WebSize& min_size,
                                            const WebSize& max_size) {
  EnableAutoResizeMode(render_view(), min_size, max_size);
}

void WebKitTestRunner::disableAutoResizeMode(const WebSize& new_size) {
  DisableAutoResizeMode(render_view(), new_size);
  if (!new_size.isEmpty())
    ForceResizeRenderView(render_view(), new_size);
}

void WebKitTestRunner::clearDevToolsLocalStorage() {
  Send(new ShellViewHostMsg_ClearDevToolsLocalStorage(routing_id()));
}

void WebKitTestRunner::showDevTools(const std::string& settings,
                                    const std::string& frontend_url) {
  Send(new ShellViewHostMsg_ShowDevTools(
      routing_id(), settings, frontend_url));
}

void WebKitTestRunner::closeDevTools() {
  Send(new ShellViewHostMsg_CloseDevTools(routing_id()));
  WebDevToolsAgent* agent = render_view()->GetWebView()->devToolsAgent();
  if (agent)
    agent->detach();
}

void WebKitTestRunner::evaluateInWebInspector(long call_id,
                                              const std::string& script) {
  WebDevToolsAgent* agent = render_view()->GetWebView()->devToolsAgent();
  if (agent)
    agent->evaluateInWebInspector(call_id, WebString::fromUTF8(script));
}

void WebKitTestRunner::clearAllDatabases() {
  Send(new ShellViewHostMsg_ClearAllDatabases(routing_id()));
}

void WebKitTestRunner::setDatabaseQuota(int quota) {
  Send(new ShellViewHostMsg_SetDatabaseQuota(routing_id(), quota));
}

blink::WebNotificationPresenter::Permission
WebKitTestRunner::checkWebNotificationPermission(const GURL& origin) {
  int permission = blink::WebNotificationPresenter::PermissionNotAllowed;
  Send(new ShellViewHostMsg_CheckWebNotificationPermission(
          routing_id(),
          origin,
          &permission));
  return static_cast<blink::WebNotificationPresenter::Permission>(permission);
}

void WebKitTestRunner::grantWebNotificationPermission(const GURL& origin,
                                                      bool permission_granted) {
  Send(new ShellViewHostMsg_GrantWebNotificationPermission(
      routing_id(), origin, permission_granted));
}

void WebKitTestRunner::clearWebNotificationPermissions() {
  Send(new ShellViewHostMsg_ClearWebNotificationPermissions(routing_id()));
}

void WebKitTestRunner::setDeviceScaleFactor(float factor) {
  SetDeviceScaleFactor(render_view(), factor);
}

void WebKitTestRunner::setDeviceColorProfile(const std::string& name) {
  SetDeviceColorProfile(render_view(), name);
}

void WebKitTestRunner::setFocus(WebTestProxyBase* proxy, bool focus) {
  ProxyToRenderViewVisitor visitor(proxy);
  RenderView::ForEach(&visitor);
  if (!visitor.render_view()) {
    NOTREACHED();
    return;
  }

  // Check whether the focused view was closed meanwhile.
  if (!WebKitTestRunner::Get(focused_view_))
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

void WebKitTestRunner::setAcceptAllCookies(bool accept) {
  Send(new ShellViewHostMsg_AcceptAllCookies(routing_id(), accept));
}

std::string WebKitTestRunner::pathToLocalResource(const std::string& resource) {
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
  while (StringToLowerASCII(result).find("file:////") == 0) {
    result = result.substr(0, strlen("file:///")) +
             result.substr(strlen("file:////"));
  }
  return rewriteLayoutTestsURL(result).spec();
}

void WebKitTestRunner::setLocale(const std::string& locale) {
  setlocale(LC_ALL, locale.c_str());
}

void WebKitTestRunner::testFinished() {
  if (!is_main_window_) {
    Send(new ShellViewHostMsg_TestFinishedInSecondaryWindow(routing_id()));
    return;
  }
  WebTestInterfaces* interfaces =
      ShellRenderProcessObserver::GetInstance()->test_interfaces();
  interfaces->setTestIsRunning(false);
  if (interfaces->testRunner()->ShouldDumpBackForwardList()) {
    SyncNavigationStateVisitor visitor;
    RenderView::ForEach(&visitor);
    Send(new ShellViewHostMsg_CaptureSessionHistory(routing_id()));
  } else {
    CaptureDump();
  }
}

void WebKitTestRunner::closeRemainingWindows() {
  NavigateAwayVisitor visitor(render_view());
  RenderView::ForEach(&visitor);
  Send(new ShellViewHostMsg_CloseRemainingWindows(routing_id()));
}

void WebKitTestRunner::deleteAllCookies() {
  Send(new ShellViewHostMsg_DeleteAllCookies(routing_id()));
}

int WebKitTestRunner::navigationEntryCount() {
  return GetLocalSessionHistoryLength(render_view());
}

void WebKitTestRunner::goToOffset(int offset) {
  Send(new ShellViewHostMsg_GoToOffset(routing_id(), offset));
}

void WebKitTestRunner::reload() {
  Send(new ShellViewHostMsg_Reload(routing_id()));
}

void WebKitTestRunner::loadURLForFrame(const WebURL& url,
                             const std::string& frame_name) {
  Send(new ShellViewHostMsg_LoadURLForFrame(
      routing_id(), url, frame_name));
}

bool WebKitTestRunner::allowExternalPages() {
  return test_config_.allow_external_pages;
}

std::string WebKitTestRunner::dumpHistoryForWindow(WebTestProxyBase* proxy) {
  size_t pos = 0;
  std::vector<int>::iterator id;
  for (id = routing_ids_.begin(); id != routing_ids_.end(); ++id, ++pos) {
    RenderView* render_view = RenderView::FromRoutingID(*id);
    if (!render_view) {
      NOTREACHED();
      continue;
    }
    if (WebKitTestRunner::Get(render_view)->proxy() == proxy)
      break;
  }

  if (id == routing_ids_.end()) {
    NOTREACHED();
    return std::string();
  }
  return DumpBackForwardList(session_histories_[pos],
                             current_entry_indexes_[pos]);
}

// RenderViewObserver  --------------------------------------------------------

void WebKitTestRunner::DidClearWindowObject(WebLocalFrame* frame) {
  WebTestingSupport::injectInternalsObject(frame);
  ShellRenderProcessObserver::GetInstance()->test_interfaces()->bindTo(frame);
  GCController::Install(frame);
}

bool WebKitTestRunner::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebKitTestRunner, message)
    IPC_MESSAGE_HANDLER(ShellViewMsg_SetTestConfiguration,
                        OnSetTestConfiguration)
    IPC_MESSAGE_HANDLER(ShellViewMsg_SessionHistory, OnSessionHistory)
    IPC_MESSAGE_HANDLER(ShellViewMsg_Reset, OnReset)
    IPC_MESSAGE_HANDLER(ShellViewMsg_NotifyDone, OnNotifyDone)
    IPC_MESSAGE_HANDLER(ShellViewMsg_TryLeakDetection, OnTryLeakDetection)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void WebKitTestRunner::Navigate(const GURL& url) {
  focus_on_next_commit_ = true;
  if (!is_main_window_ &&
      ShellRenderProcessObserver::GetInstance()->main_test_runner() == this) {
    WebTestInterfaces* interfaces =
        ShellRenderProcessObserver::GetInstance()->test_interfaces();
    interfaces->setTestIsRunning(true);
    interfaces->configureForTestWithURL(GURL(), false);
    ForceResizeRenderView(render_view(), WebSize(800, 600));
  }
}

void WebKitTestRunner::DidCommitProvisionalLoad(WebLocalFrame* frame,
                                                bool is_new_navigation) {
  if (!focus_on_next_commit_)
    return;
  focus_on_next_commit_ = false;
  render_view()->GetWebView()->setFocusedFrame(frame);
}

void WebKitTestRunner::DidFailProvisionalLoad(WebLocalFrame* frame,
                                              const WebURLError& error) {
  focus_on_next_commit_ = false;
}

// Public methods - -----------------------------------------------------------

void WebKitTestRunner::Reset() {
  // The proxy_ is always non-NULL, it is set right after construction.
  proxy_->set_widget(render_view()->GetWebView());
  proxy_->Reset();
  prefs_.Reset();
  routing_ids_.clear();
  session_histories_.clear();
  current_entry_indexes_.clear();

  render_view()->ClearEditCommands();
  render_view()->GetWebView()->mainFrame()->setName(WebString());
  render_view()->GetWebView()->mainFrame()->clearOpener();
  render_view()->GetWebView()->setPageScaleFactorLimits(-1, -1);
  render_view()->GetWebView()->setPageScaleFactor(1, WebPoint(0, 0));

  // Resetting the internals object also overrides the WebPreferences, so we
  // have to sync them to WebKit again.
  WebTestingSupport::resetInternalsObject(
      render_view()->GetWebView()->mainFrame()->toWebLocalFrame());
  render_view()->SetWebkitPreferences(render_view()->GetWebkitPreferences());
}

// Private methods  -----------------------------------------------------------

void WebKitTestRunner::CaptureDump() {
  WebTestInterfaces* interfaces =
      ShellRenderProcessObserver::GetInstance()->test_interfaces();
  TRACE_EVENT0("shell", "WebKitTestRunner::CaptureDump");

  if (interfaces->testRunner()->ShouldDumpAsAudio()) {
    std::vector<unsigned char> vector_data;
    interfaces->testRunner()->GetAudioData(&vector_data);
    Send(new ShellViewHostMsg_AudioDump(routing_id(), vector_data));
  } else {
    Send(new ShellViewHostMsg_TextDump(routing_id(),
                                       proxy()->CaptureTree(false)));

    if (test_config_.enable_pixel_dumping &&
        interfaces->testRunner()->ShouldGeneratePixelResults()) {
      CHECK(render_view()->GetWebView()->isAcceleratedCompositingActive());
      proxy()->CapturePixelsAsync(base::Bind(
          &WebKitTestRunner::CaptureDumpPixels, base::Unretained(this)));
      return;
    }
  }

  CaptureDumpComplete();
}

void WebKitTestRunner::CaptureDumpPixels(const SkBitmap& snapshot) {
  DCHECK_NE(0, snapshot.info().fWidth);
  DCHECK_NE(0, snapshot.info().fHeight);

  SkAutoLockPixels snapshot_lock(snapshot);
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

void WebKitTestRunner::CaptureDumpComplete() {
  render_view()->GetWebView()->mainFrame()->stopLoading();

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&WebKitTestRunner::Send),
                 base::Unretained(this),
                 new ShellViewHostMsg_TestFinished(routing_id())));
}

void WebKitTestRunner::OnSetTestConfiguration(
    const ShellTestConfiguration& params) {
  test_config_ = params;
  is_main_window_ = true;

  ForceResizeRenderView(
      render_view(),
      WebSize(params.initial_size.width(), params.initial_size.height()));
  setFocus(proxy_, true);

  WebTestInterfaces* interfaces =
      ShellRenderProcessObserver::GetInstance()->test_interfaces();
  interfaces->setTestIsRunning(true);
  interfaces->configureForTestWithURL(params.test_url,
                                      params.enable_pixel_dumping);
}

void WebKitTestRunner::OnSessionHistory(
    const std::vector<int>& routing_ids,
    const std::vector<std::vector<PageState> >& session_histories,
    const std::vector<unsigned>& current_entry_indexes) {
  routing_ids_ = routing_ids;
  session_histories_ = session_histories;
  current_entry_indexes_ = current_entry_indexes;
  CaptureDump();
}

void WebKitTestRunner::OnReset() {
  ShellRenderProcessObserver::GetInstance()->test_interfaces()->resetAll();
  Reset();
  // Navigating to about:blank will make sure that no new loads are initiated
  // by the renderer.
  render_view()->GetWebView()->mainFrame()->loadRequest(
      WebURLRequest(GURL(url::kAboutBlankURL)));
  Send(new ShellViewHostMsg_ResetDone(routing_id()));
}

void WebKitTestRunner::OnNotifyDone() {
  render_view()->GetWebView()->mainFrame()->executeScript(
      WebScriptSource(WebString::fromUTF8("testRunner.notifyDone();")));
}

void WebKitTestRunner::OnTryLeakDetection() {
  WebLocalFrame* main_frame =
      render_view()->GetWebView()->mainFrame()->toWebLocalFrame();
  DCHECK_EQ(GURL(url::kAboutBlankURL), GURL(main_frame->document().url()));
  DCHECK(!main_frame->isLoading());

  leak_detector_->TryLeakDetection(main_frame);
}

void WebKitTestRunner::ReportLeakDetectionResult(
    const LeakDetectionResult& report) {
  Send(new ShellViewHostMsg_LeakDetectionDone(routing_id(), report));
}

}  // namespace content
