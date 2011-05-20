// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/session.h"

#include <sstream>
#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/automation_json_requests.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/session_manager.h"
#include "chrome/test/webdriver/utility_functions.h"
#include "chrome/test/webdriver/webdriver_key_converter.h"
#include "googleurl/src/gurl.h"
#include "third_party/webdriver/atoms.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace webdriver {

FrameId::FrameId(int window_id, const FramePath& frame_path)
    : window_id(window_id),
      frame_path(frame_path) {
}

FrameId& FrameId::operator=(const FrameId& other) {
  window_id = other.window_id;
  frame_path = other.frame_path;
  return *this;
}

Session::Session()
    : id_(GenerateRandomID()),
      current_target_(FrameId(0, FramePath())),
      thread_(id_.c_str()),
      async_script_timeout_(0),
      implicit_wait_(0),
      screenshot_on_error_(false),
      use_native_events_(false),
      has_alert_prompt_text_(false) {
  SessionManager::GetInstance()->Add(this);
}

Session::~Session() {
  SessionManager::GetInstance()->Remove(id_);
}

Error* Session::Init(const FilePath& browser_exe,
                     const CommandLine& options) {
  if (!thread_.Start()) {
    delete this;
    return new Error(kUnknownError, "Cannot start session thread");
  }

  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      this,
      &Session::InitOnSessionThread,
      browser_exe,
      options,
      &error));
  if (error)
    Terminate();
  return error;
}

void Session::Terminate() {
  RunSessionTask(NewRunnableMethod(
      this,
      &Session::TerminateOnSessionThread));
  delete this;
}

Error* Session::ExecuteScript(const FrameId& frame_id,
                              const std::string& script,
                              const ListValue* const args,
                              Value** value) {
  std::string args_as_json;
  base::JSONWriter::Write(static_cast<const Value* const>(args),
                          /*pretty_print=*/false,
                          &args_as_json);

  // Every injected script is fed through the executeScript atom. This atom
  // will catch any errors that are thrown and convert them to the
  // appropriate JSON structure.
  std::string jscript = base::StringPrintf(
      "window.domAutomationController.send((%s).apply(null,"
      "[function(){%s\n},%s,true]));",
      atoms::EXECUTE_SCRIPT, script.c_str(), args_as_json.c_str());

  return ExecuteScriptAndParseResponse(frame_id, jscript, value);
}

Error* Session::ExecuteScript(const std::string& script,
                              const ListValue* const args,
                              Value** value) {
  return ExecuteScript(current_target_, script, args, value);
}

Error* Session::ExecuteAsyncScript(const FrameId& frame_id,
                                   const std::string& script,
                                   const ListValue* const args,
                                   Value** value) {
  std::string args_as_json;
  base::JSONWriter::Write(static_cast<const Value* const>(args),
                          /*pretty_print=*/false,
                          &args_as_json);

  int timeout_ms = async_script_timeout();

  // Every injected script is fed through the executeScript atom. This atom
  // will catch any errors that are thrown and convert them to the
  // appropriate JSON structure.
  std::string jscript = base::StringPrintf(
      "(%s).apply(null, [function(){%s},%s,%d,%s,true]);",
      atoms::EXECUTE_ASYNC_SCRIPT,
      script.c_str(),
      args_as_json.c_str(),
      timeout_ms,
      "function(result) {window.domAutomationController.send(result);}");

  return ExecuteScriptAndParseResponse(frame_id, jscript, value);
}

Error* Session::SendKeys(const WebElementId& element, const string16& keys) {
  bool is_displayed = false;
  Error* error = IsElementDisplayed(current_target_, element, &is_displayed);
  if (error)
    return error;
  if (!is_displayed)
    return new Error(kElementNotVisible);

  bool is_enabled = false;
  error = IsElementEnabled(current_target_, element, &is_enabled);
  if (error)
    return error;
  if (!is_enabled)
    return new Error(kInvalidElementState);

  ListValue args;
  args.Append(element.ToValue());
  // This method will first check if the element we want to send the keys to is
  // already focused, if not it will try to focus on it first.
  // TODO(jleyba): Update this to use the correct atom.
  std::string script = "if(document.activeElement != arguments[0]) {"
                       "  if(document.activeElement)"
                       "    document.activeElement.blur();"
                       "  arguments[0].focus();"
                       "}";
  Value* unscoped_result = NULL;
  error = ExecuteScript(script, &args, &unscoped_result);
  if (error) {
    error->AddDetails("Failed to focus element before sending keys");
    return error;
  }
  error = NULL;
  RunSessionTask(NewRunnableMethod(
      this,
      &Session::SendKeysOnSessionThread,
      keys,
      &error));
  return error;
}

Error* Session::NavigateToURL(const std::string& url) {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::NavigateToURL,
      current_target_.window_id,
      url,
      &error));
  return error;
}

Error* Session::GoForward() {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GoForward,
      current_target_.window_id,
      &error));
  return error;
}

Error* Session::GoBack() {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GoBack,
      current_target_.window_id,
      &error));
  return error;
}

Error* Session::Reload() {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::Reload,
      current_target_.window_id,
      &error));
  return error;
}

Error* Session::GetURL(std::string* url) {
  ListValue no_args;
  Value* unscoped_value = NULL;
  Error* error = ExecuteScript(current_target_,
                               "return document.URL;",
                               &no_args,
                               &unscoped_value);
  scoped_ptr<Value> value(unscoped_value);
  if (error)
    return error;
  if (!value->GetAsString(url))
    return new Error(kUnknownError, "GetURL Script returned non-string");
  return NULL;
}

Error* Session::GetURL(GURL* url) {
  std::string url_spec;
  Error* error = GetURL(&url_spec);
  if (!error)
    *url = GURL(url_spec);
  return error;
}

Error* Session::GetTitle(std::string* tab_title) {
  std::string script =
      "if (document.title)"
      "  return document.title;"
      "else"
      "  return document.URL;";

  ListValue no_args;
  Value* unscoped_value = NULL;
  Error* error = ExecuteScript(current_target_,
                               script,
                               &no_args,
                               &unscoped_value);
  scoped_ptr<Value> value(unscoped_value);
  if (error)
    return error;
  if (!value->GetAsString(tab_title))
    return new Error(kUnknownError, "GetTitle script returned non-string");
  return NULL;
}

Error* Session::MouseMoveAndClick(const gfx::Point& location,
                                  automation::MouseButton button) {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::MouseClick,
      current_target_.window_id,
      location,
      button,
      &error));
  if (!error)
    mouse_position_ = location;
  return error;
}

Error* Session::MouseMove(const gfx::Point& location) {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::MouseMove,
      current_target_.window_id,
      location,
      &error));
  if (!error)
    mouse_position_ = location;
  return error;
}

Error* Session::MouseDrag(const gfx::Point& start,
                          const gfx::Point& end) {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::MouseDrag,
      current_target_.window_id,
      start,
      end,
      &error));
  if (!error)
    mouse_position_ = end;
  return error;
}

Error* Session::MouseClick(automation::MouseButton button) {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::MouseClick,
      current_target_.window_id,
      mouse_position_,
      button,
      &error));
  return error;
}

Error* Session::MouseButtonDown() {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::MouseButtonDown,
      current_target_.window_id,
      mouse_position_,
      &error));
  return error;
}

Error* Session::MouseButtonUp() {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::MouseButtonUp,
      current_target_.window_id,
      mouse_position_,
      &error));
  return error;
}

Error* Session::MouseDoubleClick() {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::MouseDoubleClick,
      current_target_.window_id,
      mouse_position_,
      &error));
  return error;
}

Error* Session::GetCookies(const std::string& url, ListValue** cookies) {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GetCookies,
      url,
      cookies,
      &error));
  return error;
}

bool Session::GetCookiesDeprecated(const GURL& url, std::string* cookies) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GetCookiesDeprecated,
      current_target_.window_id,
      url,
      cookies,
      &success));
  return success;
}

bool Session::GetCookieByNameDeprecated(const GURL& url,
                                        const std::string& cookie_name,
                                        std::string* cookie) {
  std::string cookies;
  if (!GetCookiesDeprecated(url, &cookies))
    return false;

  std::string namestr = cookie_name + "=";
  std::string::size_type idx = cookies.find(namestr);
  if (idx != std::string::npos) {
    cookies.erase(0, idx + namestr.length());
    *cookie = cookies.substr(0, cookies.find(";"));
  } else {
    cookie->clear();
  }

  return true;
}

Error* Session::DeleteCookie(const std::string& url,
                           const std::string& cookie_name) {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::DeleteCookie,
      url,
      cookie_name,
      &error));
  return error;
}

bool Session::DeleteCookieDeprecated(const GURL& url,
                                     const std::string& cookie_name) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::DeleteCookieDeprecated,
      current_target_.window_id,
      url,
      cookie_name,
      &success));
  return success;
}

Error* Session::SetCookie(const std::string& url,
                          DictionaryValue* cookie_dict) {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::SetCookie,
      url,
      cookie_dict,
      &error));
  return error;
}

bool Session::SetCookieDeprecated(const GURL& url, const std::string& cookie) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::SetCookieDeprecated,
      current_target_.window_id,
      url,
      cookie,
      &success));
  return success;
}

Error* Session::GetWindowIds(std::vector<int>* window_ids) {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GetTabIds,
      window_ids,
      &error));
  return error;
}

Error* Session::SwitchToWindow(const std::string& name) {
  int switch_to_id = 0;
  int name_no = 0;
  if (base::StringToInt(name, &name_no)) {
    Error* error = NULL;
    bool does_exist = false;
    RunSessionTask(NewRunnableMethod(
        automation_.get(),
        &Automation::DoesTabExist,
        name_no,
        &does_exist,
        &error));
    if (error)
      return error;
    if (does_exist)
      switch_to_id = name_no;
  }

  if (!switch_to_id) {
    std::vector<int> window_ids;
    Error* error = GetWindowIds(&window_ids);
    if (error)
      return error;
    // See if any of the window names match |name|.
    for (size_t i = 0; i < window_ids.size(); ++i) {
      ListValue empty_list;
      Value* unscoped_name_value;
      std::string window_name;
      Error* error = ExecuteScript(FrameId(window_ids[i], FramePath()),
                                   "return window.name;",
                                   &empty_list,
                                   &unscoped_name_value);
      scoped_ptr<Value> name_value(unscoped_name_value);
      if (error)
        return error;
      if (name_value->GetAsString(&window_name) &&
          name == window_name) {
        switch_to_id = window_ids[i];
        break;
      }
    }
  }

  if (!switch_to_id)
    return new Error(kNoSuchWindow);
  current_target_ = FrameId(switch_to_id, FramePath());
  return NULL;
}

Error* Session::SwitchToFrameWithNameOrId(const std::string& name_or_id) {
  std::string script =
      "var arg = arguments[0];"
      "var xpath = '(/html/body//iframe|/html/frameset/frame)';"
      "var sub = function(s) { return s.replace(/\\$/g, arg); };"
      "xpath += sub('[@name=\"$\" or @id=\"$\"]');"
      "var frame = document.evaluate(xpath, document, null, "
      "    XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;"
      "if (!frame) { return null; }"
      "xpath = frame.tagName == 'IFRAME' ? '/html/body//iframe'"
      "                                  : '/html/frameset/frame';"
      "frame_xpath = xpath + "
      "              sub('[@' + (frame.id == arg ? 'id' : 'name') + '=\"$\"]');"
      "return [frame, frame_xpath];";
  ListValue args;
  args.Append(new StringValue(name_or_id));
  return SwitchToFrameWithJavaScriptLocatedFrame(script, &args);
}

Error* Session::SwitchToFrameWithIndex(int index) {
  // We cannot simply index into window.frames because we need to know the
  // tagName of the frameElement. If child frame N is from another domain, then
  // the following will run afoul of the same origin policy:
  //   window.frames[N].frameElement;
  // Instead of indexing window.frames, we use a an XPath expression to index
  // into the list of all IFRAME and FRAME elements on the page - if we find
  // something, then that XPath expression can be used as the new frame's XPath.
  std::string script =
      "var index = '[' + (arguments[0] + 1) + ']';"
      "var xpath = '(/html/body//iframe|/html/frameset/frame)' + "
      "            index;"
      "console.info('searching for frame by xpath: ' + xpath);"
      "var frame = document.evaluate(xpath, document, null, "
      "XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;"
      "console.info(frame == null ? 'found nothing' : frame);"
      "if (!frame) { return null; }"
      "frame_xpath = ((frame.tagName == 'IFRAME' ? "
      "    '/html/body//iframe' : '/html/frameset/frame') + index);"
      "return [frame, frame_xpath];";
  ListValue args;
  args.Append(Value::CreateIntegerValue(index));
  return SwitchToFrameWithJavaScriptLocatedFrame(script, &args);
}

Error* Session::SwitchToFrameWithElement(const WebElementId& element) {
  // TODO(jleyba): Extract this, and the other frame switch methods to an atom.
  std::string script =
      "var element = arguments[0];"
      "console.info('Attempting to switch to ' + element);"
      "if (element.nodeType != 1 || !/^i?frame$/i.test(element.tagName)) {"
      "  console.info('Element is not a frame: ' + element + "
      "' {nodeType:' + element.nodeType + ',tagName:' + element.tagName + '}');"
      "  return null;"
      "}"
      "for (var i = 0; i < window.frames.length; i++) {"
      "  if (element.contentWindow == window.frames[i]) {"
      "    return [element, '(//iframe|//frame)[' + (i + 1) + ']'];"
      "  }"
      "}"
      "console.info('Frame is not connected to this DOM tree');"
      "return null;";

  ListValue args;
  args.Append(element.ToValue());
  return SwitchToFrameWithJavaScriptLocatedFrame(script, &args);
}

void Session::SwitchToTopFrame() {
  frame_elements_.clear();
  current_target_.frame_path = FramePath();
}

Error* Session::SwitchToTopFrameIfCurrentFrameInvalid() {
  std::vector<std::string> components;
  current_target_.frame_path.GetComponents(&components);
  if (frame_elements_.size() != components.size()) {
    return new Error(kUnknownError,
                     "Frame element vector out of sync with frame path");
  }
  FramePath frame_path;
  Value* unscoped_value;
  // Start from the root path and check that each frame element that makes
  // up the current frame target is valid by executing an empty script.
  // This code should not execute script in any frame before making sure the
  // frame element is valid, otherwise the automation hangs until a timeout.
  for (size_t i = 0; i < frame_elements_.size(); ++i) {
    FrameId frame_id(current_target_.window_id, frame_path);
    ListValue args;
    args.Append(frame_elements_[i].ToValue());
    scoped_ptr<Error> error(ExecuteScript(
        frame_id, "", &args, &unscoped_value));

    scoped_ptr<Value> value(unscoped_value);
    if (error.get() && error->code() == kStaleElementReference) {
      SwitchToTopFrame();
    } else if (error.get()) {
      return error.release();
    }
    frame_path = frame_path.Append(components[i]);
  }
  return NULL;
}

Error* Session::CloseWindow() {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::CloseTab,
      current_target_.window_id,
      &error));

  if (!error) {
    std::vector<int> window_ids;
    scoped_ptr<Error> error(GetWindowIds(&window_ids));
    if (error.get() || window_ids.empty()) {
      // The automation connection will soon be closed, if not already,
      // because we supposedly just closed the last window. Terminate the
      // session.
      // TODO(kkania): This will cause us problems if GetWindowIds fails for a
      // reason other than the channel is disconnected. Look into having
      // |GetWindowIds| tell us if it just closed the last window.
      Terminate();
    }
  }
  return error;
}

Error* Session::GetAlertMessage(std::string* text) {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GetAppModalDialogMessage,
      text,
      &error));
  return error;
}

Error* Session::SetAlertPromptText(const std::string& alert_prompt_text) {
  std::string message_text;
  // Only set the alert prompt text if an alert is actually active.
  Error* error = GetAlertMessage(&message_text);
  if (!error) {
    has_alert_prompt_text_ = true;
    alert_prompt_text_ = alert_prompt_text;
  }
  return error;
}

Error* Session::AcceptOrDismissAlert(bool accept) {
  Error* error = NULL;
  if (accept && has_alert_prompt_text_) {
    RunSessionTask(NewRunnableMethod(
        automation_.get(),
        &Automation::AcceptPromptAppModalDialog,
        alert_prompt_text_,
        &error));
  } else {
    RunSessionTask(NewRunnableMethod(
        automation_.get(),
        &Automation::AcceptOrDismissAppModalDialog,
        accept,
        &error));
  }
  has_alert_prompt_text_ = false;
  return error;
}

std::string Session::GetBrowserVersion() {
  std::string version;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GetBrowserVersion,
      &version));
  return version;
}

Error* Session::CompareBrowserVersion(int client_build_no,
                                      int client_patch_no,
                                      bool* is_newer_or_equal) {
  std::string version = GetBrowserVersion();
  std::vector<std::string> split_version;
  base::SplitString(version, '.', &split_version);
  if (split_version.size() != 4) {
    return new Error(
        kUnknownError, "Browser version has unrecognized format: " + version);
  }
  int build_no, patch_no;
  if (!base::StringToInt(split_version[2], &build_no) ||
      !base::StringToInt(split_version[3], &patch_no)) {
    return new Error(
        kUnknownError, "Browser version has unrecognized format: " + version);
  }
  if (build_no < client_build_no)
    *is_newer_or_equal = false;
  else if (build_no > client_build_no)
    *is_newer_or_equal = true;
  else
    *is_newer_or_equal = patch_no >= client_patch_no;
  return NULL;
}

Error* Session::FindElement(const FrameId& frame_id,
                            const WebElementId& root_element,
                            const std::string& locator,
                            const std::string& query,
                            WebElementId* element) {
  std::vector<WebElementId> elements;
  Error* error = FindElementsHelper(
      frame_id, root_element, locator, query, true, &elements);
  if (!error)
    *element = elements[0];
  return error;
}

Error* Session::FindElements(const FrameId& frame_id,
                             const WebElementId& root_element,
                             const std::string& locator,
                             const std::string& query,
                             std::vector<WebElementId>* elements) {
  return FindElementsHelper(
      frame_id, root_element, locator, query, false, elements);
}

Error* Session::CheckElementPreconditionsForClicking(
    const WebElementId& element) {
  bool is_displayed = false;
  Error* error = IsElementDisplayed(current_target_, element, &is_displayed);
  if (error)
    return error;
  if (!is_displayed)
    return new Error(kElementNotVisible, "Element must be displayed");
  return NULL;
}

Error* Session::GetElementLocationInView(
    const WebElementId& element, gfx::Point* location) {
  CHECK(element.is_valid());

  gfx::Size elem_size;
  Error* error = GetElementSize(current_target_, element, &elem_size);
  if (error)
    return error;

  gfx::Point elem_offset(0, 0);
  error = GetLocationInViewHelper(
      current_target_, element,
      gfx::Rect(elem_offset, elem_size), &elem_offset);
  if (error)
    return error;

  for (FramePath frame_path = current_target_.frame_path;
       frame_path.IsSubframe();
       frame_path = frame_path.Parent()) {
    // Find the frame element for the current frame path.
    FrameId frame_id(current_target_.window_id, frame_path.Parent());
    WebElementId frame_element;
    error = FindElement(
        frame_id, WebElementId(""),
        LocatorType::kXpath, frame_path.BaseName().value(), &frame_element);
    if (error) {
      std::string context = base::StringPrintf(
          "Could not find frame element (%s) in frame (%s)",
          frame_path.BaseName().value().c_str(),
          frame_path.Parent().value().c_str());
      error->AddDetails(context);
      return error;
    }
    // Modify |elem_offset| by the frame's border.
    int border_left, border_top;
    error = GetElementBorder(
        frame_id, frame_element, &border_left, &border_top);
    if (error)
      return error;
    elem_offset.Offset(border_left, border_top);

    error = GetLocationInViewHelper(
        frame_id, frame_element,
        gfx::Rect(elem_offset, elem_size), &elem_offset);
    if (error)
      return error;
  }
  *location = elem_offset;
  return NULL;
}

Error* Session::GetElementSize(const FrameId& frame_id,
                               const WebElementId& element,
                               gfx::Size* size) {
  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);", atoms::GET_SIZE);
  ListValue args;
  args.Append(element.ToValue());

  Value* unscoped_result = NULL;
  Error* error = ExecuteScript(frame_id, script, &args, &unscoped_result);
  scoped_ptr<Value> result(unscoped_result);
  if (error)
    return error;
  if (!result->IsType(Value::TYPE_DICTIONARY)) {
    return new Error(kUnknownError, "GetSize atom returned non-dict type");
  }
  DictionaryValue* dict = static_cast<DictionaryValue*>(result.get());
  int width, height;
  if (!dict->GetInteger("width", &width) ||
      !dict->GetInteger("height", &height)) {
    return new Error(kUnknownError, "GetSize atom returned invalid dict");
  }
  *size = gfx::Size(width, height);
  return NULL;
}

Error* Session::GetElementEffectiveStyle(
    const FrameId& frame_id,
    const WebElementId& element,
    const std::string& prop,
    std::string* value) {
  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);", atoms::GET_EFFECTIVE_STYLE);
  ListValue args;
  args.Append(element.ToValue());
  args.Append(Value::CreateStringValue(prop));
  Value* unscoped_result = NULL;
  Error* error = ExecuteScript(
      frame_id, script, &args, &unscoped_result);
  scoped_ptr<Value> result(unscoped_result);
  if (error) {
    error->AddDetails(base::StringPrintf(
        "GetEffectiveStyle atom failed for property (%s)", prop.c_str()));
    return error;
  }

  if (!result->GetAsString(value)) {
    std::string context = base::StringPrintf(
        "GetEffectiveStyle atom returned non-string type for property (%s)",
        prop.c_str());
    return new Error(kUnknownError, context);
  }
  return NULL;
}

Error* Session::GetElementBorder(const FrameId& frame_id,
                                 const WebElementId& element,
                                 int* border_left,
                                 int* border_top) {
  std::string border_left_str, border_top_str;
  Error* error = GetElementEffectiveStyle(
      frame_id, element, "border-left-width", &border_left_str);
  if (error)
    return error;
  error = GetElementEffectiveStyle(
      frame_id, element, "border-top-width", &border_top_str);
  if (error)
    return error;

  base::StringToInt(border_left_str, border_left);
  base::StringToInt(border_top_str, border_top);
  return NULL;
}

Error* Session::IsElementDisplayed(const FrameId& frame_id,
                                   const WebElementId& element,
                                   bool* is_displayed) {
  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);", atoms::IS_DISPLAYED);
  ListValue args;
  args.Append(element.ToValue());

  Value* unscoped_result = NULL;
  Error* error = ExecuteScript(frame_id, script, &args, &unscoped_result);
  scoped_ptr<Value> result(unscoped_result);
  if (error)
    return error;
  if (!result->GetAsBoolean(is_displayed))
    return new Error(kUnknownError, "IsDisplayed atom returned non boolean");
  return NULL;
}

Error* Session::IsElementEnabled(const FrameId& frame_id,
                                 const WebElementId& element,
                                 bool* is_enabled) {
  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);", atoms::IS_ENABLED);
  ListValue args;
  args.Append(element.ToValue());

  Value* unscoped_result = NULL;
  Error* error = ExecuteScript(frame_id, script, &args, &unscoped_result);
  scoped_ptr<Value> result(unscoped_result);
  if (error)
    return error;
  if (!result->GetAsBoolean(is_enabled))
    return new Error(kUnknownError, "IsEnabled atom returned non boolean");
  return NULL;
}

Error* Session::WaitForAllTabsToStopLoading() {
  if (!automation_.get())
    return NULL;
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::WaitForAllTabsToStopLoading,
      &error));
  return error;
}

const std::string& Session::id() const {
  return id_;
}

const FrameId& Session::current_target() const {
  return current_target_;
}

void Session::set_async_script_timeout(int timeout_ms) {
  async_script_timeout_ = timeout_ms;
}

int Session::async_script_timeout() const {
  return async_script_timeout_;
}

void Session::set_implicit_wait(int timeout_ms) {
  implicit_wait_ = timeout_ms;
}

int Session::implicit_wait() const {
  return implicit_wait_;
}

void Session::set_screenshot_on_error(bool error) {
  screenshot_on_error_ = error;
}

bool Session::screenshot_on_error() const {
  return screenshot_on_error_;
}

void Session::set_use_native_events(bool use_native_events) {
  use_native_events_ = use_native_events;
}

bool Session::use_native_events() const {
  return use_native_events_;
}

const gfx::Point& Session::get_mouse_position() const {
  return mouse_position_;
}

void Session::RunSessionTask(Task* task) {
  base::WaitableEvent done_event(false, false);
  thread_.message_loop_proxy()->PostTask(FROM_HERE, NewRunnableMethod(
      this,
      &Session::RunSessionTaskOnSessionThread,
      task,
      &done_event));
  done_event.Wait();
}

void Session::RunSessionTaskOnSessionThread(Task* task,
                                            base::WaitableEvent* done_event) {
  task->Run();
  delete task;
  done_event->Signal();
}

void Session::InitOnSessionThread(const FilePath& browser_exe,
                                  const CommandLine& options,
                                  Error** error) {
  automation_.reset(new Automation());
  if (browser_exe.empty())
    automation_->Init(options, error);
  else
    automation_->InitWithBrowserPath(browser_exe, options, error);
  if (*error)
    return;

  std::vector<int> tab_ids;
  automation_->GetTabIds(&tab_ids, error);
  if (*error)
    return;
  if (tab_ids.empty()) {
    *error = new Error(kUnknownError, "No tab ids after initialization");
    return;
  }
  current_target_ = FrameId(tab_ids[0], FramePath());
}

void Session::TerminateOnSessionThread() {
  if (automation_.get())
    automation_->Terminate();
  automation_.reset();
}

Error* Session::ExecuteScriptAndParseResponse(const FrameId& frame_id,
                                              const std::string& script,
                                              Value** value) {
  // Should we also log the script that's being executed? It could be several KB
  // in size and will add lots of noise to the logs.
  VLOG(1) << "Executing script in frame: " << frame_id.frame_path.value();

  std::string result;
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::ExecuteScript,
      frame_id.window_id,
      frame_id.frame_path,
      script,
      &result,
      &error));
  if (error)
    return error;

  VLOG(1) << "...script result: " << result;
  scoped_ptr<Value> r(base::JSONReader::ReadAndReturnError(
      result, true, NULL, NULL));
  if (!r.get())
    return new Error(kUnknownError, "Failed to parse script result");

  if (r->GetType() != Value::TYPE_DICTIONARY)
    return new Error(kUnknownError, "Execute script did not return dictionary");

  DictionaryValue* result_dict = static_cast<DictionaryValue*>(r.get());

  Value* tmp;
  if (result_dict->Get("value", &tmp)) {
    // result_dict owns the returned value, so we need to make a copy.
    *value = tmp->DeepCopy();
  } else {
    // "value" was not defined in the returned dictionary; set to null.
    *value = Value::CreateNullValue();
  }

  int status;
  if (!result_dict->GetInteger("status", &status))
    return new Error(kUnknownError, "Execute script did not return status");
  ErrorCode code = static_cast<ErrorCode>(status);
  if (code != kSuccess)
    return new Error(code);
  return NULL;
}

void Session::SendKeysOnSessionThread(const string16& keys, Error** error) {
  std::vector<WebKeyEvent> key_events;
  std::string error_msg;
  if (!ConvertKeysToWebKeyEvents(keys, &key_events, &error_msg)) {
    *error = new Error(kUnknownError, error_msg);
    return;
  }
  for (size_t i = 0; i < key_events.size(); ++i) {
    if (use_native_events_) {
      // The automation provider will generate up/down events for us, we
      // only need to call it once as compared to the WebKeyEvent method.
      // Hence we filter events by their types, keeping only rawkeydown.
      if (key_events[i].type != automation::kRawKeyDownType)
        continue;
      automation_->SendNativeKeyEvent(
          current_target_.window_id,
          key_events[i].key_code,
          key_events[i].modifiers,
          error);
    } else {
      automation_->SendWebKeyEvent(
          current_target_.window_id, key_events[i], error);
    }
    if (*error) {
      std::string details = base::StringPrintf(
          "Failed to send key event. Event details:\n"
              "Type: %d, KeyCode: %d, UnmodifiedText: %s, ModifiedText: %s, "
              "Modifiers: %d",
          key_events[i].type,
          key_events[i].key_code,
          key_events[i].unmodified_text.c_str(),
          key_events[i].modified_text.c_str(),
          key_events[i].modifiers);
      (*error)->AddDetails(details);
      return;
    }
  }
}

Error* Session::SwitchToFrameWithJavaScriptLocatedFrame(
    const std::string& script, ListValue* args) {
  Value* unscoped_result = NULL;
  Error* error = ExecuteScript(script, args, &unscoped_result);
  scoped_ptr<Value> result(unscoped_result);
  if (error)
    return error;

  ListValue* frame_and_xpath_list;
  if (!result->GetAsList(&frame_and_xpath_list))
    return new Error(kNoSuchFrame);
  DictionaryValue* element_dict;
  std::string xpath;
  if (!frame_and_xpath_list->GetDictionary(0, &element_dict) ||
      !frame_and_xpath_list->GetString(1, &xpath)) {
    return new Error(kUnknownError,
                     "Frame finding script did not return correct type");
  }
  WebElementId new_frame_element(element_dict);
  if (!new_frame_element.is_valid()) {
    return new Error(kUnknownError,
                     "Frame finding script did not return a frame element");
  }

  frame_elements_.push_back(new_frame_element);
  current_target_.frame_path = current_target_.frame_path.Append(xpath);
  return NULL;
}

Error* Session::FindElementsHelper(const FrameId& frame_id,
                                   const WebElementId& root_element,
                                   const std::string& locator,
                                   const std::string& query,
                                   bool find_one,
                                   std::vector<WebElementId>* elements) {
  CHECK(root_element.is_valid());

  std::string jscript;
  if (find_one) {
    // TODO(jleyba): Write a Chrome-specific find element atom that will
    // correctly throw an error if the element cannot be found.
    jscript = base::StringPrintf(
        "var result = (%s).apply(null, arguments);"
        "if (!result) {"
        "var e = new Error('Unable to locate element');"
        "e.code = %d;"
        "throw e;"
        "} else { return result; }",
        atoms::FIND_ELEMENT, kNoSuchElement);
  } else {
    jscript = base::StringPrintf("return (%s).apply(null, arguments);",
                                 atoms::FIND_ELEMENTS);
  }
  ListValue jscript_args;
  DictionaryValue* locator_dict = new DictionaryValue();
  locator_dict->SetString(locator, query);
  jscript_args.Append(locator_dict);
  jscript_args.Append(root_element.ToValue());

  // The element search needs to loop until at least one element is found or the
  // session's implicit wait timeout expires, whichever occurs first.
  base::Time start_time = base::Time::Now();

  scoped_ptr<Value> value;
  scoped_ptr<Error> error;
  bool done = false;
  while (!done) {
    Value* unscoped_value;
    error.reset(ExecuteScript(
        frame_id, jscript, &jscript_args, &unscoped_value));
    value.reset(unscoped_value);
    if (!error.get()) {
      // If searching for many elements, make sure we found at least one before
      // stopping.
      done = find_one ||
             (value->GetType() == Value::TYPE_LIST &&
             static_cast<ListValue*>(value.get())->GetSize() > 0);
    } else if (error->code() != kNoSuchElement) {
      return error.release();
    }
    int64 elapsed_time = (base::Time::Now() - start_time).InMilliseconds();
    done = done || elapsed_time > implicit_wait_;
    if (!done)
      base::PlatformThread::Sleep(50);  // Prevent a busy loop.
  }

  // Parse the results.
  const char* kInvalidElementDictionaryMessage =
      "Find element script returned invalid element dictionary";
  if (!error.get()) {
    if (value->IsType(Value::TYPE_LIST)) {
      ListValue* element_list = static_cast<ListValue*>(value.get());
      for (size_t i = 0; i < element_list->GetSize(); ++i) {
        DictionaryValue* element_dict = NULL;
        if (!element_list->GetDictionary(i, &element_dict)) {
          return new Error(kUnknownError,
                           "Find element script returned non-dictionary");
        }

        WebElementId element(element_dict);
        if (!element.is_valid()) {
          return new Error(kUnknownError, kInvalidElementDictionaryMessage);
        }
        elements->push_back(element);
      }
    } else if (value->IsType(Value::TYPE_DICTIONARY)) {
      DictionaryValue* element_dict =
          static_cast<DictionaryValue*>(value.get());
      WebElementId element(element_dict);
      if (!element.is_valid()) {
        return new Error(kUnknownError, kInvalidElementDictionaryMessage);
      }
      elements->push_back(element);
    } else {
      return new Error(kUnknownError,
                       "Find element script returned unsupported type");
    }
  }
  return error.release();
}

Error* Session::GetLocationInViewHelper(const FrameId& frame_id,
                                        const WebElementId& element,
                                        const gfx::Rect& region,
                                        gfx::Point* location) {
  std::string jscript = base::StringPrintf(
      "return (%s).apply(null, arguments);", atoms::GET_LOCATION_IN_VIEW);
  ListValue jscript_args;
  jscript_args.Append(element.ToValue());
  DictionaryValue* elem_offset_dict = new DictionaryValue();
  elem_offset_dict->SetInteger("left", region.x());
  elem_offset_dict->SetInteger("top", region.y());
  elem_offset_dict->SetInteger("width", region.width());
  elem_offset_dict->SetInteger("height", region.height());
  jscript_args.Append(elem_offset_dict);
  Value* unscoped_value = NULL;
  Error* error = ExecuteScript(frame_id, jscript, &jscript_args,
                               &unscoped_value);
  scoped_ptr<Value> value(unscoped_value);
  if (error)
    return error;
  if (!value->IsType(Value::TYPE_DICTIONARY)) {
    return new Error(kUnknownError,
                     "Location atom returned non-dictionary type");
  }
  DictionaryValue* loc_dict = static_cast<DictionaryValue*>(value.get());
  int x = 0, y = 0;
  if (!loc_dict->GetInteger("x", &x) ||
      !loc_dict->GetInteger("y", &y)) {
    return new Error(kUnknownError,
                     "Location atom returned bad coordinate dictionary");
  }
  *location = gfx::Point(x, y);
  return NULL;
}

Error* Session::GetScreenShot(std::string* png) {
  Error* error = NULL;
  ScopedTempDir screenshots_dir;
  if (!screenshots_dir.CreateUniqueTempDir()) {
    return new Error(kUnknownError,
                     "Could not create temp directory for screenshot");
  }

  FilePath path = screenshots_dir.path().AppendASCII("screen");
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::CaptureEntirePageAsPNG,
      current_target_.window_id,
      path,
      &error));
  if (error)
    return error;
  if (!file_util::ReadFileToString(path, png))
    return new Error(kUnknownError, "Could not read screenshot file");
  return NULL;
}

}  // namespace webdriver
