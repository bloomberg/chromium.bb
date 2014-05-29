// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/configuration.h"

#include <windows.h>
#include <shellapi.h>  // NOLINT

#include "chrome/installer/mini_installer/appid.h"

namespace mini_installer {

Configuration::Configuration() : args_(NULL) {
  Clear();
}

Configuration::~Configuration() {
  Clear();
}

const wchar_t* Configuration::program() const {
  return args_ == NULL || argument_count_ < 1 ? NULL : args_[0];
}

void Configuration::Clear() {
  if (args_ != NULL) {
    ::LocalFree(args_);
    args_ = NULL;
  }
  chrome_app_guid_ = google_update::kAppGuid;
  command_line_ = NULL;
  operation_ = INSTALL_PRODUCT;
  argument_count_ = 0;
  has_chrome_ = false;
  has_chrome_frame_ = false;
  has_app_host_ = false;
  is_multi_install_ = false;
  is_system_level_ = false;
  query_component_build_ = false;
}

bool Configuration::Initialize() {
  return InitializeFromCommandLine(::GetCommandLine());
}

// This is its own function so that unit tests can provide their own command
// lines.  |command_line| is shared with this instance in the sense that this
// instance may refer to it at will throughout its lifetime, yet it will
// not release it.
bool Configuration::InitializeFromCommandLine(const wchar_t* command_line) {
  Clear();

  command_line_ = command_line;
  args_ = ::CommandLineToArgvW(command_line_, &argument_count_);
  if (args_ != NULL) {
    for (int i = 1; i < argument_count_; ++i) {
      if (0 == ::lstrcmpi(args_[i], L"--chrome-sxs"))
        chrome_app_guid_ = google_update::kSxSAppGuid;
      else if (0 == ::lstrcmpi(args_[i], L"--chrome"))
        has_chrome_ = true;
      else if (0 == ::lstrcmpi(args_[i], L"--chrome-frame"))
        has_chrome_frame_ = true;
      else if ((0 == ::lstrcmpi(args_[i], L"--app-host")) ||
               (0 == ::lstrcmpi(args_[i], L"--app-launcher")))
        has_app_host_ = true;
      else if (0 == ::lstrcmpi(args_[i], L"--multi-install"))
        is_multi_install_ = true;
      else if (0 == ::lstrcmpi(args_[i], L"--system-level"))
        is_system_level_ = true;
      else if (0 == ::lstrcmpi(args_[i], L"--cleanup"))
        operation_ = CLEANUP;
      else if (0 == ::lstrcmpi(args_[i], L"--query-component-build"))
        query_component_build_ = true;
    }

    // Single-install defaults to Chrome.
    if (!is_multi_install_)
      has_chrome_ = !(has_chrome_frame_ || has_app_host_);
  }
  return args_ != NULL;
}

}  // namespace mini_installer
