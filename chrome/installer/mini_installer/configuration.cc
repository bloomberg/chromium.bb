// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/configuration.h"

#include <shellapi.h>  // NOLINT

#include "chrome/installer/mini_installer/appid.h"
#include "chrome/installer/mini_installer/mini_installer_constants.h"
#include "chrome/installer/mini_installer/mini_installer_resource.h"
#include "chrome/installer/mini_installer/regkey.h"

namespace mini_installer {

Configuration::Configuration() : args_(NULL) {
  Clear();
}

Configuration::~Configuration() {
  Clear();
}

// When multi_install is true, we are potentially:
// 1. Performing a multi-install of some product(s) on a clean machine.
//    Neither the product(s) nor the multi-installer will have a
//    ClientState key in the registry, so there is no key to be modified.
// 2. Upgrading an existing multi-install.  The multi-installer will have
//    a ClientState key in the registry.  Only it need be modified.
// 3. Migrating a single-install into a multi-install.  The product will
//    have a ClientState key in the registry.  Only it need be modified.
// To handle all cases, we inspect the product's ClientState to see if it
// exists and its "ap" value does not contain "-multi".  This is case 3,
// so we modify the product's ClientState.  Otherwise, we check the
// multi-installer's ClientState and modify it if it exists.
// TODO(bcwhite): Write a unit test for this that uses registry virtualization.
void Configuration::SetChromeAppGuid() {
  const HKEY root_key =
      is_system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  const wchar_t* app_guid =
      has_chrome_frame_ ?
          google_update::kChromeFrameAppGuid :
          is_side_by_side_ ? google_update::kSxSAppGuid
                           : google_update::kAppGuid;

  // This is the value for single-install and case 3.
  chrome_app_guid_ = app_guid;

  if (is_multi_install_) {
    ValueString value;
    LONG ret = ERROR_SUCCESS;
    if (ReadClientStateRegistryValue(root_key, app_guid, &ret, value)) {
      // The product has a client state key.  See if it's a single-install.
      if (ret == ERROR_FILE_NOT_FOUND ||
          (ret == ERROR_SUCCESS &&
           !FindTagInStr(value.get(), kMultiInstallTag, NULL))) {
        // yes -- case 3: use the existing key.
        return;
      }
    }
    // error, case 1, or case 2: modify the multi-installer's key.
    chrome_app_guid_ = google_update::kMultiInstallAppGuid;
  }
}

bool Configuration::ReadClientStateRegistryValue(
    const HKEY root_key, const wchar_t* app_guid,
    LONG* retval, ValueString& value) {
  RegKey key;
  if (!OpenClientStateKey(root_key, app_guid, KEY_QUERY_VALUE, &key))
    return false;
  *retval = key.ReadSZValue(kApRegistryValue, value.get(), value.capacity());
  return true;
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
  is_side_by_side_ = false;
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
        is_side_by_side_ = true;
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

    SetChromeAppGuid();
    if (!is_multi_install_) {
      has_chrome_ = !has_chrome_frame_;
    }
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
