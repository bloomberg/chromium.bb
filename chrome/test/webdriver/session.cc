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
#include "base/memory/scoped_temp_dir.h"
#include "base/message_loop_proxy.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
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

ErrorCode Session::Init(const FilePath& browser_exe,
                        const CommandLine& options) {
  if (!thread_.Start()) {
    LOG(ERROR) << "Cannot start session thread";
    delete this;
    return kUnknownError;
  }

  ErrorCode code = kUnknownError;
  RunSessionTask(NewRunnableMethod(
      this,
      &Session::InitOnSessionThread,
      browser_exe,
      options,
      &code));
  if (code != kSuccess)
    Terminate();
  return code;
}

void Session::Terminate() {
  RunSessionTask(NewRunnableMethod(
      this,
      &Session::TerminateOnSessionThread));
  delete this;
}

ErrorCode Session::ExecuteScript(const FrameId& frame_id,
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

ErrorCode Session::ExecuteScript(const std::string& script,
                                 const ListValue* const args,
                                 Value** value) {
  return ExecuteScript(current_target_, script, args, value);
}

ErrorCode Session::ExecuteAsyncScript(const FrameId& frame_id,
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

ErrorCode Session::SendKeys(const WebElementId& element, const string16& keys) {
  bool is_displayed = false;
  ErrorCode code = IsElementDisplayed(current_target_, element, &is_displayed);
  if (code != kSuccess)
    return code;
  if (!is_displayed)
    return kElementNotVisible;

  bool is_enabled = false;
  code = IsElementEnabled(current_target_, element, &is_enabled);
  if (code != kSuccess) {
    LOG(ERROR) << "Failed to determine if element is enabled";
    return code;
  }
  if (!is_enabled)
    return kInvalidElementState;

  ListValue args;
  args.Append(element.ToValue());
  // This method will first check if the element we want to send the keys to is
  // already focused, if not it will try to focus on it first.
  // TODO(jleyba): Update this to use the correct atom.
  std::string script = "if(document.activeElement!=arguments[0]){"
                       "  if(document.activeElement)"
                       "    document.activeElement.blur();"
                       "  arguments[0].focus();"
                       "}";
  Value* unscoped_result = NULL;
  code = ExecuteScript(script, &args, &unscoped_result);
  if (code != kSuccess) {
    LOG(ERROR) << "Failed to get or set focus element before sending keys";
    return code;
  }
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      this,
      &Session::SendKeysOnSessionThread,
      keys,
      &success));
  if (!success)
    return kUnknownError;
  return kSuccess;
}

bool Session::NavigateToURL(const std::string& url) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::NavigateToURL,
      current_target_.window_id,
      url,
      &success));
  return success;
}

bool Session::GoForward() {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GoForward,
      current_target_.window_id,
      &success));
  return success;
}

bool Session::GoBack() {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GoBack,
      current_target_.window_id,
      &success));
  return success;
}

bool Session::Reload() {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::Reload,
      current_target_.window_id,
      &success));
  return success;
}

ErrorCode Session::GetURL(std::string* url) {
  ListValue no_args;
  Value* unscoped_value = NULL;
  ErrorCode code = ExecuteScript(current_target_,
                                 "return document.URL;",
                                 &no_args,
                                 &unscoped_value);
  scoped_ptr<Value> value(unscoped_value);
  if (code != kSuccess)
    return code;
  if (!value->GetAsString(url)) {
    LOG(ERROR) << "Script returned non-string type";
    return kUnknownError;
  }
  return kSuccess;
}

ErrorCode Session::GetURL(GURL* url) {
  std::string url_spec;
  ErrorCode code = GetURL(&url_spec);
  if (code == kSuccess) {
    *url = GURL(url_spec);
  }
  return code;
}

ErrorCode Session::GetTitle(std::string* tab_title) {
  std::string script =
      "if (document.title)"
      "  return document.title;"
      "else"
      "  return document.URL;";

  ListValue no_args;
  Value* unscoped_value = NULL;
  ErrorCode code = ExecuteScript(current_target_,
                                 script,
                                 &no_args,
                                 &unscoped_value);
  scoped_ptr<Value> value(unscoped_value);
  if (code != kSuccess)
    return code;
  if (!value->GetAsString(tab_title)) {
    LOG(ERROR) << "Script returned non-string type";
    return kUnknownError;
  }
  return kSuccess;
}

bool Session::MouseMoveAndClick(const gfx::Point& location,
                                automation::MouseButton button) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::MouseClick,
      current_target_.window_id,
      location,
      button,
      &success));
  if (success)
    mouse_position_ = location;
  return success;
}

bool Session::MouseMove(const gfx::Point& location) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::MouseMove,
      current_target_.window_id,
      location,
      &success));
  if (success)
    mouse_position_ = location;
  return success;
}

bool Session::MouseDrag(const gfx::Point& start,
                        const gfx::Point& end) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::MouseDrag,
      current_target_.window_id,
      start,
      end,
      &success));
  if (success)
    mouse_position_ = end;
  return success;
}

bool Session::MouseClick(automation::MouseButton button) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::MouseClick,
      current_target_.window_id,
      mouse_position_,
      button,
      &success));
  return success;
}

bool Session::MouseButtonDown() {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::MouseButtonDown,
      current_target_.window_id,
      mouse_position_,
      &success));
  return success;
}

bool Session::MouseButtonUp() {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::MouseButtonUp,
      current_target_.window_id,
      mouse_position_,
      &success));
  return success;
}

bool Session::MouseDoubleClick() {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::MouseDoubleClick,
      current_target_.window_id,
      mouse_position_,
      &success));
  return success;
}

bool Session::GetCookies(const std::string& url, ListValue** cookies) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GetCookies,
      url,
      cookies,
      &success));
  return success;
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

bool Session::DeleteCookie(const std::string& url,
                           const std::string& cookie_name) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::DeleteCookie,
      url,
      cookie_name,
      &success));
  return success;
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

bool Session::SetCookie(const std::string& url, DictionaryValue* cookie_dict) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::SetCookie,
      url,
      cookie_dict,
      &success));
  return success;
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

bool Session::GetWindowIds(std::vector<int>* window_ids) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GetTabIds,
      window_ids,
      &success));
  return success;
}

ErrorCode Session::SwitchToWindow(const std::string& name) {
  int switch_to_id = 0;
  int name_no = 0;
  if (base::StringToInt(name, &name_no)) {
    bool success = false;
    bool does_exist = false;
    RunSessionTask(NewRunnableMethod(
        automation_.get(),
        &Automation::DoesTabExist,
        name_no,
        &does_exist,
        &success));
    if (!success) {
      LOG(ERROR) << "Unable to determine if window exists";
      return kUnknownError;
    }
    if (does_exist)
      switch_to_id = name_no;
  }

  if (!switch_to_id) {
    std::vector<int> window_ids;
    GetWindowIds(&window_ids);
    // See if any of the window names match |name|.
    for (size_t i = 0; i < window_ids.size(); ++i) {
      ListValue empty_list;
      Value* unscoped_name_value;
      std::string window_name;
      ErrorCode code = ExecuteScript(FrameId(window_ids[i], FramePath()),
                                     "return window.name;",
                                     &empty_list,
                                     &unscoped_name_value);
      scoped_ptr<Value> name_value(unscoped_name_value);
      if (code == kSuccess &&
          name_value->GetAsString(&window_name) &&
          name == window_name) {
        switch_to_id = window_ids[i];
        break;
      }
    }
  }

  if (!switch_to_id)
    return kNoSuchWindow;
  current_target_ = FrameId(switch_to_id, FramePath());
  return kSuccess;
}

ErrorCode Session::SwitchToFrameWithNameOrId(const std::string& name_or_id) {
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

ErrorCode Session::SwitchToFrameWithIndex(int index) {
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

ErrorCode Session::SwitchToFrameWithElement(const WebElementId& element) {
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

void Session::SwitchToTopFrameIfCurrentFrameInvalid() {
  std::vector<std::string> components;
  current_target_.frame_path.GetComponents(&components);
  if (frame_elements_.size() != components.size()) {
    LOG(ERROR) << "Frame element vector out of sync with frame path";
    return;
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
    ErrorCode code = ExecuteScript(
        frame_id, "", &args, &unscoped_value);

    scoped_ptr<Value> value(unscoped_value);
    if (code == kStaleElementReference) {
      SwitchToTopFrame();
    } else if (code != kSuccess) {
      LOG(WARNING) << "Unable to determine if target frame should be reset";
      return;
    }
    frame_path = frame_path.Append(components[i]);
  }
}

bool Session::CloseWindow() {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::CloseTab,
      current_target_.window_id,
      &success));

  if (success) {
    std::vector<int> window_ids;
    if (!GetWindowIds(&window_ids) || window_ids.empty()) {
      // The automation connection will soon be closed, if not already,
      // because we supposedly just closed the last window. Terminate the
      // session.
      // TODO(kkania): This will cause us problems if GetWindowIds fails for a
      // reason other than the channel is disconnected. Look into having
      // |GetWindowIds| tell us if it just closed the last window.
      Terminate();
    }
  }
  return success;
}

ErrorCode Session::GetAlertMessage(std::string* text) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GetAppModalDialogMessage,
      text,
      &success));
  return success ? kSuccess : kUnknownError;
}

ErrorCode Session::SetAlertPromptText(const std::string& alert_prompt_text) {
  std::string message_text;
  // Only set the alert prompt text if an alert is actually active.
  ErrorCode code = GetAlertMessage(&message_text);
  if (code == kSuccess) {
    has_alert_prompt_text_ = true;
    alert_prompt_text_ = alert_prompt_text;
  }
  return code;
}

ErrorCode Session::AcceptOrDismissAlert(bool accept) {
  bool success = false;
  if (accept && has_alert_prompt_text_) {
    RunSessionTask(NewRunnableMethod(
        automation_.get(),
        &Automation::AcceptPromptAppModalDialog,
        alert_prompt_text_,
        &success));
  } else {
    RunSessionTask(NewRunnableMethod(
        automation_.get(),
        &Automation::AcceptOrDismissAppModalDialog,
        accept,
        &success));
  }
  has_alert_prompt_text_ = false;
  return success ? kSuccess : kUnknownError;
}

std::string Session::GetBrowserVersion() {
  std::string version;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GetBrowserVersion,
      &version));
  return version;
}

bool Session::CompareBrowserVersion(int client_build_no,
                                    int client_patch_no,
                                    bool* is_newer_or_equal) {
  std::string version = GetBrowserVersion();
  std::vector<std::string> split_version;
  base::SplitString(version, '.', &split_version);
  if (split_version.size() != 4)
    return false;
  int build_no, patch_no;
  if (!base::StringToInt(split_version[2], &build_no) ||
      !base::StringToInt(split_version[3], &patch_no)) {
    return false;
  }
  if (build_no < client_build_no)
    *is_newer_or_equal = false;
  else if (build_no > client_build_no)
    *is_newer_or_equal = true;
  else
    *is_newer_or_equal = patch_no >= client_patch_no;
  return true;
}

ErrorCode Session::FindElement(const FrameId& frame_id,
                               const WebElementId& root_element,
                               const std::string& locator,
                               const std::string& query,
                               WebElementId* element) {
  std::vector<WebElementId> elements;
  ErrorCode code = FindElementsHelper(
      frame_id, root_element, locator, query, true, &elements);
  if (code == kSuccess)
    *element = elements[0];
  return code;
}

ErrorCode Session::FindElements(const FrameId& frame_id,
                                const WebElementId& root_element,
                                const std::string& locator,
                                const std::string& query,
                                std::vector<WebElementId>* elements) {
  return FindElementsHelper(
      frame_id, root_element, locator, query, false, elements);
}

ErrorCode Session::GetElementLocationInView(
    const WebElementId& element, gfx::Point* location) {
  CHECK(element.is_valid());

  gfx::Size elem_size;
  ErrorCode code = GetElementSize(current_target_, element, &elem_size);
  if (code != kSuccess)
    return code;

  gfx::Point elem_offset(0, 0);
  code = GetLocationInViewHelper(
      current_target_, element,
      gfx::Rect(elem_offset, elem_size), &elem_offset);
  if (code != kSuccess)
    return code;

  for (FramePath frame_path = current_target_.frame_path;
       frame_path.IsSubframe();
       frame_path = frame_path.Parent()) {
    // Find the frame element for the current frame path.
    FrameId frame_id(current_target_.window_id, frame_path.Parent());
    WebElementId frame_element;
    code = FindElement(
        frame_id, WebElementId(""),
        LocatorType::kXpath, frame_path.BaseName().value(), &frame_element);
    if (code != kSuccess) {
      LOG(ERROR) << "Could not find frame element: "
                 << frame_path.BaseName().value()
                 << " in frame: " << frame_path.Parent().value();
      return code;
    }
    // Modify |elem_offset| by the frame's border.
    int border_left, border_top;
    code = GetElementBorder(
        frame_id, frame_element, &border_left, &border_top);
    if (code != kSuccess) {
      LOG(ERROR) << "Could not get frame border width";
      return code;
    }
    elem_offset.Offset(border_left, border_top);

    code = GetLocationInViewHelper(
        frame_id, frame_element,
        gfx::Rect(elem_offset, elem_size), &elem_offset);
    if (code != kSuccess)
      return code;
  }
  *location = elem_offset;
  return kSuccess;
}

ErrorCode Session::GetElementSize(const FrameId& frame_id,
                                  const WebElementId& element,
                                  gfx::Size* size) {
  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);", atoms::GET_SIZE);
  ListValue args;
  args.Append(element.ToValue());

  Value* unscoped_result = NULL;
  ErrorCode code = ExecuteScript(frame_id, script, &args, &unscoped_result);
  scoped_ptr<Value> result(unscoped_result);
  if (code != kSuccess)
    return code;
  if (!result->IsType(Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "GetSize atom returned non-dict type";
    return kUnknownError;
  }
  DictionaryValue* dict = static_cast<DictionaryValue*>(result.get());
  int width, height;
  if (!dict->GetInteger("width", &width) ||
      !dict->GetInteger("height", &height)) {
    LOG(ERROR) << "GetSize atom returned dict without width and height keys";
    return kUnknownError;
  }
  *size = gfx::Size(width, height);
  return kSuccess;
}

ErrorCode Session::GetElementEffectiveStyle(
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
  ErrorCode code = ExecuteScript(
      frame_id, script, &args, &unscoped_result);
  scoped_ptr<Value> result(unscoped_result);
  if (code != kSuccess) {
    LOG(ERROR) << "GetEffectiveStyle atom failed for property: " << prop;
    return code;
  }

  if (!result->GetAsString(value)) {
    LOG(ERROR) << "GetEffectiveStyle atom returned non-string type for "
               << "property: " << prop;
    return kUnknownError;
  }
  return kSuccess;
}

ErrorCode Session::GetElementBorder(const FrameId& frame_id,
                                    const WebElementId& element,
                                    int* border_left,
                                    int* border_top) {
  std::string border_left_str, border_top_str;
  ErrorCode code_left = GetElementEffectiveStyle(
      frame_id, element, "border-left-width", &border_left_str);
  ErrorCode code_top = GetElementEffectiveStyle(
      frame_id, element, "border-top-width", &border_top_str);
  if (code_left != kSuccess || code_top != kSuccess)
    return code_left;

  base::StringToInt(border_left_str, border_left);
  base::StringToInt(border_top_str, border_top);
  return kSuccess;
}

ErrorCode Session::IsElementDisplayed(const FrameId& frame_id,
                                      const WebElementId& element,
                                      bool* is_displayed) {
  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);", atoms::IS_DISPLAYED);
  ListValue args;
  args.Append(element.ToValue());

  Value* unscoped_result = NULL;
  ErrorCode code = ExecuteScript(frame_id, script, &args, &unscoped_result);
  scoped_ptr<Value> result(unscoped_result);
  if (code != kSuccess)
    return code;
  if (!result->GetAsBoolean(is_displayed)) {
    LOG(ERROR) << "IsDisplayed atom returned non boolean";
    return kUnknownError;
  }
  return kSuccess;
}

ErrorCode Session::IsElementEnabled(const FrameId& frame_id,
                                    const WebElementId& element,
                                    bool* is_enabled) {
  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);", atoms::IS_ENABLED);
  ListValue args;
  args.Append(element.ToValue());

  Value* unscoped_result = NULL;
  ErrorCode code = ExecuteScript(frame_id, script, &args, &unscoped_result);
  scoped_ptr<Value> result(unscoped_result);
  if (code != kSuccess)
    return code;
  if (!result->GetAsBoolean(is_enabled)) {
    LOG(ERROR) << "IsEnabled atom returned non boolean";
    return kUnknownError;
  }
  return kSuccess;
}

bool Session::WaitForAllTabsToStopLoading() {
  if (!automation_.get())
    return true;
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::WaitForAllTabsToStopLoading,
      &success));
  return success;
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

void Session::set_speed(Speed speed) {
  speed_ = speed;
}

Session::Speed Session::speed() const {
  return speed_;
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
                                  ErrorCode* code) {
  automation_.reset(new Automation());
  if (browser_exe.empty())
    automation_->Init(options, code);
  else
    automation_->InitWithBrowserPath(browser_exe, options, code);
  if (*code != kSuccess)
    return;

  bool success = false;
  std::vector<int> tab_ids;
  automation_->GetTabIds(&tab_ids, &success);
  if (!success) {
    LOG(ERROR) << "Could not get tab ids";
    *code = kUnknownError;
    return;
  }
  if (tab_ids.empty()) {
    LOG(ERROR) << "No tab ids after initialization";
    *code = kUnknownError;
    return;
  }
  current_target_ = FrameId(tab_ids[0], FramePath());
}

void Session::TerminateOnSessionThread() {
  if (automation_.get())
    automation_->Terminate();
  automation_.reset();
}

ErrorCode Session::ExecuteScriptAndParseResponse(const FrameId& frame_id,
                                                 const std::string& script,
                                                 Value** value) {

  // Should we also log the script that's being executed? It could be several KB
  // in size and will add lots of noise to the logs.
  VLOG(1) << "Executing script in frame: " << frame_id.frame_path.value();

  std::string result;
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::ExecuteScript,
      frame_id.window_id,
      frame_id.frame_path,
      script,
      &result,
      &success));
  if (!success) {
    LOG(ERROR) << "Automation failed to execute script";
    *value = Value::CreateStringValue(
        "Unknown internal script execution failure");
    return kUnknownError;
  }

  VLOG(1) << "...script result: " << result;
  scoped_ptr<Value> r(base::JSONReader::ReadAndReturnError(
      result, true, NULL, NULL));
  if (!r.get()) {
    LOG(ERROR) << "Failed to parse script result";
    *value = Value::CreateStringValue(
        "Internal script execution error: failed to parse script result");
    return kUnknownError;
  }

  if (r->GetType() != Value::TYPE_DICTIONARY) {
    LOG(ERROR) << "Execute script returned non-dictionary type";
    std::ostringstream stream;
    stream << "Internal script execution error: script result must be a "
           << print_valuetype(Value::TYPE_DICTIONARY) << ", but was "
           << print_valuetype(r->GetType()) << ": " << result;
    *value = Value::CreateStringValue(stream.str());
    return kUnknownError;
  }

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
  if (!result_dict->GetInteger("status", &status)) {
    NOTREACHED() << "...script did not return a status flag.";
  }
  return static_cast<ErrorCode>(status);
}

void Session::SendKeysOnSessionThread(const string16& keys, bool* success) {
  *success = true;
  std::vector<WebKeyEvent> key_events;
  std::string error_msg;
  if (!ConvertKeysToWebKeyEvents(keys, &key_events, &error_msg)) {
    // TODO(kkania): Return this message to the user.
    LOG(ERROR) << error_msg;
    *success = false;
    return;
  }
  for (size_t i = 0; i < key_events.size(); ++i) {
    bool key_success = false;
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
          &key_success);
    } else {
      automation_->SendWebKeyEvent(
          current_target_.window_id, key_events[i], &key_success);
    }
    if (!key_success) {
      LOG(ERROR) << "Failed to send key event. Event details:\n"
                 << "Type: " << key_events[i].type << "\n"
                 << "KeyCode: " << key_events[i].key_code << "\n"
                 << "UnmodifiedText: " << key_events[i].unmodified_text << "\n"
                 << "ModifiedText: " << key_events[i].modified_text << "\n"
                 << "Modifiers: " << key_events[i].modifiers << "\n";
      *success = false;
    }
  }
}

ErrorCode Session::SwitchToFrameWithJavaScriptLocatedFrame(
    const std::string& script,
    ListValue* args) {
  Value* unscoped_result = NULL;
  ErrorCode code = ExecuteScript(script, args, &unscoped_result);
  scoped_ptr<Value> result(unscoped_result);
  if (code != kSuccess)
    return code;

  ListValue* frame_and_xpath_list;
  if (!result->GetAsList(&frame_and_xpath_list))
    return kNoSuchFrame;
  DictionaryValue* element_dict;
  std::string xpath;
  if (!frame_and_xpath_list->GetDictionary(0, &element_dict) ||
      !frame_and_xpath_list->GetString(1, &xpath)) {
    LOG(ERROR) << "Frame finding script did not return correct type";
    return kUnknownError;
  }
  WebElementId new_frame_element(element_dict);
  if (!new_frame_element.is_valid()) {
    LOG(ERROR) << "Frame finding script did not return a frame element";
    return kUnknownError;
  }

  frame_elements_.push_back(new_frame_element);
  current_target_.frame_path = current_target_.frame_path.Append(xpath);
  return kSuccess;
}

ErrorCode Session::FindElementsHelper(const FrameId& frame_id,
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
  ErrorCode code = kUnknownError;
  bool done = false;
  while (!done) {
    Value* unscoped_value;
    code = ExecuteScript(frame_id, jscript, &jscript_args, &unscoped_value);
    value.reset(unscoped_value);
    if (code == kSuccess) {
      // If searching for many elements, make sure we found at least one before
      // stopping.
      done = find_one ||
             (value->GetType() == Value::TYPE_LIST &&
             static_cast<ListValue*>(value.get())->GetSize() > 0);
    } else if (code != kNoSuchElement) {
      return code;
    }
    int64 elapsed_time = (base::Time::Now() - start_time).InMilliseconds();
    done = done || elapsed_time > implicit_wait_;
    if (!done)
      base::PlatformThread::Sleep(50);  // Prevent a busy loop.
  }

  // Parse the results.
  if (code == kSuccess) {
    if (value->IsType(Value::TYPE_LIST)) {
      ListValue* element_list = static_cast<ListValue*>(value.get());
      for (size_t i = 0; i < element_list->GetSize(); ++i) {
        DictionaryValue* element_dict = NULL;
        if (!element_list->GetDictionary(i, &element_dict)) {
          LOG(ERROR) << "Not all elements were dictionaries";
          return kUnknownError;
        }
        WebElementId element(element_dict);
        if (!element.is_valid()) {
          LOG(ERROR) << "Not all elements were valid";
          return kUnknownError;
        }
        elements->push_back(element);
      }
    } else if (value->IsType(Value::TYPE_DICTIONARY)) {
      DictionaryValue* element_dict =
          static_cast<DictionaryValue*>(value.get());
      WebElementId element(element_dict);
      if (!element.is_valid()) {
        LOG(ERROR) << "Element was invalid";
        return kUnknownError;
      }
      elements->push_back(element);
    } else {
      LOG(ERROR) << "Invalid result type from find element atom";
      return kUnknownError;
    }
  }
  return code;
}

ErrorCode Session::GetLocationInViewHelper(const FrameId& frame_id,
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
  ErrorCode code = ExecuteScript(frame_id, jscript, &jscript_args,
                                 &unscoped_value);
  scoped_ptr<Value> value(unscoped_value);
  if (code != kSuccess)
    return code;
  if (!value->IsType(Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "Location atom returned non-dictionary type";
    code = kUnknownError;
  }
  DictionaryValue* loc_dict = static_cast<DictionaryValue*>(value.get());
  int x = 0, y = 0;
  if (!loc_dict->GetInteger("x", &x) ||
      !loc_dict->GetInteger("y", &y)) {
    LOG(ERROR) << "Location atom returned bad coordinate dictionary";
    code = kUnknownError;
  }
  *location = gfx::Point(x, y);
  return kSuccess;
}

bool Session::GetScreenShot(std::string* png) {
  bool success = false;
  ScopedTempDir screenshots_dir;

  // Create a temp directory for screenshots.
  if (!screenshots_dir.CreateUniqueTempDir()) {
    return false;
  }

  FilePath path = screenshots_dir.path().AppendASCII("screen");

  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::CaptureEntirePageAsPNG,
      current_target_.window_id,
      path,
      &success));

  if (success) {
    success = file_util::ReadFileToString(path, png);
  }
  return success;
}

const gfx::Point& Session::get_mouse_position() const {
  return mouse_position_;
}

}  // namespace webdriver
