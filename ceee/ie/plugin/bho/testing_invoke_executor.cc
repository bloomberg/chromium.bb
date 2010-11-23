// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implements a command-line executable that assists in testing the executor
// by invoking on it from across a process boundary.
#include <atlbase.h>
#include <string>
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "ceee/ie/plugin/toolband/toolband_proxy.h"

#include "toolband.h"  // NOLINT

HRESULT DoInitialize(ICeeeTabExecutor* executor, CommandLine* cmd_line) {
  CeeeWindowHandle hwnd = static_cast<CeeeWindowHandle>(
      atoi(cmd_line->GetSwitchValueASCII("hwnd").c_str()));

  return executor->Initialize(hwnd);
}

HRESULT DoGetTabInfo(ICeeeTabExecutor* executor, CommandLine* cmd_line) {
  CeeeTabInfo info = {};
  return executor->GetTabInfo(&info);
}

HRESULT DoNavigate(ICeeeTabExecutor* executor, CommandLine* cmd_line) {
  std::wstring url = cmd_line->GetSwitchValueNative("url");
  long flags = atoi(cmd_line->GetSwitchValueASCII("flags").c_str());
  std::wstring target = cmd_line->GetSwitchValueNative("target");

  return executor->Navigate(CComBSTR(url.c_str()),
                            flags,
                            CComBSTR(target.c_str()));
}

HRESULT DoInsertCode(ICeeeTabExecutor* executor, CommandLine* cmd_line) {
  std::wstring code = cmd_line->GetSwitchValueNative("code");
  std::wstring file = cmd_line->GetSwitchValueNative("file");
  bool all_frames = cmd_line->GetSwitchValueASCII("all_frames") == "true";
  CeeeTabCodeType type = static_cast<CeeeTabCodeType>(
      atoi(cmd_line->GetSwitchValueASCII("type").c_str()));

  return executor->InsertCode(CComBSTR(code.c_str()),
                              CComBSTR(file.c_str()),
                              all_frames,
                              type);
}

int main(int argc, char** argv) {
  ::CoInitialize(NULL);

  base::AtExitManager at_exit;
  CommandLine::Init(argc, argv);

  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  std::wstring clsid = cmd_line->GetSwitchValueNative("class_id");

  // Register the proxy/stubs for the exector interfaces.
  RegisterProxyStubs(NULL);

  CLSID executor_clsid = {};
  HRESULT hr = ::CLSIDFromString(clsid.c_str(), &executor_clsid);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to read class_id argument";
    exit(hr);
  }

  CComPtr<ICeeeTabExecutor> executor;
  hr = executor.CoCreateInstance(executor_clsid);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create tab executor.";
    exit(hr);
  }

  std::string func = cmd_line->GetSwitchValueASCII("func");
  if (func == "Initialize") {
    hr = DoInitialize(executor, cmd_line);
  } else if (func == "GetTabInfo") {
    hr = DoGetTabInfo(executor, cmd_line);
  } else if (func == "Navigate") {
    hr = DoNavigate(executor, cmd_line);
  } else if (func == "InsertCode") {
    hr = DoInsertCode(executor, cmd_line);
  } else {
    hr = E_UNEXPECTED;
  }

  // We exit, because we don't want to trust the
  // server to be around for teardown and goodbyes.
  exit(hr);

  NOTREACHED();
  return hr;
}
