// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/configuration.h"

#include <shellapi.h>  // NOLINT

#include "chrome/installer/mini_installer/appid.h"
#include "chrome/installer/mini_installer/mini_installer_resource.h"

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
  is_multi_install_ = false;
  is_system_level_ = false;
  previous_version_ = NULL;
}

bool Configuration::Initialize(HMODULE module) {
  Clear();
  ReadResources(module);
  return ParseCommandLine(::GetCommandLine());
}

// |command_line| is shared with this instance in the sense that this
// instance may refer to it at will throughout its lifetime, yet it will
// not release it.
bool Configuration::ParseCommandLine(const wchar_t* command_line) {
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
      else if (0 == ::lstrcmpi(args_[i], L"--multi-install"))
        is_multi_install_ = true;
      else if (0 == ::lstrcmpi(args_[i], L"--system-level"))
        is_system_level_ = true;
      else if (0 == ::lstrcmpi(args_[i], L"--cleanup"))
        operation_ = CLEANUP;
    }

    // Single-install defaults to Chrome.
    if (!is_multi_install_)
      has_chrome_ = !has_chrome_frame_;
  }

  return args_ != NULL;
}

void Configuration::ReadResources(HMODULE module) {
  HRSRC resource_info_block =
      FindResource(module, MAKEINTRESOURCE(ID_PREVIOUS_VERSION), RT_RCDATA);
  if (!resource_info_block)
    return;

  HGLOBAL data_handle = LoadResource(module, resource_info_block);
  if (!data_handle)
    return;

  // The data is a Unicode string, so it must be a multiple of two bytes.
  DWORD version_size = SizeofResource(module, resource_info_block);
  if (!version_size || (version_size & 0x01) != 0)
    return;

  void* version_data = LockResource(data_handle);
  if (!version_data)
    return;

  const wchar_t* version_string = reinterpret_cast<wchar_t*>(version_data);
  size_t version_len = version_size / sizeof(wchar_t);

  // The string must be terminated.
  if (version_string[version_len - 1])
    return;

  previous_version_ = version_string;
}

}  // namespace mini_installer
