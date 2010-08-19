// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/session_manager.h"

#ifdef OS_POSIX
#include <unistd.h>
#endif
#ifdef OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

#include <stdlib.h>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/utf_string_conversions.h"

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/webdriver/utility_functions.h"

#include "third_party/webdriver/atoms.h"

namespace webdriver {

#ifdef OS_WIN
namespace {
std::string GetTempPath() {
  DWORD result = ::GetTempPath(0, L"");
  if (result == 0)
    LOG(ERROR) << "Could not get system temp path" << std::endl;

  std::vector<TCHAR> tempPath(result + 1);
  result = ::GetTempPath(static_cast<DWORD>(tempPath.size()), &tempPath[0]);
  if ((result == 0) || (result >= tempPath.size())) {
    LOG(ERROR) << "Could not get system temp path" << std::endl;
    NOTREACHED();
  }
  return std::string(tempPath.begin(),
                     tempPath.begin() + static_cast<std::size_t>(result));
}
}  // namespace
#endif

Session::Session(const std::string& id, AutomationProxy* proxy)
  : id_(id), proxy_(proxy), process_(base::kNullProcessHandle),
    window_num_(0), implicit_wait_(0), current_frame_xpath_(L"") {
}

bool Session::Init(base::ProcessHandle process_handle) {
  CHECK(process_.handle() == base::kNullProcessHandle)
    << "Session has already been initialized";
  process_.set_handle(process_handle);

  if (!proxy_->WaitForInitialLoads()) {
    LOG(WARNING) << "Failed to wait for initial loads" << std::endl;
    return false;
  }

  if (!WaitForLaunch()) {
    return false;
  }

  return LoadProxies();
}

scoped_refptr<WindowProxy> Session::GetWindow() {
  return ActivateTab() ? browser_->GetWindow() : NULL;
}

scoped_refptr<TabProxy> Session::ActivateTab() {
  int tab_index;
  if (!tab_->GetTabIndex(&tab_index) ||
      !browser_->ActivateTab(tab_index)) {
    LOG(ERROR) << "Failed to session tab";
    return NULL;
  }
  return tab_;
}

bool Session::WaitForLaunch() {
  AutomationLaunchResult result = proxy_->WaitForAppLaunch();
  if (result == AUTOMATION_SUCCESS) {
    LOG(INFO) << "Automation setup is a success" << std::endl;
  } else if (result == AUTOMATION_TIMEOUT) {
    LOG(WARNING) << "Timeout, automation setup failed" << std::endl;
    return false;
  } else {
    LOG(WARNING) << "Failure in automation setup" << std::endl;
    return false;
  }

  LOG(INFO) << proxy_->channel_id() << std::endl;
  if (!proxy_->OpenNewBrowserWindow(Browser::TYPE_NORMAL, true)) {
    LOG(WARNING) << "Failed to open a new browser window" << std::endl;
    return false;
  }
  return true;
}

bool Session::LoadProxies() {
  scoped_refptr<BrowserProxy> browser = proxy_->GetBrowserWindow(0);
  if (!browser.get()) {
    LOG(WARNING) << "Failed to get browser window.";
    return false;
  }

  scoped_refptr<TabProxy> tab = browser->GetActiveTab();
  if (!tab->NavigateToURL(GURL("about://config/"))) {
    LOG(WARNING) << "Could not navigate to about://config/" << std::endl;
    return false;
  }

  SetBrowserAndTab(0, browser, tab);
  return true;
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
    LOG(ERROR) << "Could not find the temp directory" << std::endl;
    return false;
  }

  ret = GetTempFileNameA(temp_dir,        // directory for tmp files
                         "webdriver",     // temp file name prefix
                         static_cast<int>(time(NULL)) % 65535 + 1,
                         tmp_profile_dir_);  // buffer for name

  if (ret ==0) {
    LOG(ERROR) << "Could not generate temp directory name" << std::endl;
    return false;
  }

  if (!CreateDirectoryA(tmp_profile_dir_, NULL)) {
    DWORD dw = GetLastError();
    LOG(ERROR) << "Error code: " << dw << std::endl;
    return false;
  }
#endif
  LOG(INFO) << "Using temporary profile directory: " << tmp_profile_dir_;
  return true;
}

#ifdef OS_WIN
bool DeleteDirectory(const char* directory) {
  SHFILEOPSTRUCT fileop;
  size_t convertedChars;
  int len = strlen(directory);
  wchar_t *pszFrom = new wchar_t[len+2];
  memset(&fileop, 0, sizeof fileop);
  memset(pszFrom, 0, sizeof pszFrom);

  mbstowcs_s(&convertedChars, pszFrom, len+2, directory, len);
  fileop.wFunc  = FO_DELETE;  // delete operation
  // source file name as double null terminated string
  fileop.pFrom  = (LPCWSTR) pszFrom;
  fileop.fFlags = FOF_NOCONFIRMATION|FOF_SILENT;  // do not prompt the user
  fileop.fAnyOperationsAborted = FALSE;

  int ret = SHFileOperation(&fileop);
  delete pszFrom;
  return (ret == 0);
}
#endif

void Session::Terminate() {
  if (!proxy_->SetFilteredInet(false)) {
    LOG(ERROR) << "Error in closing down session" << std::endl;
  }

#ifdef OS_WIN
  if (!DeleteDirectory(tmp_profile_dir_)) {
    LOG(ERROR) << "Could not clean up temp directory: "
               << tmp_profile_dir_ << std::endl;
  }
#else
  if (rmdir(tmp_profile_dir_)) {
    LOG(ERROR) << "Could not clean up temp directory: "
               << tmp_profile_dir_ << std::endl;
  }
#endif
  process_.Terminate(EXIT_SUCCESS);
}

ErrorCode Session::ExecuteScript(const std::wstring& script,
                                 const ListValue* const args,
                                 Value** value) {
  std::string args_as_json;
  base::JSONWriter::Write(static_cast<const Value* const>(args),
                          /*pretty_print=*/false,
                          &args_as_json);

  std::wstring jscript = L"window.domAutomationController.send(";
  jscript.append(L"(function(){")
  // Every injected script is fed through the executeScript atom. This atom
  // will catch any errors that are thrown and convert them to the
  // appropriate JSON structure.
  .append(build_atom(EXECUTE_SCRIPT, sizeof EXECUTE_SCRIPT))
  .append(L"var result = executeScript(function(){")
  .append(script)
  .append(L"},")
  .append(ASCIIToWide(args_as_json))
  .append(L");return JSON.stringify(result);})());");

  // Should we also log the script that's being executed? It could be several KB
  // in size and will add lots of noise to the logs.
  LOG(INFO) << "Executing script in frame: " << current_frame_xpath_;

  std::wstring result;
  scoped_refptr<TabProxy> tab = ActivateTab();
  if (!tab->ExecuteAndExtractString(current_frame_xpath_, jscript, &result)) {
    *value = Value::CreateStringValue(
        "Unknown internal script execution failure");
    return kUnknownError;
  }

  LOG(INFO) << "...script result: " << result;
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
      << print_valuetype(Value::TYPE_DICTIONARY)
      << ", but was "
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
