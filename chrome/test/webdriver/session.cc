// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/session_manager.h"

#ifdef OS_POSIX
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#endif
#ifdef OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

#include <stdlib.h>
#ifdef OS_POSIX
#include <algorithm>
#endif
#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/test/test_timeouts.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/utf_string_conversions.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/test_launcher_utils.h"
#include "chrome/test/webdriver/utility_functions.h"

#include "third_party/webdriver/atoms.h"

namespace webdriver {

#ifdef OS_WIN
namespace {
std::string GetTempPath() {
  DWORD result = ::GetTempPath(0, L"");
  if (result == 0)
    LOG(ERROR) << "Could not get system temp path";

  std::vector<TCHAR> tempPath(result + 1);
  result = ::GetTempPath(static_cast<DWORD>(tempPath.size()), &tempPath[0]);
  if ((result == 0) || (result >= tempPath.size())) {
    LOG(ERROR) << "Could not get system temp path";
    NOTREACHED();
  }
  return std::string(tempPath.begin(),
                     tempPath.begin() + static_cast<std::size_t>(result));
}
}  // namespace
#endif

Session::Session(const std::string& id)
    : UITestBase(), id_(id), window_num_(0), implicit_wait_(0),
      current_frame_xpath_(L"") {
}

bool Session::Init() {
  // Create a temp directory for the new profile.
  if (!CreateTemporaryProfileDirectory()) {
    LOG(ERROR) << "Could not make a temp profile directory, "
               << tmp_profile_dir()
               << "\nNeed to quit, the issue must be fixed";
    exit(-1);
  }

  SetupCommandLine();
  LaunchBrowserAndServer();
  return LoadProxies();
}

scoped_refptr<TabProxy> Session::ActiveTab() {
  int tab_index;
  if (!tab_->GetTabIndex(&tab_index) ||
      !browser_->ActivateTab(tab_index)) {
    LOG(ERROR) << "Failed to session tab";
    return NULL;
  }
  return tab_;
}

bool Session::LoadProxies() {
  AutomationProxy* proxy = automation();
  scoped_refptr<BrowserProxy> browser = proxy->GetBrowserWindow(0);
  if (!browser.get()) {
    LOG(WARNING) << "Failed to get browser window.";
    return false;
  }

  scoped_refptr<TabProxy> tab = browser->GetActiveTab();
  if (!tab.get()) {
    LOG(ERROR) << "Could not load tab";
    return false;
  }

  SetBrowserAndTab(0, browser, tab);
  return true;
}

void Session::SetupCommandLine() {
  test_launcher_utils::PrepareBrowserCommandLineForTests(&launch_arguments_);
  launch_arguments_.AppendSwitch(switches::kDomAutomationController);
  launch_arguments_.AppendSwitch(switches::kFullMemoryCrashReport);

  launch_arguments_.AppendSwitchASCII(switches::kUserDataDir,
                                      tmp_profile_dir());
}

void Session::SetBrowserAndTab(const int window_num,
                               const scoped_refptr<BrowserProxy>& browser,
                               const scoped_refptr<TabProxy>& tab) {
  window_num_ = window_num;
  browser_ = browser;
  tab_ = tab;
  current_frame_xpath_ = L"";

  int tab_num;
  LOG_IF(WARNING, !tab->GetTabIndex(&tab_num) || !browser->ActivateTab(tab_num))
      << "Failed to activate tab";
}

bool Session::CreateTemporaryProfileDirectory() {
  memset(tmp_profile_dir_, 0, sizeof tmp_profile_dir_);
#ifdef OS_POSIX
  strncat(tmp_profile_dir_, "/tmp/webdriverXXXXXX", sizeof tmp_profile_dir_);
  if (mkdtemp(tmp_profile_dir_) == NULL) {
    LOG(ERROR) << "mkdtemp failed";
    return false;
  }
#elif OS_WIN
  DWORD ret;
  ProfileDir temp_dir;

  ret = GetTempPathA(sizeof temp_dir, temp_dir);
  if (ret == 0 || ret > sizeof temp_dir) {
    LOG(ERROR) << "Could not find the temp directory";
    return false;
  }

  ret = GetTempFileNameA(temp_dir,        // Directory for tmp files.
                         "webdriver",     // Temp file name prefix.
                         static_cast<int>(time(NULL)) % 65535 + 1,
                         tmp_profile_dir_);  // Buffer for name.

  if (ret ==0) {
    LOG(ERROR) << "Could not generate temp directory name";
    return false;
  }

  if (!CreateDirectoryA(tmp_profile_dir_, NULL)) {
    DWORD dw = GetLastError();
    LOG(ERROR) << "Error code: " << dw;
    return false;
  }
#endif
  VLOG(1) << "Using temporary profile directory: " << tmp_profile_dir_;
  return true;
}

void Session::Terminate() {
  QuitBrowser();
#ifdef OS_POSIX
  FilePath del_dir = FilePath(tmp_profile_dir());
#elif OS_WIN
  FilePath del_dir = FilePath(ASCIIToWide(tmp_profile_dir()));
#endif
  if (file_util::PathExists(del_dir) && !file_util::Delete(del_dir, true))
    LOG(ERROR) << "Could not clean up temp directory: " << tmp_profile_dir();
}

ErrorCode Session::ExecuteScript(const std::wstring& script,
                                 const ListValue* const args,
                                 Value** value) {
  std::string args_as_json;
  base::JSONWriter::Write(static_cast<const Value* const>(args),
                          /*pretty_print=*/false,
                          &args_as_json);

  std::wstring jscript = L"window.domAutomationController.send((function(){" +
      // Every injected script is fed through the executeScript atom. This atom
      // will catch any errors that are thrown and convert them to the
      // appropriate JSON structure.
      build_atom(EXECUTE_SCRIPT, sizeof EXECUTE_SCRIPT) +
      L"var result = executeScript(function(){" + script + L"}," +
      ASCIIToWide(args_as_json) + L");return JSON.stringify(result);})());";

  // Should we also log the script that's being executed? It could be several KB
  // in size and will add lots of noise to the logs.
  VLOG(1) << "Executing script in frame: " << current_frame_xpath_;

  std::wstring result;
  scoped_refptr<TabProxy> tab = ActiveTab();
  if (!tab->ExecuteAndExtractString(current_frame_xpath_, jscript, &result)) {
    *value = Value::CreateStringValue(
        "Unknown internal script execution failure");
    return kUnknownError;
  }

  VLOG(1) << "...script result: " << result;
  std::string temp = WideToASCII(result);
  scoped_ptr<Value> r(base::JSONReader::ReadAndReturnError(
      temp, true, NULL, NULL));
  if (!r.get()) {
    *value = Value::CreateStringValue(
        "Internal script execution error: failed to parse script result");
    return kUnknownError;
  }

  if (r->GetType() != Value::TYPE_DICTIONARY) {
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

}  // namespace webdriver
