// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/session_manager.h"

#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/test_launcher_utils.h"
#include "chrome/test/webdriver/session_manager.h"
#include "chrome/test/webdriver/utility_functions.h"
#include "chrome/test/webdriver/webdriver_key_converter.h"
#include "googleurl/src/gurl.h"
#include "third_party/webdriver/atoms.h"

namespace webdriver {

Session::Session()
    : id_(GenerateRandomID()),
      thread_(id_.c_str()),
      implicit_wait_(0),
      current_frame_xpath_(""),
      current_window_id_(0) {
  SessionManager::GetInstance()->Add(this);
}

Session::~Session() {
  SessionManager::GetInstance()->Remove(id_);
}

bool Session::Init() {
  bool success = false;
  if (thread_.Start()) {
    RunSessionTask(NewRunnableMethod(
        this,
        &Session::InitOnSessionThread,
        &success));
  } else {
    LOG(ERROR) << "Cannot start session thread";
  }
  if (!success)
    delete this;
  return success;
}

void Session::Terminate() {
  RunSessionTask(NewRunnableMethod(
      this,
      &Session::TerminateOnSessionThread));
  delete this;
}

ErrorCode Session::ExecuteScript(int window_id,
                                 const std::string& frame_xpath,
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

  // Should we also log the script that's being executed? It could be several KB
  // in size and will add lots of noise to the logs.
  VLOG(1) << "Executing script in frame: " << current_frame_xpath_;

  std::string result;
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::ExecuteScript,
      window_id,
      frame_xpath,
      jscript,
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
    // "value" was not defined in the returned dictionary, set to null.
    *value = Value::CreateNullValue();
  }

  int status;
  if (!result_dict->GetInteger("status", &status)) {
    NOTREACHED() << "...script did not return a status flag.";
  }
  return static_cast<ErrorCode>(status);
}

ErrorCode Session::ExecuteScript(const std::string& script,
                                 const ListValue* const args,
                                 Value** value) {
  return ExecuteScript(
      current_window_id_, current_frame_xpath_, script, args, value);
}

ErrorCode Session::SendKeys(DictionaryValue* element, const string16& keys) {
  ListValue args;
  args.Append(element);
  // TODO(jleyba): Update this to use the correct atom.
  std::string script = "document.activeElement.blur();arguments[0].focus();";
  Value* unscoped_result = NULL;
  ErrorCode code = ExecuteScript(script, &args, &unscoped_result);
  scoped_ptr<Value> result(unscoped_result);
  if (code != kSuccess) {
    LOG(ERROR) << "Failed to focus element before sending keys";
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
      current_window_id_,
      url,
      &success));
  return success;
}

bool Session::GoForward() {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GoForward,
      current_window_id_,
      &success));
  return success;
}

bool Session::GoBack() {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GoBack,
      current_window_id_,
      &success));
  return success;
}

bool Session::Reload() {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::Reload,
      current_window_id_,
      &success));
  return success;
}

bool Session::GetURL(std::string* url) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GetURL,
      current_window_id_,
      url,
      &success));
  return success;
}

bool Session::GetURL(GURL* gurl) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GetGURL,
      current_window_id_,
      gurl,
      &success));
  return success;
}

bool Session::GetTabTitle(std::string* tab_title) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GetTabTitle,
      current_window_id_,
      tab_title,
      &success));
  return success;
}

bool Session::GetCookies(const GURL& url, std::string* cookies) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GetCookies,
      current_window_id_,
      url,
      cookies,
      &success));
  return success;
}

bool Session::GetCookieByName(const GURL& url,
                              const std::string& cookie_name,
                              std::string* cookie) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::GetCookieByName,
      current_window_id_,
      url,
      cookie_name,
      cookie,
      &success));
  return success;
}

bool Session::DeleteCookie(const GURL& url, const std::string& cookie_name) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::DeleteCookie,
      current_window_id_,
      url,
      cookie_name,
      &success));
  return success;
}

bool Session::SetCookie(const GURL& url, const std::string& cookie) {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::SetCookie,
      current_window_id_,
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
    bool does_exist = false;
    RunSessionTask(NewRunnableMethod(
        automation_.get(),
        &Automation::DoesTabExist,
        name_no,
        &does_exist));
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
      ErrorCode code = ExecuteScript(window_ids[i],
                                     "",
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
  current_window_id_ = switch_to_id;
  current_frame_xpath_ = "";
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
      "return xpath + sub('[@' + (frame.id == arg ? 'id' : 'name')"
      "                   + '=\"$\"]');";
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
      "return frame == null ? null : ((frame.tagName == 'IFRAME' ? "
      "    '/html/body//iframe' : '/html/frameset/frame') + index);";
  ListValue args;
  args.Append(Value::CreateIntegerValue(index));
  return SwitchToFrameWithJavaScriptLocatedFrame(script, &args);
}

bool Session::CloseWindow() {
  bool success = false;
  RunSessionTask(NewRunnableMethod(
      automation_.get(),
      &Automation::CloseTab,
      current_window_id_,
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

void Session::InitOnSessionThread(bool* success) {
  automation_.reset(new Automation());
  automation_->Init(success);
  if (!*success)
    return;

  std::vector<int> tab_ids;
  automation_->GetTabIds(&tab_ids, success);
  if (!*success) {
    LOG(ERROR) << "Could not get tab ids";
    return;
  }
  if (tab_ids.empty()) {
    LOG(ERROR) << "No tab ids after initialization";
    *success = false;
  } else {
    current_window_id_ = tab_ids[0];
  }
}

void Session::TerminateOnSessionThread() {
  if (automation_.get())
    automation_->Terminate();
  automation_.reset();
}

void Session::SendKeysOnSessionThread(const string16& keys,
                                      bool* success) {
  *success = true;
  std::vector<WebKeyEvent> key_events;
  ConvertKeysToWebKeyEvents(keys, &key_events);
  for (size_t i = 0; i < key_events.size(); ++i) {
    bool key_success = false;
    automation_->SendWebKeyEvent(
        current_window_id_, key_events[i], &key_success);
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
  std::string xpath;
  if (result->GetAsString(&xpath)) {
    if (current_frame_xpath_.length())
      current_frame_xpath_ += "\n";
    current_frame_xpath_ += xpath;
    return kSuccess;
  }
  return kNoSuchFrame;
}

}  // namespace webdriver
