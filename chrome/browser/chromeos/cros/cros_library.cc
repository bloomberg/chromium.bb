// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_library.h"

#include "chrome/browser/chromeos/cros/burn_library.h"
#include "chrome/browser/chromeos/cros/cros_library_loader.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/cros/keyboard_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros/mount_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/power_library.h"
#include "chrome/browser/chromeos/cros/screen_lock_library.h"
#include "chrome/browser/chromeos/cros/speech_synthesis_library.h"
#include "chrome/browser/chromeos/cros/syslogs_library.h"
#include "chrome/browser/chromeos/cros/system_library.h"
#include "chrome/browser/chromeos/cros/touchpad_library.h"
#include "chrome/browser/chromeos/cros/update_library.h"

namespace chromeos {

CrosLibrary::CrosLibrary() : library_loader_(NULL),
                             own_library_loader_(false),
                             use_stub_impl_(false),
                             loaded_(false),
                             load_error_(false),
                             test_api_(NULL) {
}

CrosLibrary::~CrosLibrary() {
  if (own_library_loader_)
    delete library_loader_;
}

// static
CrosLibrary* CrosLibrary::Get() {
  return Singleton<CrosLibrary>::get();
}

BurnLibrary* CrosLibrary::GetBurnLibrary() {
  return burn_lib_.GetDefaultImpl(use_stub_impl_);
}

CryptohomeLibrary* CrosLibrary::GetCryptohomeLibrary() {
  return crypto_lib_.GetDefaultImpl(use_stub_impl_);
}

KeyboardLibrary* CrosLibrary::GetKeyboardLibrary() {
  return keyboard_lib_.GetDefaultImpl(use_stub_impl_);
}

InputMethodLibrary* CrosLibrary::GetInputMethodLibrary() {
  return input_method_lib_.GetDefaultImpl(use_stub_impl_);
}

LoginLibrary* CrosLibrary::GetLoginLibrary() {
  return login_lib_.GetDefaultImpl(use_stub_impl_);
}

MountLibrary* CrosLibrary::GetMountLibrary() {
  return mount_lib_.GetDefaultImpl(use_stub_impl_);
}

NetworkLibrary* CrosLibrary::GetNetworkLibrary() {
  return network_lib_.GetDefaultImpl(use_stub_impl_);
}

PowerLibrary* CrosLibrary::GetPowerLibrary() {
  return power_lib_.GetDefaultImpl(use_stub_impl_);
}

ScreenLockLibrary* CrosLibrary::GetScreenLockLibrary() {
  return screen_lock_lib_.GetDefaultImpl(use_stub_impl_);
}

SpeechSynthesisLibrary* CrosLibrary::GetSpeechSynthesisLibrary() {
  return speech_synthesis_lib_.GetDefaultImpl(use_stub_impl_);
}

SyslogsLibrary* CrosLibrary::GetSyslogsLibrary() {
  return syslogs_lib_.GetDefaultImpl(use_stub_impl_);
}

SystemLibrary* CrosLibrary::GetSystemLibrary() {
  return system_lib_.GetDefaultImpl(use_stub_impl_);
}

TouchpadLibrary* CrosLibrary::GetTouchpadLibrary() {
  return touchpad_lib_.GetDefaultImpl(use_stub_impl_);
}

UpdateLibrary* CrosLibrary::GetUpdateLibrary() {
  return update_lib_.GetDefaultImpl(use_stub_impl_);
}

bool CrosLibrary::EnsureLoaded() {
  if (use_stub_impl_)
    return true;

  if (!loaded_ && !load_error_) {
    if (!library_loader_) {
      library_loader_ = new CrosLibraryLoader();
      own_library_loader_ = true;
    }
    loaded_ = library_loader_->Load(&load_error_string_);
    load_error_ = !loaded_;
  }
  return loaded_;
}

CrosLibrary::TestApi* CrosLibrary::GetTestApi() {
  if (!test_api_.get())
    test_api_.reset(new TestApi(this));
  return test_api_.get();
}

void CrosLibrary::TestApi::SetUseStubImpl() {
  library_->use_stub_impl_ = true;
}

void CrosLibrary::TestApi::ResetUseStubImpl() {
  library_->use_stub_impl_ = false;
}

void CrosLibrary::TestApi::SetLibraryLoader(LibraryLoader* loader, bool own) {
  if (library_->library_loader_ == loader)
    return;
  if (library_->own_library_loader_)
    delete library_->library_loader_;
  library_->own_library_loader_ = own;
  library_->library_loader_ = loader;
  // Reset load flags when loader changes. Otherwise some tests are really not
  // going to be happy.
  library_->loaded_ = false;
  library_->load_error_ = false;
}

void CrosLibrary::TestApi::SetBurnLibrary(
    BurnLibrary* library, bool own) {
  library_->burn_lib_.SetImpl(library, own);
}

void CrosLibrary::TestApi::SetCryptohomeLibrary(
    CryptohomeLibrary* library, bool own) {
  library_->crypto_lib_.SetImpl(library, own);
}

void CrosLibrary::TestApi::SetKeyboardLibrary(
    KeyboardLibrary* library, bool own) {
  library_->keyboard_lib_.SetImpl(library, own);
}

void CrosLibrary::TestApi::SetInputMethodLibrary(
    InputMethodLibrary* library,  bool own) {
  library_->input_method_lib_.SetImpl(library, own);
}

void CrosLibrary::TestApi::SetLoginLibrary(
    LoginLibrary* library, bool own) {
  library_->login_lib_.SetImpl(library, own);
}

void CrosLibrary::TestApi::SetMountLibrary(
    MountLibrary* library, bool own) {
  library_->mount_lib_.SetImpl(library, own);
}

void CrosLibrary::TestApi::SetNetworkLibrary(
    NetworkLibrary* library, bool own) {
  library_->network_lib_.SetImpl(library, own);
}

void CrosLibrary::TestApi::SetPowerLibrary(
    PowerLibrary* library, bool own) {
  library_->power_lib_.SetImpl(library, own);
}

void CrosLibrary::TestApi::SetScreenLockLibrary(
    ScreenLockLibrary* library, bool own) {
  library_->screen_lock_lib_.SetImpl(library, own);
}

void CrosLibrary::TestApi::SetSpeechSynthesisLibrary(
    SpeechSynthesisLibrary* library, bool own) {
  library_->speech_synthesis_lib_.SetImpl(library, own);
}

void CrosLibrary::TestApi::SetTouchpadLibrary(
    TouchpadLibrary* library, bool own) {
  library_->touchpad_lib_.SetImpl(library, own);
}

void CrosLibrary::TestApi::SetSyslogsLibrary(
    SyslogsLibrary* library, bool own) {
  library_->syslogs_lib_.SetImpl(library, own);
}

void CrosLibrary::TestApi::SetSystemLibrary(
    SystemLibrary* library, bool own) {
  library_->system_lib_.SetImpl(library, own);
}

void CrosLibrary::TestApi::SetUpdateLibrary(
    UpdateLibrary* library, bool own) {
  library_->update_lib_.SetImpl(library, own);
}

} // namespace chromeos
