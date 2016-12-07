// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/window_commands.h"

#include <stddef.h>

#include <list>
#include <string>

#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/basic_types.h"
#include "chrome/test/chromedriver/chrome/automation_extension.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/geoposition.h"
#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/chrome/js.h"
#include "chrome/test/chromedriver/chrome/network_conditions.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/element_util.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/util.h"

namespace {

const std::string kUnreachableWebDataURL = "data:text/html,chromewebdata";

Status GetMouseButton(const base::DictionaryValue& params,
                      MouseButton* button) {
  int button_num;
  if (!params.GetInteger("button", &button_num)) {
    button_num = 0;  // Default to left mouse button.
  } else if (button_num < 0 || button_num > 2) {
    return Status(kUnknownError,
                  base::StringPrintf("invalid button: %d", button_num));
  }
  *button = static_cast<MouseButton>(button_num);
  return Status(kOk);
}

Status GetUrl(WebView* web_view, const std::string& frame, std::string* url) {
  std::unique_ptr<base::Value> value;
  base::ListValue args;
  Status status = web_view->CallFunction(
      frame, "function() { return document.URL; }", args, &value);
  if (status.IsError())
    return status;
  if (!value->GetAsString(url))
    return Status(kUnknownError, "javascript failed to return the url");
  return Status(kOk);
}

struct Cookie {
  Cookie(const std::string& name,
         const std::string& value,
         const std::string& domain,
         const std::string& path,
         double expiry,
         bool http_only,
         bool secure,
         bool session)
      : name(name), value(value), domain(domain), path(path), expiry(expiry),
        http_only(http_only), secure(secure), session(session) {}

  std::string name;
  std::string value;
  std::string domain;
  std::string path;
  double expiry;
  bool http_only;
  bool secure;
  bool session;
};

std::unique_ptr<base::DictionaryValue> CreateDictionaryFrom(
    const Cookie& cookie) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("name", cookie.name);
  dict->SetString("value", cookie.value);
  if (!cookie.domain.empty())
    dict->SetString("domain", cookie.domain);
  if (!cookie.path.empty())
    dict->SetString("path", cookie.path);
  if (!cookie.session)
    dict->SetDouble("expiry", cookie.expiry);
  dict->SetBoolean("httpOnly", cookie.http_only);
  dict->SetBoolean("secure", cookie.secure);
  return dict;
}

Status GetVisibleCookies(WebView* web_view,
                         std::list<Cookie>* cookies) {
  std::unique_ptr<base::ListValue> internal_cookies;
  Status status = web_view->GetCookies(&internal_cookies);
  if (status.IsError())
    return status;
  std::list<Cookie> cookies_tmp;
  for (size_t i = 0; i < internal_cookies->GetSize(); ++i) {
    base::DictionaryValue* cookie_dict;
    if (!internal_cookies->GetDictionary(i, &cookie_dict))
      return Status(kUnknownError, "DevTools returns a non-dictionary cookie");

    std::string name;
    cookie_dict->GetString("name", &name);
    std::string value;
    cookie_dict->GetString("value", &value);
    std::string domain;
    cookie_dict->GetString("domain", &domain);
    std::string path;
    cookie_dict->GetString("path", &path);
    double expiry = 0;
    cookie_dict->GetDouble("expires", &expiry);
    expiry /= 1000;  // Convert from millisecond to second.
    bool http_only = false;
    cookie_dict->GetBoolean("httpOnly", &http_only);
    bool session = false;
    cookie_dict->GetBoolean("session", &session);
    bool secure = false;
    cookie_dict->GetBoolean("secure", &secure);

    cookies_tmp.push_back(
        Cookie(name, value, domain, path, expiry, http_only, secure, session));
  }
  cookies->swap(cookies_tmp);
  return Status(kOk);
}

Status ScrollCoordinateInToView(
    Session* session, WebView* web_view, int x, int y, int* offset_x,
    int* offset_y) {
  std::unique_ptr<base::Value> value;
  base::ListValue args;
  args.AppendInteger(x);
  args.AppendInteger(y);
  Status status = web_view->CallFunction(
      std::string(),
      "function(x, y) {"
      "  if (x < window.pageXOffset ||"
      "      x >= window.pageXOffset + window.innerWidth ||"
      "      y < window.pageYOffset ||"
      "      y >= window.pageYOffset + window.innerHeight) {"
      "    window.scrollTo(x - window.innerWidth/2, y - window.innerHeight/2);"
      "  }"
      "  return {"
      "    view_x: Math.floor(window.pageXOffset),"
      "    view_y: Math.floor(window.pageYOffset),"
      "    view_width: Math.floor(window.innerWidth),"
      "    view_height: Math.floor(window.innerHeight)};"
      "}",
      args,
      &value);
  if (!status.IsOk())
    return status;
  base::DictionaryValue* view_attrib;
  value->GetAsDictionary(&view_attrib);
  int view_x, view_y, view_width, view_height;
  view_attrib->GetInteger("view_x", &view_x);
  view_attrib->GetInteger("view_y", &view_y);
  view_attrib->GetInteger("view_width", &view_width);
  view_attrib->GetInteger("view_height", &view_height);
  *offset_x = x - view_x;
  *offset_y = y - view_y;
  if (*offset_x < 0 || *offset_x >= view_width || *offset_y < 0 ||
      *offset_y >= view_height)
    return Status(kUnknownError, "Failed to scroll coordinate into view");
  return Status(kOk);
}

Status ExecuteTouchEvent(
    Session* session, WebView* web_view, TouchEventType type,
    const base::DictionaryValue& params) {
  int x, y;
  if (!params.GetInteger("x", &x))
    return Status(kUnknownError, "'x' must be an integer");
  if (!params.GetInteger("y", &y))
    return Status(kUnknownError, "'y' must be an integer");
  int relative_x = x;
  int relative_y = y;
  Status status = ScrollCoordinateInToView(
      session, web_view, x, y, &relative_x, &relative_y);
  if (!status.IsOk())
    return status;
  std::list<TouchEvent> events;
  events.push_back(
      TouchEvent(type, relative_x, relative_y));
  return web_view->DispatchTouchEvents(events);
}

}  // namespace

Status ExecuteWindowCommand(const WindowCommand& command,
                            Session* session,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value) {
  Timeout timeout;
  WebView* web_view = NULL;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  status = web_view->ConnectIfNecessary();
  if (status.IsError())
    return status;

  status = web_view->HandleReceivedEvents();
  if (status.IsError())
    return status;

  JavaScriptDialogManager* dialog_manager =
      web_view->GetJavaScriptDialogManager();
  if (dialog_manager->IsDialogOpen()) {
    std::string alert_text;
    status = dialog_manager->GetDialogMessage(&alert_text);
    if (status.IsError())
      return status;

    // Close the dialog depending on the unexpectedalert behaviour set by user
    // before returning an error, so that subsequent commands do not fail.
    std::string alert_behaviour = session->unexpected_alert_behaviour;
    if (alert_behaviour == kAccept)
      status = dialog_manager->HandleDialog(true, session->prompt_text.get());
    else if (alert_behaviour == kDismiss)
      status = dialog_manager->HandleDialog(false, session->prompt_text.get());
    if (status.IsError())
      return status;

    return Status(kUnexpectedAlertOpen, "{Alert text : " + alert_text + "}");
  }

  Status nav_status(kOk);
  for (int attempt = 0; attempt < 3; attempt++) {
    if (attempt == 2) {
      // Switch to main frame and retry command if subframe no longer exists.
      session->SwitchToTopFrame();
    }

    nav_status = web_view->WaitForPendingNavigations(
        session->GetCurrentFrameId(),
        Timeout(session->page_load_timeout, &timeout), true);
    if (nav_status.IsError())
      return nav_status;

    status = command.Run(session, web_view, params, value, &timeout);
    if (status.code() == kNoSuchExecutionContext || status.code() == kTimeout) {
      // If the command timed out, let WaitForPendingNavigations cancel
      // the navigation if there is one.
      continue;
    } else if (status.IsError()) {
      // If the command failed while a new page or frame started loading, retry
      // the command after the pending navigation has completed.
      bool is_pending = false;
      nav_status = web_view->IsPendingNavigation(session->GetCurrentFrameId(),
                                                 &timeout, &is_pending);
      if (nav_status.IsError())
        return nav_status;
      else if (is_pending)
        continue;
    }
    break;
  }

  nav_status = web_view->WaitForPendingNavigations(
      session->GetCurrentFrameId(),
      Timeout(session->page_load_timeout, &timeout), true);

  if (status.IsOk() && nav_status.IsError() &&
      nav_status.code() != kUnexpectedAlertOpen)
    return nav_status;
  if (status.code() == kUnexpectedAlertOpen)
    return Status(kOk);
  return status;
}

Status ExecuteGet(Session* session,
                  WebView* web_view,
                  const base::DictionaryValue& params,
                  std::unique_ptr<base::Value>* value,
                  Timeout* timeout) {
  timeout->SetDuration(session->page_load_timeout);
  std::string url;
  if (!params.GetString("url", &url))
    return Status(kUnknownError, "'url' must be a string");
  Status status = web_view->Load(url, timeout);
  if (status.IsError())
    return status;
  session->SwitchToTopFrame();
  return Status(kOk);
}

Status ExecuteExecuteScript(Session* session,
                            WebView* web_view,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  std::string script;
  if (!params.GetString("script", &script))
    return Status(kUnknownError, "'script' must be a string");
  if (script == ":takeHeapSnapshot") {
    return web_view->TakeHeapSnapshot(value);
  } else if (script == ":startProfile") {
    return web_view->StartProfile();
  } else if (script == ":endProfile") {
    return web_view->EndProfile(value);
  } else {
    const base::ListValue* args;
    if (!params.GetList("args", &args))
      return Status(kUnknownError, "'args' must be a list");

    return web_view->CallFunction(session->GetCurrentFrameId(),
                                  "function(){" + script + "}", *args, value);
  }
}

Status ExecuteExecuteAsyncScript(Session* session,
                                 WebView* web_view,
                                 const base::DictionaryValue& params,
                                 std::unique_ptr<base::Value>* value,
                                 Timeout* timeout) {
  std::string script;
  if (!params.GetString("script", &script))
    return Status(kUnknownError, "'script' must be a string");
  const base::ListValue* args;
  if (!params.GetList("args", &args))
    return Status(kUnknownError, "'args' must be a list");

  return web_view->CallUserAsyncFunction(
      session->GetCurrentFrameId(), "function(){" + script + "}", *args,
      session->script_timeout, value);
}

Status ExecuteSwitchToFrame(Session* session,
                            WebView* web_view,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  const base::Value* id;
  if (!params.Get("id", &id))
    return Status(kUnknownError, "missing 'id'");

  if (id->IsType(base::Value::Type::NONE)) {
    session->SwitchToTopFrame();
    return Status(kOk);
  }

  std::string script;
  base::ListValue args;
  const base::DictionaryValue* id_dict;
  if (id->GetAsDictionary(&id_dict)) {
    script = "function(elem) { return elem; }";
    args.Append(id_dict->CreateDeepCopy());
  } else {
    script =
        "function(xpath) {"
        "  return document.evaluate(xpath, document, null, "
        "      XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;"
        "}";
    std::string xpath = "(/html/body//iframe|/html/frameset//frame)";
    std::string id_string;
    int id_int;
    if (id->GetAsString(&id_string)) {
      xpath += base::StringPrintf(
          "[@name=\"%s\" or @id=\"%s\"]", id_string.c_str(), id_string.c_str());
    } else if (id->GetAsInteger(&id_int)) {
      xpath += base::StringPrintf("[%d]", id_int + 1);
    } else {
      return Status(kUnknownError, "invalid 'id'");
    }
    args.AppendString(xpath);
  }
  std::string frame;
  Status status = web_view->GetFrameByFunction(
      session->GetCurrentFrameId(), script, args, &frame);
  if (status.IsError())
    return status;

  std::unique_ptr<base::Value> result;
  status = web_view->CallFunction(
      session->GetCurrentFrameId(), script, args, &result);
  if (status.IsError())
    return status;
  const base::DictionaryValue* element;
  if (!result->GetAsDictionary(&element))
    return Status(kUnknownError, "fail to locate the sub frame element");

  std::string chrome_driver_id = GenerateId();
  const char kSetFrameIdentifier[] =
      "function(frame, id) {"
      "  frame.setAttribute('cd_frame_id_', id);"
      "}";
  base::ListValue new_args;
  new_args.Append(element->CreateDeepCopy());
  new_args.AppendString(chrome_driver_id);
  result.reset(NULL);
  status = web_view->CallFunction(
      session->GetCurrentFrameId(), kSetFrameIdentifier, new_args, &result);
  if (status.IsError())
    return status;
  session->SwitchToSubFrame(frame, chrome_driver_id);
  return Status(kOk);
}

Status ExecuteSwitchToParentFrame(Session* session,
                                  WebView* web_view,
                                  const base::DictionaryValue& params,
                                  std::unique_ptr<base::Value>* value,
                                  Timeout* timeout) {
  session->SwitchToParentFrame();
  return Status(kOk);
}

Status ExecuteGetTitle(Session* session,
                       WebView* web_view,
                       const base::DictionaryValue& params,
                       std::unique_ptr<base::Value>* value,
                       Timeout* timeout) {
  const char kGetTitleScript[] = "function() {  return document.title;}";
  base::ListValue args;
  return web_view->CallFunction(std::string(), kGetTitleScript, args, value);
}

Status ExecuteGetPageSource(Session* session,
                            WebView* web_view,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  const char kGetPageSource[] =
      "function() {"
      "  return new XMLSerializer().serializeToString(document);"
      "}";
  base::ListValue args;
  return web_view->CallFunction(
      session->GetCurrentFrameId(), kGetPageSource, args, value);
}

Status ExecuteFindElement(int interval_ms,
                          Session* session,
                          WebView* web_view,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  return FindElement(interval_ms, true, NULL, session, web_view, params, value);
}

Status ExecuteFindElements(int interval_ms,
                           Session* session,
                           WebView* web_view,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value,
                           Timeout* timeout) {
  return FindElement(
      interval_ms, false, NULL, session, web_view, params, value);
}

Status ExecuteGetCurrentUrl(Session* session,
                            WebView* web_view,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  std::string url;
  Status status = GetUrl(web_view, std::string(), &url);
  if (status.IsError())
    return status;
  if (url == kUnreachableWebDataURL) {
    // https://bugs.chromium.org/p/chromedriver/issues/detail?id=1272
    const BrowserInfo* browser_info = session->chrome->GetBrowserInfo();
    bool is_kitkat_webview = browser_info->browser_name == "webview" &&
                             browser_info->major_version <= 30 &&
                             browser_info->is_android;
    if (!is_kitkat_webview) {
      // Page.getNavigationHistory isn't implemented in WebView for KitKat and
      // older Android releases.
      status = web_view->GetUrl(&url);
      if (status.IsError())
        return status;
    }
  }
  value->reset(new base::StringValue(url));
  return Status(kOk);
}

Status ExecuteGoBack(Session* session,
                     WebView* web_view,
                     const base::DictionaryValue& params,
                     std::unique_ptr<base::Value>* value,
                     Timeout* timeout) {
  timeout->SetDuration(session->page_load_timeout);
  Status status = web_view->TraverseHistory(-1, timeout);
  if (status.IsError())
    return status;
  session->SwitchToTopFrame();
  return Status(kOk);
}

Status ExecuteGoForward(Session* session,
                        WebView* web_view,
                        const base::DictionaryValue& params,
                        std::unique_ptr<base::Value>* value,
                        Timeout* timeout) {
  timeout->SetDuration(session->page_load_timeout);
  Status status = web_view->TraverseHistory(1, timeout);
  if (status.IsError())
    return status;
  session->SwitchToTopFrame();
  return Status(kOk);
}

Status ExecuteRefresh(Session* session,
                      WebView* web_view,
                      const base::DictionaryValue& params,
                      std::unique_ptr<base::Value>* value,
                      Timeout* timeout) {
  timeout->SetDuration(session->page_load_timeout);
  Status status = web_view->Reload(timeout);
  if (status.IsError())
    return status;
  session->SwitchToTopFrame();
  return Status(kOk);
}

Status ExecuteMouseMoveTo(Session* session,
                          WebView* web_view,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  std::string element_id;
  bool has_element = params.GetString("element", &element_id);
  int x_offset = 0;
  int y_offset = 0;
  bool has_offset = params.GetInteger("xoffset", &x_offset) &&
      params.GetInteger("yoffset", &y_offset);
  if (!has_element && !has_offset)
    return Status(kUnknownError, "at least an element or offset should be set");

  WebPoint location;
  if (has_element) {
    WebPoint offset(x_offset, y_offset);
    Status status = ScrollElementIntoView(session, web_view, element_id,
        has_offset ? &offset : nullptr, &location);
    if (status.IsError())
      return status;
  } else {
    location = session->mouse_position;
    if (has_offset)
      location.Offset(x_offset, y_offset);
  }

  std::list<MouseEvent> events;
  events.push_back(
      MouseEvent(kMovedMouseEventType, kNoneMouseButton,
                 location.x, location.y, session->sticky_modifiers, 0));
  Status status =
      web_view->DispatchMouseEvents(events, session->GetCurrentFrameId());
  if (status.IsOk())
    session->mouse_position = location;
  return status;
}

Status ExecuteMouseClick(Session* session,
                         WebView* web_view,
                         const base::DictionaryValue& params,
                         std::unique_ptr<base::Value>* value,
                         Timeout* timeout) {
  MouseButton button;
  Status status = GetMouseButton(params, &button);
  if (status.IsError())
    return status;
  std::list<MouseEvent> events;
  events.push_back(
      MouseEvent(kPressedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y,
                 session->sticky_modifiers, 1));
  events.push_back(
      MouseEvent(kReleasedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y,
                 session->sticky_modifiers, 1));
  return web_view->DispatchMouseEvents(events, session->GetCurrentFrameId());
}

Status ExecuteMouseButtonDown(Session* session,
                              WebView* web_view,
                              const base::DictionaryValue& params,
                              std::unique_ptr<base::Value>* value,
                              Timeout* timeout) {
  MouseButton button;
  Status status = GetMouseButton(params, &button);
  if (status.IsError())
    return status;
  std::list<MouseEvent> events;
  events.push_back(
      MouseEvent(kPressedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y,
                 session->sticky_modifiers, 1));
  return web_view->DispatchMouseEvents(events, session->GetCurrentFrameId());
}

Status ExecuteMouseButtonUp(Session* session,
                            WebView* web_view,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  MouseButton button;
  Status status = GetMouseButton(params, &button);
  if (status.IsError())
    return status;
  std::list<MouseEvent> events;
  events.push_back(
      MouseEvent(kReleasedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y,
                 session->sticky_modifiers, 1));
  return web_view->DispatchMouseEvents(events, session->GetCurrentFrameId());
}

Status ExecuteMouseDoubleClick(Session* session,
                               WebView* web_view,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value,
                               Timeout* timeout) {
  MouseButton button;
  Status status = GetMouseButton(params, &button);
  if (status.IsError())
    return status;
  std::list<MouseEvent> events;
  events.push_back(
      MouseEvent(kPressedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y,
                 session->sticky_modifiers, 2));
  events.push_back(
      MouseEvent(kReleasedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y,
                 session->sticky_modifiers, 2));
  return web_view->DispatchMouseEvents(events, session->GetCurrentFrameId());
}

Status ExecuteTouchDown(Session* session,
                        WebView* web_view,
                        const base::DictionaryValue& params,
                        std::unique_ptr<base::Value>* value,
                        Timeout* timeout) {
  return ExecuteTouchEvent(session, web_view, kTouchStart, params);
}

Status ExecuteTouchUp(Session* session,
                      WebView* web_view,
                      const base::DictionaryValue& params,
                      std::unique_ptr<base::Value>* value,
                      Timeout* timeout) {
  return ExecuteTouchEvent(session, web_view, kTouchEnd, params);
}

Status ExecuteTouchMove(Session* session,
                        WebView* web_view,
                        const base::DictionaryValue& params,
                        std::unique_ptr<base::Value>* value,
                        Timeout* timeout) {
  return ExecuteTouchEvent(session, web_view, kTouchMove, params);
}

Status ExecuteTouchScroll(Session* session,
                          WebView* web_view,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  if (session->chrome->GetBrowserInfo()->build_no < 2286) {
    // TODO(samuong): remove this once we stop supporting M41.
    return Status(kUnknownCommand, "Touch scroll action requires Chrome 42+");
  }
  WebPoint location = session->mouse_position;
  std::string element;
  if (params.GetString("element", &element)) {
    Status status = GetElementClickableLocation(
        session, web_view, element, &location);
    if (status.IsError())
      return status;
  }
  int xoffset;
  if (!params.GetInteger("xoffset", &xoffset))
    return Status(kUnknownError, "'xoffset' must be an integer");
  int yoffset;
  if (!params.GetInteger("yoffset", &yoffset))
    return Status(kUnknownError, "'yoffset' must be an integer");
  return web_view->SynthesizeScrollGesture(
      location.x, location.y, xoffset, yoffset);
}

Status ExecuteTouchPinch(Session* session,
                         WebView* web_view,
                         const base::DictionaryValue& params,
                         std::unique_ptr<base::Value>* value,
                         Timeout* timeout) {
  if (session->chrome->GetBrowserInfo()->build_no < 2286) {
    // TODO(samuong): remove this once we stop supporting M41.
    return Status(kUnknownCommand, "Pinch action requires Chrome 42+");
  }
  WebPoint location;
  if (!params.GetInteger("x", &location.x))
    return Status(kUnknownError, "'x' must be an integer");
  if (!params.GetInteger("y", &location.y))
    return Status(kUnknownError, "'y' must be an integer");
  double scale_factor;
  if (!params.GetDouble("scale", &scale_factor))
    return Status(kUnknownError, "'scale' must be an integer");
  return web_view->SynthesizePinchGesture(location.x, location.y, scale_factor);
}

Status ExecuteGetActiveElement(Session* session,
                               WebView* web_view,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value,
                               Timeout* timeout) {
  return GetActiveElement(session, web_view, value);
}

Status ExecuteSendKeysToActiveElement(Session* session,
                                      WebView* web_view,
                                      const base::DictionaryValue& params,
                                      std::unique_ptr<base::Value>* value,
                                      Timeout* timeout) {
  const base::ListValue* key_list;
  if (!params.GetList("value", &key_list))
    return Status(kUnknownError, "'value' must be a list");
  return SendKeysOnWindow(
      web_view, key_list, false, &session->sticky_modifiers);
}

Status ExecuteGetAppCacheStatus(Session* session,
                                WebView* web_view,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value,
                                Timeout* timeout) {
  return web_view->EvaluateScript(
      session->GetCurrentFrameId(),
      "applicationCache.status",
      value);
}

Status ExecuteIsBrowserOnline(Session* session,
                              WebView* web_view,
                              const base::DictionaryValue& params,
                              std::unique_ptr<base::Value>* value,
                              Timeout* timeout) {
  return web_view->EvaluateScript(
      session->GetCurrentFrameId(),
      "navigator.onLine",
      value);
}

Status ExecuteGetStorageItem(const char* storage,
                             Session* session,
                             WebView* web_view,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  std::string key;
  if (!params.GetString("key", &key))
    return Status(kUnknownError, "'key' must be a string");
  base::ListValue args;
  args.AppendString(key);
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      base::StringPrintf("function(key) { return %s[key]; }", storage),
      args,
      value);
}

Status ExecuteGetStorageKeys(const char* storage,
                             Session* session,
                             WebView* web_view,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  const char script[] =
      "var keys = [];"
      "for (var key in %s) {"
      "  keys.push(key);"
      "}"
      "keys";
  return web_view->EvaluateScript(
      session->GetCurrentFrameId(),
      base::StringPrintf(script, storage),
      value);
}

Status ExecuteSetStorageItem(const char* storage,
                             Session* session,
                             WebView* web_view,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  std::string key;
  if (!params.GetString("key", &key))
    return Status(kUnknownError, "'key' must be a string");
  std::string storage_value;
  if (!params.GetString("value", &storage_value))
    return Status(kUnknownError, "'value' must be a string");
  base::ListValue args;
  args.AppendString(key);
  args.AppendString(storage_value);
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      base::StringPrintf("function(key, value) { %s[key] = value; }", storage),
      args,
      value);
}

Status ExecuteRemoveStorageItem(const char* storage,
                                Session* session,
                                WebView* web_view,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value,
                                Timeout* timeout) {
  std::string key;
  if (!params.GetString("key", &key))
    return Status(kUnknownError, "'key' must be a string");
  base::ListValue args;
  args.AppendString(key);
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      base::StringPrintf("function(key) { %s.removeItem(key) }", storage),
      args,
      value);
}

Status ExecuteClearStorage(const char* storage,
                           Session* session,
                           WebView* web_view,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value,
                           Timeout* timeout) {
  return web_view->EvaluateScript(
      session->GetCurrentFrameId(),
      base::StringPrintf("%s.clear()", storage),
      value);
}

Status ExecuteGetStorageSize(const char* storage,
                             Session* session,
                             WebView* web_view,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  return web_view->EvaluateScript(
      session->GetCurrentFrameId(),
      base::StringPrintf("%s.length", storage),
      value);
}

Status ExecuteScreenshot(Session* session,
                         WebView* web_view,
                         const base::DictionaryValue& params,
                         std::unique_ptr<base::Value>* value,
                         Timeout* timeout) {
  Status status = session->chrome->ActivateWebView(web_view->GetId());
  if (status.IsError())
    return status;

  std::string screenshot;
  ChromeDesktopImpl* desktop = NULL;
  status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsOk() && !session->force_devtools_screenshot) {
    AutomationExtension* extension = NULL;
    status = desktop->GetAutomationExtension(&extension,
                                             session->w3c_compliant);
    if (status.IsError())
      return status;
    status = extension->CaptureScreenshot(&screenshot);
  } else {
    status = web_view->CaptureScreenshot(&screenshot);
  }
  if (status.IsError()) {
    LOG(WARNING) << "screenshot failed, retrying";
    status = web_view->CaptureScreenshot(&screenshot);
  }
  if (status.IsError())
    return status;

  value->reset(new base::StringValue(screenshot));
  return Status(kOk);
}

Status ExecuteGetCookies(Session* session,
                         WebView* web_view,
                         const base::DictionaryValue& params,
                         std::unique_ptr<base::Value>* value,
                         Timeout* timeout) {
  std::list<Cookie> cookies;
  Status status = GetVisibleCookies(web_view, &cookies);
  if (status.IsError())
    return status;
  std::unique_ptr<base::ListValue> cookie_list(new base::ListValue());
  for (std::list<Cookie>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    cookie_list->Append(CreateDictionaryFrom(*it));
  }
  value->reset(cookie_list.release());
  return Status(kOk);
}

Status ExecuteAddCookie(Session* session,
                        WebView* web_view,
                        const base::DictionaryValue& params,
                        std::unique_ptr<base::Value>* value,
                        Timeout* timeout) {
  const base::DictionaryValue* cookie;
  if (!params.GetDictionary("cookie", &cookie))
    return Status(kUnknownError, "missing 'cookie'");
  base::ListValue args;
  args.Append(cookie->CreateDeepCopy());
  std::unique_ptr<base::Value> result;
  return web_view->CallFunction(
      session->GetCurrentFrameId(), kAddCookieScript, args, &result);
}

Status ExecuteDeleteCookie(Session* session,
                           WebView* web_view,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value,
                           Timeout* timeout) {
  std::string name;
  if (!params.GetString("name", &name))
    return Status(kUnknownError, "missing 'name'");
  base::DictionaryValue params_url;
  std::unique_ptr<base::Value> value_url;
  std::string url;
  Status status = GetUrl(web_view, session->GetCurrentFrameId(), &url);
  if (status.IsError())
    return status;
  return web_view->DeleteCookie(name, url);
}

Status ExecuteDeleteAllCookies(Session* session,
                               WebView* web_view,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value,
                               Timeout* timeout) {
  std::list<Cookie> cookies;
  Status status = GetVisibleCookies(web_view, &cookies);
  if (status.IsError())
    return status;

  if (!cookies.empty()) {
    base::DictionaryValue params_url;
    std::unique_ptr<base::Value> value_url;
    std::string url;
    status = GetUrl(web_view, session->GetCurrentFrameId(), &url);
    if (status.IsError())
      return status;
    for (std::list<Cookie>::const_iterator it = cookies.begin();
         it != cookies.end(); ++it) {
      status = web_view->DeleteCookie(it->name, url);
      if (status.IsError())
        return status;
    }
  }

  return Status(kOk);
}

Status ExecuteSetLocation(Session* session,
                          WebView* web_view,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  const base::DictionaryValue* location = NULL;
  Geoposition geoposition;
  if (!params.GetDictionary("location", &location) ||
      !location->GetDouble("latitude", &geoposition.latitude) ||
      !location->GetDouble("longitude", &geoposition.longitude))
    return Status(kUnknownError, "missing or invalid 'location'");
  if (location->HasKey("accuracy") &&
      !location->GetDouble("accuracy", &geoposition.accuracy)) {
    return Status(kUnknownError, "invalid 'accuracy'");
  } else {
    // |accuracy| is not part of the WebDriver spec yet, so if it is not given
    // default to 100 meters accuracy.
    geoposition.accuracy = 100;
  }

  Status status = web_view->OverrideGeolocation(geoposition);
  if (status.IsOk())
    session->overridden_geoposition.reset(new Geoposition(geoposition));
  return status;
}

Status ExecuteSetNetworkConditions(Session* session,
                                   WebView* web_view,
                                   const base::DictionaryValue& params,
                                   std::unique_ptr<base::Value>* value,
                                   Timeout* timeout) {
  std::string network_name;
  const base::DictionaryValue* conditions = NULL;
  std::unique_ptr<NetworkConditions> network_conditions(
      new NetworkConditions());
  if (params.GetString("network_name", &network_name)) {
    // Get conditions from preset list.
    Status status = FindPresetNetwork(network_name, network_conditions.get());
    if (status.IsError())
      return status;
  } else if (params.GetDictionary("network_conditions", &conditions)) {
    // |latency| is required.
    if (!conditions->GetDouble("latency", &network_conditions->latency))
      return Status(kUnknownError,
                    "invalid 'network_conditions' is missing 'latency'");

    // Either |throughput| or the pair |download_throughput| and
    // |upload_throughput| is required.
    if (conditions->HasKey("throughput")) {
      if (!conditions->GetDouble("throughput",
                                 &network_conditions->download_throughput))
        return Status(kUnknownError, "invalid 'throughput'");
      conditions->GetDouble("throughput",
                            &network_conditions->upload_throughput);
    } else if (conditions->HasKey("download_throughput") &&
               conditions->HasKey("upload_throughput")) {
      if (!conditions->GetDouble("download_throughput",
                                 &network_conditions->download_throughput) ||
          !conditions->GetDouble("upload_throughput",
                                 &network_conditions->upload_throughput))
        return Status(kUnknownError,
                      "invalid 'download_throughput' or 'upload_throughput'");
    } else {
      return Status(kUnknownError,
                    "invalid 'network_conditions' is missing 'throughput' or "
                    "'download_throughput'/'upload_throughput' pair");
    }

    // |offline| is optional.
    if (conditions->HasKey("offline")) {
      if (!conditions->GetBoolean("offline", &network_conditions->offline))
        return Status(kUnknownError, "invalid 'offline'");
    } else {
      network_conditions->offline = false;
    }
  } else {
    return Status(kUnknownError,
                  "either 'network_conditions' or 'network_name' must be "
                  "supplied");
  }

  session->overridden_network_conditions.reset(
      network_conditions.release());
  return web_view->OverrideNetworkConditions(
      *session->overridden_network_conditions);
}

Status ExecuteDeleteNetworkConditions(Session* session,
                                      WebView* web_view,
                                      const base::DictionaryValue& params,
                                      std::unique_ptr<base::Value>* value,
                                      Timeout* timeout) {
  // Chrome does not have any command to stop overriding network conditions, so
  // we just override the network conditions with the "No throttling" preset.
  NetworkConditions network_conditions;
  // Get conditions from preset list.
  Status status = FindPresetNetwork("No throttling", &network_conditions);
  if (status.IsError())
    return status;

  status = web_view->OverrideNetworkConditions(network_conditions);
  if (status.IsError())
    return status;

  // After we've successfully overridden the network conditions with
  // "No throttling", we can delete them from |session|.
  session->overridden_network_conditions.reset();
  return status;
}

Status ExecuteTakeHeapSnapshot(Session* session,
                               WebView* web_view,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value,
                               Timeout* timeout) {
  return web_view->TakeHeapSnapshot(value);
}
