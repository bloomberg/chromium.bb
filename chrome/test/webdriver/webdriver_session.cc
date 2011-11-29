// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_session.h"

#include <sstream>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
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
#include "chrome/test/automation/value_conversion_util.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_key_converter.h"
#include "chrome/test/webdriver/webdriver_session_manager.h"
#include "chrome/test/webdriver/webdriver_util.h"
#include "third_party/webdriver/atoms.h"

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

Session::Options::Options()
    : use_native_events(false),
      load_async(false) {
}

Session::Options::~Options() {
}

Session::Session(const Options& options)
    : id_(GenerateRandomID()),
      current_target_(FrameId(0, FramePath())),
      thread_(id_.c_str()),
      async_script_timeout_(0),
      implicit_wait_(0),
      has_alert_prompt_text_(false),
      options_(options) {
  SessionManager::GetInstance()->Add(this);
}

Session::~Session() {
  SessionManager::GetInstance()->Remove(id_);
}

Error* Session::Init(const Automation::BrowserOptions& options) {
  if (!thread_.Start()) {
    delete this;
    return new Error(kUnknownError, "Cannot start session thread");
  }

  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      this,
      &Session::InitOnSessionThread,
      options,
      &error));
  if (error)
    Terminate();
  return error;
}

Error* Session::BeforeExecuteCommand() {
  Error* error = AfterExecuteCommand();
  if (!error) {
    scoped_ptr<Error> switch_error(SwitchToTopFrameIfCurrentFrameInvalid());
    if (switch_error.get()) {
      std::string text;
      scoped_ptr<Error> alert_error(GetAlertMessage(&text));
      if (alert_error.get()) {
        // Only return a frame checking error if a modal dialog is not present.
        // TODO(kkania): This is ugly. Fix.
        return switch_error.release();
      }
    }
  }
  return error;
}

Error* Session::AfterExecuteCommand() {
  Error* error = NULL;
  if (!options_.load_async) {
    LOG(INFO) << "Waiting for the page to stop loading";
    error = WaitForAllTabsToStopLoading();
    LOG(INFO) << "Done waiting for the page to stop loading";
  }
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
      atoms::asString(atoms::EXECUTE_SCRIPT).c_str(), script.c_str(),
      args_as_json.c_str());

  return ExecuteScriptAndParseValue(frame_id, jscript, value);
}

Error* Session::ExecuteScript(const std::string& script,
                              const ListValue* const args,
                              Value** value) {
  return ExecuteScript(current_target_, script, args, value);
}

Error* Session::ExecuteScriptAndParse(const FrameId& frame_id,
                                      const std::string& anonymous_func_script,
                                      const std::string& script_name,
                                      const ListValue* args,
                                      const ValueParser* parser) {
  scoped_ptr<const ListValue> scoped_args(args);
  scoped_ptr<const ValueParser> scoped_parser(parser);
  std::string called_script = base::StringPrintf(
      "return (%s).apply(null, arguments);", anonymous_func_script.c_str());
  Value* unscoped_value = NULL;
  Error* error = ExecuteScript(frame_id, called_script, args, &unscoped_value);
  if (error) {
    error->AddDetails(script_name + " execution failed");
    return error;
  }

  scoped_ptr<Value> value(unscoped_value);
  std::string error_msg;
  if (!parser->Parse(value.get())) {
    error_msg = base::StringPrintf("%s returned invalid value: %s",
        script_name.c_str(), JsonStringify(value.get()).c_str());
    return new Error(kUnknownError, error_msg);
  }
  return NULL;
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
      atoms::asString(atoms::EXECUTE_ASYNC_SCRIPT).c_str(),
      script.c_str(),
      args_as_json.c_str(),
      timeout_ms,
      "function(result) {window.domAutomationController.send(result);}");

  return ExecuteScriptAndParseValue(frame_id, jscript, value);
}

Error* Session::SendKeys(const ElementId& element, const string16& keys) {
  bool is_displayed = false;
  Error* error = IsElementDisplayed(
      current_target_, element, true /* ignore_opacity */, &is_displayed);
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

  // Focus the target element in order to send keys to it.
  // First, the currently active element is blurred, if it is different from
  // the target element. We do not want to blur an element unnecessarily,
  // because this may cause us to lose the current cursor position in the
  // element.
  // Secondly, we focus the target element.
  // Thirdly, if the target element is newly focused and is a text input, we
  // set the cursor position at the end.
  // Fourthly, we check if the new active element is the target element. If not,
  // we throw an error.
  // Additional notes:
  //   - |document.activeElement| is the currently focused element, or body if
  //     no element is focused
  //   - Even if |document.hasFocus()| returns true and the active element is
  //     the body, sometimes we still need to focus the body element for send
  //     keys to work. Not sure why
  //   - You cannot focus a descendant of a content editable node
  // TODO(jleyba): Update this to use the correct atom.
  const char* kFocusScript =
      "function(elem) {"
      "  var doc = elem.ownerDocument || elem;"
      "  var prevActiveElem = doc.activeElement;"
      "  if (elem != prevActiveElem && prevActiveElem)"
      "    prevActiveElem.blur();"
      "  elem.focus();"
      "  if (elem != prevActiveElem && elem.value && elem.value.length &&"
      "      elem.setSelectionRange) {"
      "    elem.setSelectionRange(elem.value.length, elem.value.length);"
      "  }"
      "  if (elem != doc.activeElement)"
      "    throw new Error('Failed to send keys because cannot focus element');"
      "}";
  error = ExecuteScriptAndParse(current_target_,
                                kFocusScript,
                                "focusElement",
                                CreateListValueFrom(element),
                                CreateDirectValueParser(kSkipParsing));
  if (error)
    return error;

  RunSessionTask(NewRunnableMethod(
      this,
      &Session::SendKeysOnSessionThread,
      keys,
      &error));
  return error;
}

Error* Session::DragAndDropFilePaths(
    const Point& location,
    const std::vector<FilePath::StringType>& paths) {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::DragAndDropFilePaths,
      current_target_.window_id,
      location,
      paths,
      &error));
  return error;
}

Error* Session::NavigateToURL(const std::string& url) {
  Error* error = NULL;
  if (options_.load_async) {
    RunSessionTask(NewRunnableMethod(
        automation_.get(),
        &Automation::NavigateToURLAsync,
        current_target_.window_id,
        url,
        &error));
  } else {
    RunSessionTask(NewRunnableMethod(
        automation_.get(),
        &Automation::NavigateToURL,
        current_target_.window_id,
        url,
        &error));
  }
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
  return ExecuteScriptAndParse(current_target_,
                               "function() { return document.URL }",
                               "getUrl",
                               new ListValue(),
                               CreateDirectValueParser(url));
}

Error* Session::GetTitle(std::string* tab_title) {
  const char* kGetTitleScript =
      "function() {"
      "  if (document.title)"
      "    return document.title;"
      "  else"
      "    return document.URL;"
      "}";
  return ExecuteScriptAndParse(FrameId(current_target_.window_id, FramePath()),
                               kGetTitleScript,
                               "getTitle",
                               new ListValue(),
                               CreateDirectValueParser(tab_title));
}

Error* Session::MouseMoveAndClick(const Point& location,
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

Error* Session::MouseMove(const Point& location) {
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

Error* Session::MouseDrag(const Point& start,
                          const Point& end) {
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
  return MouseMoveAndClick(mouse_position_, button);
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
      std::string window_name;
      Error* error = ExecuteScriptAndParse(
          FrameId(window_ids[i], FramePath()),
          "function() { return window.name; }",
          "getWindowName",
          new ListValue(),
          CreateDirectValueParser(&window_name));
      if (error)
        return error;
      if (name == window_name) {
        switch_to_id = window_ids[i];
        break;
      }
    }
  }

  if (!switch_to_id)
    return new Error(kNoSuchWindow);
  frame_elements_.clear();
  current_target_ = FrameId(switch_to_id, FramePath());
  return NULL;
}

Error* Session::SwitchToFrameWithNameOrId(const std::string& name_or_id) {
  std::string script =
      "function(arg) {"
      "  var xpath = '(/html/body//iframe|/html/frameset/frame)';"
      "  var sub = function(s) { return s.replace(/\\$/g, arg); };"
      "  xpath += sub('[@name=\"$\" or @id=\"$\"]');"
      "  var frame = document.evaluate(xpath, document, null, "
      "      XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;"
      "  if (!frame) { return null; }"
      "  xpath = frame.tagName == 'IFRAME' ? '/html/body//iframe'"
      "                                    : '/html/frameset/frame';"
      "  frame_xpath = xpath + "
      "      sub('[@' + (frame.id == arg ? 'id' : 'name') + '=\"$\"]');"
      "  return [frame, frame_xpath];"
      "}";
  return SwitchToFrameWithJavaScriptLocatedFrame(
      script, CreateListValueFrom(name_or_id));
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
      "function(index) {"
      "  var xpathIndex = '[' + (index + 1) + ']';"
      "  var xpath = '(/html/body//iframe|/html/frameset/frame)' + "
      "              xpathIndex;"
      "  var frame = document.evaluate(xpath, document, null, "
      "  XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;"
      "  if (!frame) { return null; }"
      "  frame_xpath = ((frame.tagName == 'IFRAME' ? "
      "      '(/html/body//iframe)' : '/html/frameset/frame') + xpathIndex);"
      "  return [frame, frame_xpath];"
      "}";
  return SwitchToFrameWithJavaScriptLocatedFrame(
      script, CreateListValueFrom(index));
}

Error* Session::SwitchToFrameWithElement(const ElementId& element) {
  // TODO(jleyba): Extract this, and the other frame switch methods to an atom.
  std::string script =
      "function(elem) {"
      "  if (elem.nodeType != 1 || !/^i?frame$/i.test(elem.tagName)) {"
      "    console.error('Element is not a frame');"
      "    return null;"
      "  }"
      "  for (var i = 0; i < window.frames.length; i++) {"
      "    if (elem.contentWindow == window.frames[i]) {"
      "      return [elem, '(//iframe|//frame)[' + (i + 1) + ']'];"
      "    }"
      "  }"
      "  console.info('Frame is not connected to this DOM tree');"
      "  return null;"
      "}";
  return SwitchToFrameWithJavaScriptLocatedFrame(
      script, CreateListValueFrom(element));
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
  // Start from the root path and check that each frame element that makes
  // up the current frame target is valid by executing an empty script.
  // This code should not execute script in any frame before making sure the
  // frame element is valid, otherwise the automation hangs until a timeout.
  for (size_t i = 0; i < frame_elements_.size(); ++i) {
    FrameId frame_id(current_target_.window_id, frame_path);
    scoped_ptr<Error> error(ExecuteScriptAndParse(
        frame_id,
        "function(){ }",
        "emptyScript",
        CreateListValueFrom(frame_elements_[i]),
        CreateDirectValueParser(kSkipParsing)));
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
                            const ElementId& root_element,
                            const std::string& locator,
                            const std::string& query,
                            ElementId* element) {
  std::vector<ElementId> elements;
  Error* error = FindElementsHelper(
      frame_id, root_element, locator, query, true, &elements);
  if (!error)
    *element = elements[0];
  return error;
}

Error* Session::FindElements(const FrameId& frame_id,
                             const ElementId& root_element,
                             const std::string& locator,
                             const std::string& query,
                             std::vector<ElementId>* elements) {
  return FindElementsHelper(
      frame_id, root_element, locator, query, false, elements);
}

Error* Session::GetElementLocationInView(
    const ElementId& element,
    Point* location) {
  Size size;
  Error* error = GetElementSize(current_target_, element, &size);
  if (error)
    return error;
  return GetElementRegionInView(
      element, Rect(Point(0, 0), size),
      false /* center */, false /* verify_clickable_at_middle */, location);
}

Error* Session::GetElementRegionInView(
    const ElementId& element,
    const Rect& region,
    bool center,
    bool verify_clickable_at_middle,
    Point* location) {
  CHECK(element.is_valid());

  Point region_offset = region.origin();
  Size region_size = region.size();
  Error* error = GetElementRegionInViewHelper(
      current_target_, element, region, center, verify_clickable_at_middle,
      &region_offset);
  if (error)
    return error;

  for (FramePath frame_path = current_target_.frame_path;
       frame_path.IsSubframe();
       frame_path = frame_path.Parent()) {
    // Find the frame element for the current frame path.
    FrameId frame_id(current_target_.window_id, frame_path.Parent());
    ElementId frame_element;
    error = FindElement(
        frame_id, ElementId(""),
        LocatorType::kXpath, frame_path.BaseName().value(), &frame_element);
    if (error) {
      std::string context = base::StringPrintf(
          "Could not find frame element (%s) in frame (%s)",
          frame_path.BaseName().value().c_str(),
          frame_path.Parent().value().c_str());
      error->AddDetails(context);
      return error;
    }
    // Modify |region_offset| by the frame's border.
    int border_left, border_top;
    error = GetElementBorder(
        frame_id, frame_element, &border_left, &border_top);
    if (error)
      return error;
    region_offset.Offset(border_left, border_top);

    error = GetElementRegionInViewHelper(
        frame_id, frame_element, Rect(region_offset, region_size),
        center, verify_clickable_at_middle, &region_offset);
    if (error)
      return error;
  }
  *location = region_offset;
  return NULL;
}

Error* Session::GetElementSize(const FrameId& frame_id,
                               const ElementId& element,
                               Size* size) {
  return ExecuteScriptAndParse(
      frame_id,
      atoms::asString(atoms::GET_SIZE),
      "getSize",
      CreateListValueFrom(element),
      CreateDirectValueParser(size));
}

Error* Session::GetElementFirstClientRect(const FrameId& frame_id,
                                          const ElementId& element,
                                          Rect* rect) {
  return ExecuteScriptAndParse(
      frame_id,
      atoms::asString(atoms::GET_FIRST_CLIENT_RECT),
      "getFirstClientRect",
      CreateListValueFrom(element),
      CreateDirectValueParser(rect));
}

Error* Session::GetElementEffectiveStyle(
    const FrameId& frame_id,
    const ElementId& element,
    const std::string& prop,
    std::string* value) {
  return ExecuteScriptAndParse(
      frame_id,
      atoms::asString(atoms::GET_EFFECTIVE_STYLE),
      "getEffectiveStyle",
      CreateListValueFrom(element, prop),
      CreateDirectValueParser(value));
}

Error* Session::GetElementBorder(const FrameId& frame_id,
                                 const ElementId& element,
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
                                   const ElementId& element,
                                   bool ignore_opacity,
                                   bool* is_displayed) {
  return ExecuteScriptAndParse(
      frame_id,
      atoms::asString(atoms::IS_DISPLAYED),
      "isDisplayed",
      CreateListValueFrom(element, ignore_opacity),
      CreateDirectValueParser(is_displayed));
}

Error* Session::IsElementEnabled(const FrameId& frame_id,
                                 const ElementId& element,
                                 bool* is_enabled) {
  return ExecuteScriptAndParse(
      frame_id,
      atoms::asString(atoms::IS_ENABLED),
      "isEnabled",
      CreateListValueFrom(element),
      CreateDirectValueParser(is_enabled));
}

Error* Session::IsOptionElementSelected(const FrameId& frame_id,
                                        const ElementId& element,
                                        bool* is_selected) {
  return ExecuteScriptAndParse(
      frame_id,
      atoms::asString(atoms::IS_SELECTED),
      "isSelected",
      CreateListValueFrom(element),
      CreateDirectValueParser(is_selected));
}

Error* Session::SetOptionElementSelected(const FrameId& frame_id,
                                         const ElementId& element,
                                         bool selected) {
  return ExecuteScriptAndParse(
      frame_id,
      atoms::asString(atoms::SET_SELECTED),
      "setSelected",
      CreateListValueFrom(element, selected),
      CreateDirectValueParser(kSkipParsing));
}

Error* Session::ToggleOptionElement(const FrameId& frame_id,
                                    const ElementId& element) {
  bool is_selected;
  Error* error = IsOptionElementSelected(frame_id, element, &is_selected);
  if (error)
    return error;

  return SetOptionElementSelected(frame_id, element, !is_selected);
}

Error* Session::GetElementTagName(const FrameId& frame_id,
                                  const ElementId& element,
                                  std::string* tag_name) {
  return ExecuteScriptAndParse(
      frame_id,
      "function(elem) { return elem.tagName.toLowerCase() }",
      "getElementTagName",
      CreateListValueFrom(element),
      CreateDirectValueParser(tag_name));
}

Error* Session::GetClickableLocation(const ElementId& element,
                                     Point* location) {
  bool is_displayed = false;
  Error* error = IsElementDisplayed(
      current_target_, element, true /* ignore_opacity */, &is_displayed);
  if (error)
    return error;
  if (!is_displayed)
    return new Error(kElementNotVisible, "Element must be displayed to click");

  // We try 3 methods to determine clickable location. This mostly follows
  // what FirefoxDriver does. Try the first client rect, then the bounding
  // client rect, and lastly the size of the element (via closure).
  // SVG is one case that doesn't have a first client rect.
  Rect rect;
  scoped_ptr<Error> ignore_error(
      GetElementFirstClientRect(current_target_, element, &rect));
  if (ignore_error.get()) {
    Rect client_rect;
    ignore_error.reset(ExecuteScriptAndParse(
        current_target_,
        "function(elem) { return elem.getBoundingClientRect() }",
        "getBoundingClientRect",
        CreateListValueFrom(element),
        CreateDirectValueParser(&client_rect)));
    rect = Rect(0, 0, client_rect.width(), client_rect.height());
  }
  if (ignore_error.get()) {
    Size size;
    ignore_error.reset(GetElementSize(current_target_, element, &size));
    rect = Rect(0, 0, size.width(), size.height());
  }
  if (ignore_error.get()) {
    return new Error(kUnknownError,
                     "Unable to determine clickable location of element");
  }

  error = GetElementRegionInView(
      element, rect, true /* center */, true /* verify_clickable_at_middle */,
      location);
  if (error)
    return error;
  location->Offset(rect.width() / 2, rect.height() / 2);
  return NULL;
}

Error* Session::GetAttribute(const ElementId& element,
                             const std::string& key,
                             Value** value) {
  return ExecuteScriptAndParse(
      current_target_,
      atoms::asString(atoms::GET_ATTRIBUTE),
      "getAttribute",
      CreateListValueFrom(element, key),
      CreateDirectValueParser(value));
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

Error* Session::InstallExtensionDeprecated(const FilePath& path) {
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::InstallExtensionDeprecated,
      path,
      &error));
  return error;
}

Error* Session::GetInstalledExtensions(
    std::vector<std::string>* extension_ids) {
  Error* error = NULL;
  RunSessionTask(base::Bind(
      &Automation::GetInstalledExtensions,
      base::Unretained(automation_.get()),
      extension_ids,
      &error));
  return error;
}

Error* Session::InstallExtension(
    const FilePath& path, std::string* extension_id) {
  Error* error = NULL;
  RunSessionTask(base::Bind(
      &Automation::InstallExtension,
      base::Unretained(automation_.get()),
      path,
      extension_id,
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

const Point& Session::get_mouse_position() const {
  return mouse_position_;
}

const Session::Options& Session::options() const {
  return options_;
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

void Session::RunSessionTask(const base::Closure& task) {
  base::WaitableEvent done_event(false, false);
  thread_.message_loop_proxy()->PostTask(FROM_HERE, base::Bind(
      &Session::RunClosureOnSessionThread,
      base::Unretained(this),
      task,
      &done_event));
  done_event.Wait();
}

void Session::RunClosureOnSessionThread(const base::Closure& task,
                                        base::WaitableEvent* done_event) {
  task.Run();
  done_event->Signal();
}

void Session::InitOnSessionThread(const Automation::BrowserOptions& options,
                                  Error** error) {
  automation_.reset(new Automation());
  automation_->Init(options, error);
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

Error* Session::ExecuteScriptAndParseValue(const FrameId& frame_id,
                                           const std::string& script,
                                           Value** script_result) {
  std::string response_json;
  Error* error = NULL;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::ExecuteScript,
      frame_id.window_id,
      frame_id.frame_path,
      script,
      &response_json,
      &error));
  if (error)
    return error;

  scoped_ptr<Value> value(base::JSONReader::ReadAndReturnError(
      response_json, true, NULL, NULL));
  if (!value.get())
    return new Error(kUnknownError, "Failed to parse script result");
  if (value->GetType() != Value::TYPE_DICTIONARY)
    return new Error(kUnknownError, "Execute script returned non-dict: " +
                         JsonStringify(value.get()));
  DictionaryValue* result_dict = static_cast<DictionaryValue*>(value.get());

  int status;
  if (!result_dict->GetInteger("status", &status))
    return new Error(kUnknownError, "Execute script did not return status: " +
                         JsonStringify(result_dict));
  ErrorCode code = static_cast<ErrorCode>(status);
  if (code != kSuccess) {
    DictionaryValue* error_dict;
    std::string error_msg;
    if (result_dict->GetDictionary("value", &error_dict))
      error_dict->GetString("message", &error_msg);
    if (error_msg.empty())
      error_msg = "Script failed with error code: " + base::IntToString(code);
    return new Error(code, error_msg);
  }

  Value* tmp;
  if (result_dict->Get("value", &tmp)) {
    *script_result= tmp->DeepCopy();
  } else {
    // "value" was not defined in the returned dictionary; set to null.
    *script_result= Value::CreateNullValue();
  }
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
    if (options_.use_native_events) {
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
  class SwitchFrameValueParser : public ValueParser {
   public:
    SwitchFrameValueParser(
        bool* found_frame, ElementId* frame, std::string* xpath)
        : found_frame_(found_frame), frame_(frame), xpath_(xpath) { }

    virtual ~SwitchFrameValueParser() { }

    virtual bool Parse(base::Value* value) const OVERRIDE {
      if (value->IsType(Value::TYPE_NULL)) {
        *found_frame_ = false;
        return true;
      }
      ListValue* list;
      if (!value->GetAsList(&list))
        return false;
      *found_frame_ = true;
      return SetFromListValue(list, frame_, xpath_);
    }

   private:
    bool* found_frame_;
    ElementId* frame_;
    std::string* xpath_;
  };

  bool found_frame;
  ElementId new_frame_element;
  std::string xpath;
  Error* error = ExecuteScriptAndParse(
      current_target_, script, "switchFrame", args,
      new SwitchFrameValueParser(&found_frame, &new_frame_element, &xpath));
  if (error)
    return error;

  if (!found_frame)
    return new Error(kNoSuchFrame);

  frame_elements_.push_back(new_frame_element);
  current_target_.frame_path = current_target_.frame_path.Append(xpath);
  return NULL;
}

Error* Session::FindElementsHelper(const FrameId& frame_id,
                                   const ElementId& root_element,
                                   const std::string& locator,
                                   const std::string& query,
                                   bool find_one,
                                   std::vector<ElementId>* elements) {
  CHECK(root_element.is_valid());
  base::Time start_time = base::Time::Now();
  while (true) {
    std::vector<ElementId> temp_elements;
    Error* error = ExecuteFindElementScriptAndParse(
        frame_id, root_element, locator, query, find_one, &temp_elements);
    if (error)
      return error;

    if (temp_elements.size() > 0u) {
      elements->swap(temp_elements);
      break;
    }

    if ((base::Time::Now() - start_time).InMilliseconds() > implicit_wait_) {
      if (find_one)
        return new Error(kNoSuchElement);
      break;
    }
    base::PlatformThread::Sleep(50);
  }
  return NULL;
}

Error* Session::ExecuteFindElementScriptAndParse(
    const FrameId& frame_id,
    const ElementId& root_element,
    const std::string& locator,
    const std::string& query,
    bool find_one,
    std::vector<ElementId>* elements) {
  CHECK(root_element.is_valid());

  class FindElementsParser : public ValueParser {
   public:
    explicit FindElementsParser(std::vector<ElementId>* elements)
        : elements_(elements) { }

    virtual ~FindElementsParser() { }

    virtual bool Parse(base::Value* value) const OVERRIDE {
      if (!value->IsType(Value::TYPE_LIST))
        return false;
      ListValue* list = static_cast<ListValue*>(value);
      for (size_t i = 0; i < list->GetSize(); ++i) {
        ElementId element;
        Value* element_value = NULL;
        if (!list->Get(i, &element_value))
          return false;
        if (!SetFromValue(element_value, &element))
          return false;
        elements_->push_back(element);
      }
      return true;
    }
   private:
    std::vector<ElementId>* elements_;
  };

  class FindElementParser : public ValueParser {
   public:
    explicit FindElementParser(std::vector<ElementId>* elements)
        : elements_(elements) { }

    virtual ~FindElementParser() { }

    virtual bool Parse(base::Value* value) const OVERRIDE {
      if (value->IsType(Value::TYPE_NULL))
        return true;
      ElementId element;
      bool set = SetFromValue(value, &element);
      if (set)
        elements_->push_back(element);
      return set;
    }
   private:
    std::vector<ElementId>* elements_;
  };

  DictionaryValue locator_dict;
  locator_dict.SetString(locator, query);
  std::vector<ElementId> temp_elements;
  Error* error = NULL;
  if (find_one) {
    error = ExecuteScriptAndParse(
          frame_id,
          atoms::asString(atoms::FIND_ELEMENT),
          "findElement",
          CreateListValueFrom(&locator_dict, root_element),
          new FindElementParser(&temp_elements));
  } else {
    error = ExecuteScriptAndParse(
          frame_id,
          atoms::asString(atoms::FIND_ELEMENTS),
          "findElements",
          CreateListValueFrom(&locator_dict, root_element),
          new FindElementsParser(&temp_elements));
  }
  if (!error)
    elements->swap(temp_elements);
  return error;
}

Error* Session::VerifyElementIsClickable(
    const FrameId& frame_id,
    const ElementId& element,
    const Point& location) {
  class IsElementClickableParser : public ValueParser {
   public:
    IsElementClickableParser(bool* clickable, std::string* message)
        : clickable_(clickable), message_(message) { }

    virtual ~IsElementClickableParser() { }

    virtual bool Parse(base::Value* value) const OVERRIDE {
      if (!value->IsType(Value::TYPE_DICTIONARY))
        return false;
      DictionaryValue* dict = static_cast<DictionaryValue*>(value);
      dict->GetString("message", message_);
      return dict->GetBoolean("clickable", clickable_);
    }

   private:
    bool* clickable_;
    std::string* message_;
  };

  bool clickable;
  std::string message;
  Error* error = ExecuteScriptAndParse(
      frame_id,
      atoms::asString(atoms::IS_ELEMENT_CLICKABLE),
      "isElementClickable",
      CreateListValueFrom(element, location),
      new IsElementClickableParser(&clickable, &message));
  if (error)
    return error;

  if (!clickable) {
    if (message.empty())
      message = "element is not clickable";
    return new Error(kUnknownError, message);
  }
  if (message.length()) {
    LOG(WARNING) << message;
  }
  return NULL;
}

Error* Session::GetElementRegionInViewHelper(
    const FrameId& frame_id,
    const ElementId& element,
    const Rect& region,
    bool center,
    bool verify_clickable_at_middle,
    Point* location) {
  Point temp_location;
  Error* error = ExecuteScriptAndParse(
      frame_id,
      atoms::asString(atoms::GET_LOCATION_IN_VIEW),
      "getLocationInView",
      CreateListValueFrom(element, center, region),
      CreateDirectValueParser(&temp_location));

  if (verify_clickable_at_middle) {
    Point middle_point = temp_location;
    middle_point.Offset(region.width() / 2, region.height() / 2);
    error = VerifyElementIsClickable(frame_id, element, middle_point);
    if (error)
      return error;
  }
  *location = temp_location;
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

Error* Session::GetBrowserConnectionState(bool* online) {
  return ExecuteScriptAndParse(
      current_target_,
      atoms::asString(atoms::IS_ONLINE),
      "isOnline",
      new ListValue(),
      CreateDirectValueParser(online));
}

Error* Session::GetAppCacheStatus(int* status) {
  return ExecuteScriptAndParse(
      current_target_,
      atoms::asString(atoms::GET_APPCACHE_STATUS),
      "getAppcacheStatus",
      new ListValue(),
      CreateDirectValueParser(status));
}

}  // namespace webdriver
