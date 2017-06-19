// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/web_view_impl.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/debugger_tracker.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/dom_tracker.h"
#include "chrome/test/chromedriver/chrome/frame_tracker.h"
#include "chrome/test/chromedriver/chrome/geolocation_override_manager.h"
#include "chrome/test/chromedriver/chrome/heap_snapshot_taker.h"
#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/chrome/js.h"
#include "chrome/test/chromedriver/chrome/mobile_emulation_override_manager.h"
#include "chrome/test/chromedriver/chrome/navigation_tracker.h"
#include "chrome/test/chromedriver/chrome/network_conditions_override_manager.h"
#include "chrome/test/chromedriver/chrome/non_blocking_navigation_tracker.h"
#include "chrome/test/chromedriver/chrome/page_load_strategy.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace {

const int kWaitForNavigationStopSeconds = 10;

Status GetContextIdForFrame(FrameTracker* tracker,
                            const std::string& frame,
                            int* context_id) {
  if (frame.empty()) {
    *context_id = 0;
    return Status(kOk);
  }
  Status status = tracker->GetContextIdForFrame(frame, context_id);
  if (status.IsError())
    return status;
  return Status(kOk);
}

const char* GetAsString(MouseEventType type) {
  switch (type) {
    case kPressedMouseEventType:
      return "mousePressed";
    case kReleasedMouseEventType:
      return "mouseReleased";
    case kMovedMouseEventType:
      return "mouseMoved";
    default:
      return "";
  }
}

const char* GetAsString(TouchEventType type) {
  switch (type) {
    case kTouchStart:
      return "touchStart";
    case kTouchEnd:
      return "touchEnd";
    case kTouchMove:
      return "touchMove";
    default:
      return "";
  }
}

const char* GetPointStateString(TouchEventType type) {
  switch (type) {
    case kTouchStart:
      return "touchPressed";
    case kTouchEnd:
      return "touchReleased";
    case kTouchMove:
      return "touchMoved";
    default:
      return "";
  }
}

const char* GetAsString(MouseButton button) {
  switch (button) {
    case kLeftMouseButton:
      return "left";
    case kMiddleMouseButton:
      return "middle";
    case kRightMouseButton:
      return "right";
    case kNoneMouseButton:
      return "none";
    default:
      return "";
  }
}

const char* GetAsString(KeyEventType type) {
  switch (type) {
    case kKeyDownEventType:
      return "keyDown";
    case kKeyUpEventType:
      return "keyUp";
    case kRawKeyDownEventType:
      return "rawKeyDown";
    case kCharEventType:
      return "char";
    default:
      return "";
  }
}

}  // namespace

WebViewImpl::WebViewImpl(const std::string& id,
                         const bool w3c_compliant,
                         const BrowserInfo* browser_info,
                         std::unique_ptr<DevToolsClient> client,
                         const DeviceMetrics* device_metrics,
                         std::string page_load_strategy)
    : id_(id),
      w3c_compliant_(w3c_compliant),
      browser_info_(browser_info),
      dom_tracker_(new DomTracker(client.get())),
      frame_tracker_(new FrameTracker(client.get())),
      dialog_manager_(new JavaScriptDialogManager(client.get())),
      navigation_tracker_(PageLoadStrategy::Create(
          page_load_strategy, client.get(),
          browser_info, dialog_manager_.get())),
      mobile_emulation_override_manager_(
          new MobileEmulationOverrideManager(client.get(), device_metrics)),
      geolocation_override_manager_(
          new GeolocationOverrideManager(client.get())),
      network_conditions_override_manager_(
          new NetworkConditionsOverrideManager(client.get())),
      heap_snapshot_taker_(new HeapSnapshotTaker(client.get())),
      debugger_(new DebuggerTracker(client.get())),
      client_(client.release()) {}

WebViewImpl::~WebViewImpl() {}

std::string WebViewImpl::GetId() {
  return id_;
}

bool WebViewImpl::WasCrashed() {
  return client_->WasCrashed();
}

Status WebViewImpl::ConnectIfNecessary() {
  return client_->ConnectIfNecessary();
}

Status WebViewImpl::HandleReceivedEvents() {
  return client_->HandleReceivedEvents();
}

Status WebViewImpl::GetUrl(std::string* url) {
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> result;
  Status status = client_->SendCommandAndGetResult(
      "Page.getNavigationHistory", params, &result);
  if (status.IsError())
    return status;
  int current_index = 0;
  if (!result->GetInteger("currentIndex", &current_index))
    return Status(kUnknownError, "navigation history missing currentIndex");
  base::ListValue* entries = nullptr;
  if (!result->GetList("entries", &entries))
    return Status(kUnknownError, "navigation history missing entries");
  base::DictionaryValue* entry = nullptr;
  if (!entries->GetDictionary(current_index, &entry))
    return Status(kUnknownError, "navigation history missing entry");
  if (!entry->GetString("url", url))
    return Status(kUnknownError, "navigation history entry is missing url");
  return Status(kOk);
}

Status WebViewImpl::Load(const std::string& url, const Timeout* timeout) {
  // Javascript URLs will cause a hang while waiting for the page to stop
  // loading, so just disallow.
  if (base::StartsWith(url, "javascript:",
                       base::CompareCase::INSENSITIVE_ASCII))
    return Status(kUnknownError, "unsupported protocol");
  base::DictionaryValue params;
  params.SetString("url", url);
  return client_->SendCommandWithTimeout("Page.navigate", params, timeout);
}

Status WebViewImpl::Reload(const Timeout* timeout) {
  base::DictionaryValue params;
  params.SetBoolean("ignoreCache", false);
  return client_->SendCommandWithTimeout("Page.reload", params, timeout);
}

Status WebViewImpl::SendCommand(const std::string& cmd,
                                const base::DictionaryValue& params) {
  return client_->SendCommand(cmd, params);
}

Status WebViewImpl::SendCommandAndGetResult(
        const std::string& cmd,
        const base::DictionaryValue& params,
        std::unique_ptr<base::Value>* value) {
  std::unique_ptr<base::DictionaryValue> result;
  Status status = client_->SendCommandAndGetResult(cmd, params, &result);
  if (status.IsError())
    return status;
  *value = std::move(result);
  return Status(kOk);
}

Status WebViewImpl::TraverseHistory(int delta, const Timeout* timeout) {
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> result;
  Status status = client_->SendCommandAndGetResult(
      "Page.getNavigationHistory", params, &result);
  if (status.IsError()) {
    // TODO(samuong): remove this once we stop supporting WebView on KitKat.
    // Older versions of WebView on Android (on KitKat and earlier) do not have
    // the Page.getNavigationHistory DevTools command handler, so fall back to
    // using JavaScript to navigate back and forward. WebView reports its build
    // number as 0, so use the error status to detect if we can't use the
    // DevTools command.
    if (browser_info_->browser_name == "webview")
      return TraverseHistoryWithJavaScript(delta);
    else
      return status;
  }

  int current_index;
  if (!result->GetInteger("currentIndex", &current_index))
    return Status(kUnknownError, "DevTools didn't return currentIndex");

  base::ListValue* entries;
  if (!result->GetList("entries", &entries))
    return Status(kUnknownError, "DevTools didn't return entries");

  base::DictionaryValue* entry;
  if (!entries->GetDictionary(current_index + delta, &entry)) {
    // The WebDriver spec says that if there are no pages left in the browser's
    // history (i.e. |current_index + delta| is out of range), then we must not
    // navigate anywhere.
    return Status(kOk);
  }

  int entry_id;
  if (!entry->GetInteger("id", &entry_id))
    return Status(kUnknownError, "history entry does not have an id");
  params.SetInteger("entryId", entry_id);

  return client_->SendCommandWithTimeout("Page.navigateToHistoryEntry", params,
                                         timeout);
}

Status WebViewImpl::TraverseHistoryWithJavaScript(int delta) {
  std::unique_ptr<base::Value> value;
  if (delta == -1)
    return EvaluateScript(std::string(), "window.history.back();", &value);
  else if (delta == 1)
    return EvaluateScript(std::string(), "window.history.forward();", &value);
  else
    return Status(kUnknownError, "expected delta to be 1 or -1");
}

Status WebViewImpl::EvaluateScript(const std::string& frame,
                                   const std::string& expression,
                                   std::unique_ptr<base::Value>* result) {
  int context_id;
  Status status = GetContextIdForFrame(frame_tracker_.get(), frame,
                                       &context_id);
  if (status.IsError())
    return status;
  return internal::EvaluateScriptAndGetValue(
      client_.get(), context_id, expression, result);
}

Status WebViewImpl::CallFunction(const std::string& frame,
                                 const std::string& function,
                                 const base::ListValue& args,
                                 std::unique_ptr<base::Value>* result) {
  std::string json;
  base::JSONWriter::Write(args, &json);
  std::string w3c = w3c_compliant_ ? "true" : "false";
  // TODO(zachconrad): Second null should be array of shadow host ids.
  std::string expression = base::StringPrintf(
      "(%s).apply(null, [null, %s, %s, %s])",
      kCallFunctionScript,
      function.c_str(),
      json.c_str(),
      w3c.c_str());
  std::unique_ptr<base::Value> temp_result;
  Status status = EvaluateScript(frame, expression, &temp_result);
  if (status.IsError())
      return status;
  return internal::ParseCallFunctionResult(*temp_result, result);
}

Status WebViewImpl::CallAsyncFunction(const std::string& frame,
                                      const std::string& function,
                                      const base::ListValue& args,
                                      const base::TimeDelta& timeout,
                                      std::unique_ptr<base::Value>* result) {
  return CallAsyncFunctionInternal(
      frame, function, args, false, timeout, result);
}

Status WebViewImpl::CallUserAsyncFunction(
    const std::string& frame,
    const std::string& function,
    const base::ListValue& args,
    const base::TimeDelta& timeout,
    std::unique_ptr<base::Value>* result) {
  return CallAsyncFunctionInternal(
      frame, function, args, true, timeout, result);
}

Status WebViewImpl::GetFrameByFunction(const std::string& frame,
                                       const std::string& function,
                                       const base::ListValue& args,
                                       std::string* out_frame) {
  int context_id;
  Status status = GetContextIdForFrame(frame_tracker_.get(), frame,
                                       &context_id);
  if (status.IsError())
    return status;
  bool found_node;
  int node_id;
  status = internal::GetNodeIdFromFunction(
      client_.get(), context_id, function, args,
      &found_node, &node_id, w3c_compliant_);
  if (status.IsError())
    return status;
  if (!found_node)
    return Status(kNoSuchFrame);
  return dom_tracker_->GetFrameIdForNode(node_id, out_frame);
}

Status WebViewImpl::DispatchMouseEvents(const std::list<MouseEvent>& events,
                                        const std::string& frame) {
  double page_scale_factor = 1.0;
  if (browser_info_->build_no >= 2358 && browser_info_->build_no <= 2430 &&
      (browser_info_->is_android ||
       mobile_emulation_override_manager_->IsEmulatingTouch())) {
    // As of crrev.com/323900, on Android and under mobile emulation,
    // Input.dispatchMouseEvent fails to apply the page scale factor to the
    // mouse event coordinates. This leads to the MouseEvent being triggered on
    // the wrong location on the page. This was fixed on the browser side in
    // crrev.com/333979.
    // TODO(samuong): remove once we stop supporting M45.
    std::unique_ptr<base::Value> value;
    Status status = EvaluateScript(
        std::string(), "window.screen.width / window.innerWidth;", &value);
    if (status.IsError())
      return status;
    if (!value->GetAsDouble(&page_scale_factor))
      return Status(kUnknownError, "unable to determine page scale factor");
  }
  for (std::list<MouseEvent>::const_iterator it = events.begin();
       it != events.end(); ++it) {
    base::DictionaryValue params;
    params.SetString("type", GetAsString(it->type));
    params.SetInteger("x", it->x * page_scale_factor);
    params.SetInteger("y", it->y * page_scale_factor);
    params.SetInteger("modifiers", it->modifiers);
    params.SetString("button", GetAsString(it->button));
    params.SetInteger("clickCount", it->click_count);
    Status status = client_->SendCommand("Input.dispatchMouseEvent", params);
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

Status WebViewImpl::DispatchTouchEvent(const TouchEvent& event) {
  base::DictionaryValue params;
  params.SetString("type", GetAsString(event.type));
  auto point = base::MakeUnique<base::DictionaryValue>();
  point->SetString("state", GetPointStateString(event.type));
  point->SetInteger("x", event.x);
  point->SetInteger("y", event.y);
  auto point_list = base::MakeUnique<base::ListValue>();
  point_list->Append(std::move(point));
  params.Set("touchPoints", std::move(point_list));
  return client_->SendCommand("Input.dispatchTouchEvent", params);
}

Status WebViewImpl::DispatchTouchEvents(const std::list<TouchEvent>& events) {
  for (std::list<TouchEvent>::const_iterator it = events.begin();
       it != events.end(); ++it) {
    Status status = DispatchTouchEvent(*it);
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

Status WebViewImpl::DispatchKeyEvents(const std::list<KeyEvent>& events) {
  for (std::list<KeyEvent>::const_iterator it = events.begin();
       it != events.end(); ++it) {
    base::DictionaryValue params;
    params.SetString("type", GetAsString(it->type));
    if (it->modifiers & kNumLockKeyModifierMask) {
      params.SetBoolean("isKeypad", true);
      params.SetInteger("modifiers",
                        it->modifiers & ~kNumLockKeyModifierMask);
    } else {
      params.SetInteger("modifiers", it->modifiers);
    }
    params.SetString("text", it->modified_text);
    params.SetString("unmodifiedText", it->unmodified_text);
    params.SetInteger("nativeVirtualKeyCode", it->key_code);
    params.SetInteger("windowsVirtualKeyCode", it->key_code);
    ui::DomCode dom_code = ui::UsLayoutKeyboardCodeToDomCode(it->key_code);
    std::string code = ui::KeycodeConverter::DomCodeToCodeString(dom_code);
    if (!code.empty())
      params.SetString("code", code);
    if (!it->key.empty())
      params.SetString("key", it->key);
    Status status = client_->SendCommand("Input.dispatchKeyEvent", params);
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

Status WebViewImpl::GetCookies(std::unique_ptr<base::ListValue>* cookies,
                               const std::string& current_page_url) {
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> result;

  if (browser_info_->build_no >= 3029 &&
      browser_info_->browser_name != "webview") {
    base::ListValue url_list;
    url_list.AppendString(current_page_url);
    params.Set("urls", base::MakeUnique<base::Value>(url_list));
    Status status =
        client_->SendCommandAndGetResult("Network.getCookies", params, &result);
    if (status.IsError())
      return status;
  } else {
    Status status =
        client_->SendCommandAndGetResult("Page.getCookies", params, &result);
    if (status.IsError())
      return status;
  }

  base::ListValue* cookies_tmp;
  if (!result->GetList("cookies", &cookies_tmp))
    return Status(kUnknownError, "DevTools didn't return cookies");
  cookies->reset(cookies_tmp->DeepCopy());
  return Status(kOk);
}

Status WebViewImpl::DeleteCookie(const std::string& name,
                                 const std::string& url) {
  base::DictionaryValue params;
  params.SetString("cookieName", name);
  params.SetString("url", url);
  return client_->SendCommand("Page.deleteCookie", params);
}

Status WebViewImpl::AddCookie(const std::string& name,
                              const std::string& url,
                              const std::string& value,
                              const std::string& domain,
                              const std::string& path,
                              bool secure,
                              bool httpOnly,
                              double expiry) {
  base::DictionaryValue params;
  params.SetString("name", name);
  params.SetString("url", url);
  params.SetString("value", value);
  params.SetString("domain", domain);
  params.SetString("path", path);
  params.SetBoolean("secure", secure);
  params.SetBoolean("httpOnly", httpOnly);
  params.SetDouble("expirationDate", expiry);
  return client_->SendCommand("Network.setCookie", params);
}

Status WebViewImpl::WaitForPendingNavigations(const std::string& frame_id,
                                              const Timeout& timeout,
                                              bool stop_load_on_timeout) {
  VLOG(0) << "Waiting for pending navigations...";
  const auto not_pending_navigation =
      base::Bind(&WebViewImpl::IsNotPendingNavigation, base::Unretained(this),
                 frame_id, base::Unretained(&timeout));
  Status status = client_->HandleEventsUntil(not_pending_navigation, timeout);
  if (status.code() == kTimeout && stop_load_on_timeout) {
    VLOG(0) << "Timed out. Stopping navigation...";
    std::unique_ptr<base::Value> unused_value;
    navigation_tracker_->set_timed_out(true);
    EvaluateScript(std::string(), "window.stop();", &unused_value);
    // We don't consider |timeout| here to make sure the navigation actually
    // stops and we cleanup properly after a command that caused a navigation
    // that timed out.  Otherwise we might have to wait for that before
    // executing the next command, and it will be counted towards its timeout.
    Status new_status = client_->HandleEventsUntil(
        not_pending_navigation,
        Timeout(base::TimeDelta::FromSeconds(kWaitForNavigationStopSeconds)));
    navigation_tracker_->set_timed_out(false);
    if (new_status.IsError())
      status = new_status;
  }
  VLOG(0) << "Done waiting for pending navigations. Status: "
          << status.message();
  return status;
}

Status WebViewImpl::IsPendingNavigation(const std::string& frame_id,
                                        const Timeout* timeout,
                                        bool* is_pending) {
  return
      navigation_tracker_->IsPendingNavigation(frame_id, timeout, is_pending);
}

JavaScriptDialogManager* WebViewImpl::GetJavaScriptDialogManager() {
  return dialog_manager_.get();
}

Status WebViewImpl::OverrideGeolocation(const Geoposition& geoposition) {
  return geolocation_override_manager_->OverrideGeolocation(geoposition);
}

Status WebViewImpl::OverrideNetworkConditions(
    const NetworkConditions& network_conditions) {
  return network_conditions_override_manager_->OverrideNetworkConditions(
      network_conditions);
}

Status WebViewImpl::CaptureScreenshot(std::string* screenshot) {
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> result;
  Status status = client_->SendCommandAndGetResult(
      "Page.captureScreenshot", params, &result);
  if (status.IsError())
    return status;
  if (!result->GetString("data", screenshot))
    return Status(kUnknownError, "expected string 'data' in response");
  return Status(kOk);
}

Status WebViewImpl::SetFileInputFiles(
    const std::string& frame,
    const base::DictionaryValue& element,
    const std::vector<base::FilePath>& files) {
  base::ListValue file_list;
  for (size_t i = 0; i < files.size(); ++i) {
    if (!files[i].IsAbsolute()) {
      return Status(kUnknownError,
                    "path is not absolute: " + files[i].AsUTF8Unsafe());
    }
    if (files[i].ReferencesParent()) {
      return Status(kUnknownError,
                    "path is not canonical: " + files[i].AsUTF8Unsafe());
    }
    file_list.AppendString(files[i].value());
  }

  int context_id;
  Status status = GetContextIdForFrame(frame_tracker_.get(), frame,
                                       &context_id);
  if (status.IsError())
    return status;
  base::ListValue args;
  args.Append(element.CreateDeepCopy());
  bool found_node;
  int node_id;
  status = internal::GetNodeIdFromFunction(
      client_.get(), context_id, "function(element) { return element; }",
      args, &found_node, &node_id, w3c_compliant_);
  if (status.IsError())
    return status;
  if (!found_node)
    return Status(kUnknownError, "no node ID for file input");
  base::DictionaryValue params;
  params.SetInteger("nodeId", node_id);
  params.Set("files", base::MakeUnique<base::Value>(file_list));
  return client_->SendCommand("DOM.setFileInputFiles", params);
}

Status WebViewImpl::TakeHeapSnapshot(std::unique_ptr<base::Value>* snapshot) {
  return heap_snapshot_taker_->TakeSnapshot(snapshot);
}

Status WebViewImpl::InitProfileInternal() {
  base::DictionaryValue params;

  // TODO: Remove Debugger.enable after Chrome 36 stable is released.
  Status status_debug = client_->SendCommand("Debugger.enable", params);

  if (status_debug.IsError())
    return status_debug;

  Status status_profiler = client_->SendCommand("Profiler.enable", params);

  if (status_profiler.IsError()) {
    Status status_debugger = client_->SendCommand("Debugger.disable", params);
    if (status_debugger.IsError())
      return status_debugger;

    return status_profiler;
  }

  return Status(kOk);
}

Status WebViewImpl::StopProfileInternal() {
  base::DictionaryValue params;
  Status status_debug = client_->SendCommand("Debugger.disable", params);
  Status status_profiler = client_->SendCommand("Profiler.disable", params);

  if (status_debug.IsError()) {
    return status_debug;
  } else if (status_profiler.IsError()) {
    return status_profiler;
  }

  return Status(kOk);
}

Status WebViewImpl::StartProfile() {
  Status status_init = InitProfileInternal();

  if (status_init.IsError())
    return status_init;

  base::DictionaryValue params;
  return client_->SendCommand("Profiler.start", params);
}

Status WebViewImpl::EndProfile(std::unique_ptr<base::Value>* profile_data) {
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> profile_result;

  Status status = client_->SendCommandAndGetResult(
      "Profiler.stop", params, &profile_result);

  if (status.IsError()) {
    Status disable_profile_status = StopProfileInternal();
    if (disable_profile_status.IsError()) {
      return disable_profile_status;
    } else {
      return status;
    }
  }

  *profile_data = std::move(profile_result);
  return status;
}

Status WebViewImpl::SynthesizeTapGesture(int x,
                                         int y,
                                         int tap_count,
                                         bool is_long_press) {
  base::DictionaryValue params;
  params.SetInteger("x", x);
  params.SetInteger("y", y);
  params.SetInteger("tapCount", tap_count);
  if (is_long_press)
    params.SetInteger("duration", 1500);
  return client_->SendCommand("Input.synthesizeTapGesture", params);
}

Status WebViewImpl::SynthesizeScrollGesture(int x,
                                            int y,
                                            int xoffset,
                                            int yoffset) {
  base::DictionaryValue params;
  params.SetInteger("x", x);
  params.SetInteger("y", y);
  // Chrome's synthetic scroll gesture is actually a "swipe" gesture, so the
  // direction of the swipe is opposite to the scroll (i.e. a swipe up scrolls
  // down, and a swipe left scrolls right).
  params.SetInteger("xDistance", -xoffset);
  params.SetInteger("yDistance", -yoffset);
  return client_->SendCommand("Input.synthesizeScrollGesture", params);
}

Status WebViewImpl::SynthesizePinchGesture(int x, int y, double scale_factor) {
  base::DictionaryValue params;
  params.SetInteger("x", x);
  params.SetInteger("y", y);
  params.SetDouble("scaleFactor", scale_factor);
  return client_->SendCommand("Input.synthesizePinchGesture", params);
}

Status WebViewImpl::GetScreenOrientation(std::string* orientation) {
  base::DictionaryValue empty_params;
  std::unique_ptr<base::DictionaryValue> result;
  Status status =
    client_->SendCommandAndGetResult("Emulation.getScreenOrientation",
                                      empty_params,
                                      &result);
  if (status.IsError() || !result->GetString("orientation", orientation))
    return status;
  return Status(kOk);
}

Status WebViewImpl::SetScreenOrientation(std::string orientation) {
  base::DictionaryValue params;
  params.SetString("screenOrientation", orientation);
  Status status =
    client_->SendCommand("Emulation.lockScreenOrientation", params);
  if (status.IsError())
    return status;
  return Status(kOk);
}

Status WebViewImpl::DeleteScreenOrientation() {
  base::DictionaryValue params;
  Status status =
    client_->SendCommand("Emulation.unlockScreenOrientation", params);
  if (status.IsError())
    return status;
  return Status(kOk);
}

Status WebViewImpl::CallAsyncFunctionInternal(
    const std::string& frame,
    const std::string& function,
    const base::ListValue& args,
    bool is_user_supplied,
    const base::TimeDelta& timeout,
    std::unique_ptr<base::Value>* result) {
  base::ListValue async_args;
  async_args.AppendString("return (" + function + ").apply(null, arguments);");
  async_args.Append(args.CreateDeepCopy());
  async_args.AppendBoolean(is_user_supplied);
  async_args.AppendInteger(timeout.InMilliseconds());
  std::unique_ptr<base::Value> tmp;
  Status status = CallFunction(
      frame, kExecuteAsyncScriptScript, async_args, &tmp);
  if (status.IsError())
    return status;

  const char kDocUnloadError[] = "document unloaded while waiting for result";
  std::string kQueryResult = base::StringPrintf(
      "function() {"
      "  var info = document.$chrome_asyncScriptInfo;"
      "  if (!info)"
      "    return {status: %d, value: '%s'};"
      "  var result = info.result;"
      "  if (!result)"
      "    return {status: 0};"
      "  delete info.result;"
      "  return result;"
      "}",
      kJavaScriptError,
      kDocUnloadError);

  while (true) {
    base::ListValue no_args;
    std::unique_ptr<base::Value> query_value;
    Status status = CallFunction(frame, kQueryResult, no_args, &query_value);
    if (status.IsError()) {
      if (status.code() == kNoSuchFrame)
        return Status(kJavaScriptError, kDocUnloadError);
      return status;
    }

    base::DictionaryValue* result_info = NULL;
    if (!query_value->GetAsDictionary(&result_info))
      return Status(kUnknownError, "async result info is not a dictionary");
    int status_code;
    if (!result_info->GetInteger("status", &status_code))
      return Status(kUnknownError, "async result info has no int 'status'");
    if (status_code != kOk) {
      std::string message;
      result_info->GetString("value", &message);
      return Status(static_cast<StatusCode>(status_code), message);
    }

    base::Value* value = NULL;
    if (result_info->Get("value", &value)) {
      result->reset(value->DeepCopy());
      return Status(kOk);
    }

    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  }
}

Status WebViewImpl::IsNotPendingNavigation(const std::string& frame_id,
                                           const Timeout* timeout,
                                           bool* is_not_pending) {
  bool is_pending;
  Status status =
      navigation_tracker_->IsPendingNavigation(frame_id, timeout, &is_pending);
  if (status.IsError())
    return status;
  // An alert may block the pending navigation.
  if (dialog_manager_->IsDialogOpen())
    return Status(kUnexpectedAlertOpen);

  *is_not_pending = !is_pending;
  return Status(kOk);
}

namespace internal {

Status EvaluateScript(DevToolsClient* client,
                      int context_id,
                      const std::string& expression,
                      EvaluateScriptReturnType return_type,
                      std::unique_ptr<base::DictionaryValue>* result) {
  base::DictionaryValue params;
  params.SetString("expression", expression);
  if (context_id)
    params.SetInteger("contextId", context_id);
  params.SetBoolean("returnByValue", return_type == ReturnByValue);
  std::unique_ptr<base::DictionaryValue> cmd_result;
  Status status = client->SendCommandAndGetResult(
      "Runtime.evaluate", params, &cmd_result);
  if (status.IsError())
    return status;

  bool was_thrown;
  if (!cmd_result->GetBoolean("wasThrown", &was_thrown)) {
    // As of crrev.com/411814, Runtime.evaluate no longer returns a 'wasThrown'
    // property in the response, so check 'exceptionDetails' instead.
    // TODO(samuong): Ignore 'wasThrown' when we stop supporting Chrome 54.
    was_thrown = cmd_result->HasKey("exceptionDetails");
  }
  if (was_thrown) {
    std::string description = "unknown";
    cmd_result->GetString("result.description", &description);
    return Status(kUnknownError,
                  "Runtime.evaluate threw exception: " + description);
  }

  base::DictionaryValue* unscoped_result;
  if (!cmd_result->GetDictionary("result", &unscoped_result))
    return Status(kUnknownError, "evaluate missing dictionary 'result'");
  result->reset(unscoped_result->DeepCopy());
  return Status(kOk);
}

Status EvaluateScriptAndGetObject(DevToolsClient* client,
                                  int context_id,
                                  const std::string& expression,
                                  bool* got_object,
                                  std::string* object_id) {
  std::unique_ptr<base::DictionaryValue> result;
  Status status = EvaluateScript(client, context_id, expression, ReturnByObject,
                                 &result);
  if (status.IsError())
    return status;
  if (!result->HasKey("objectId")) {
    *got_object = false;
    return Status(kOk);
  }
  if (!result->GetString("objectId", object_id))
    return Status(kUnknownError, "evaluate has invalid 'objectId'");
  *got_object = true;
  return Status(kOk);
}

Status EvaluateScriptAndGetValue(DevToolsClient* client,
                                 int context_id,
                                 const std::string& expression,
                                 std::unique_ptr<base::Value>* result) {
  std::unique_ptr<base::DictionaryValue> temp_result;
  Status status = EvaluateScript(client, context_id, expression, ReturnByValue,
                                 &temp_result);
  if (status.IsError())
    return status;

  std::string type;
  if (!temp_result->GetString("type", &type))
    return Status(kUnknownError, "Runtime.evaluate missing string 'type'");

  if (type == "undefined") {
    *result = base::MakeUnique<base::Value>();
  } else {
    base::Value* value;
    if (!temp_result->Get("value", &value))
      return Status(kUnknownError, "Runtime.evaluate missing 'value'");
    result->reset(value->DeepCopy());
  }
  return Status(kOk);
}

Status ParseCallFunctionResult(const base::Value& temp_result,
                               std::unique_ptr<base::Value>* result) {
  const base::DictionaryValue* dict;
  if (!temp_result.GetAsDictionary(&dict))
    return Status(kUnknownError, "call function result must be a dictionary");
  int status_code;
  if (!dict->GetInteger("status", &status_code)) {
    return Status(kUnknownError,
                  "call function result missing int 'status'");
  }
  if (status_code != kOk) {
    std::string message;
    dict->GetString("value", &message);
    return Status(static_cast<StatusCode>(status_code), message);
  }
  const base::Value* unscoped_value;
  if (!dict->Get("value", &unscoped_value)) {
    return Status(kUnknownError,
                  "call function result missing 'value'");
  }
  result->reset(unscoped_value->DeepCopy());
  return Status(kOk);
}

Status GetNodeIdFromFunction(DevToolsClient* client,
                             int context_id,
                             const std::string& function,
                             const base::ListValue& args,
                             bool* found_node,
                             int* node_id,
                             bool w3c_compliant) {
  std::string json;
  base::JSONWriter::Write(args, &json);
  std::string w3c = w3c_compliant ? "true" : "false";
  // TODO(zachconrad): Second null should be array of shadow host ids.
  std::string expression = base::StringPrintf(
      "(%s).apply(null, [null, %s, %s, %s, true])",
      kCallFunctionScript,
      function.c_str(),
      json.c_str(),
      w3c.c_str());

  bool got_object;
  std::string element_id;
  Status status = internal::EvaluateScriptAndGetObject(
      client, context_id, expression, &got_object, &element_id);
  if (status.IsError())
    return status;
  if (!got_object) {
    *found_node = false;
    return Status(kOk);
  }

  std::unique_ptr<base::DictionaryValue> cmd_result;
  {
    base::DictionaryValue params;
    params.SetString("objectId", element_id);
    status = client->SendCommandAndGetResult(
        "DOM.requestNode", params, &cmd_result);
  }
  {
    // Release the remote object before doing anything else.
    base::DictionaryValue params;
    params.SetString("objectId", element_id);
    Status release_status =
        client->SendCommand("Runtime.releaseObject", params);
    if (release_status.IsError()) {
      LOG(ERROR) << "Failed to release remote object: "
                 << release_status.message();
    }
  }
  if (status.IsError())
    return status;

  if (!cmd_result->GetInteger("nodeId", node_id))
    return Status(kUnknownError, "DOM.requestNode missing int 'nodeId'");
  *found_node = true;
  return Status(kOk);
}



}  // namespace internal
