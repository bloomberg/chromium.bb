// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_library.h"

#include "chrome/browser/chromeos/cros/cros_library_loader.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/cros/language_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros/mount_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/power_library.h"
#include "chrome/browser/chromeos/cros/screen_lock_library.h"
#include "chrome/browser/chromeos/cros/speech_synthesis_library.h"
#include "chrome/browser/chromeos/cros/synaptics_library.h"

namespace chromeos {

CrosLibrary::CrosLibrary() : library_loader_(NULL),
                             crypto_lib_(NULL),
                             language_lib_(NULL),
                             login_lib_(NULL),
                             mount_lib_(NULL),
                             network_lib_(NULL),
                             power_lib_(NULL),
                             screen_lock_lib_(NULL),
                             speech_synthesis_lib_(NULL),
                             synaptics_lib_(NULL),
                             own_library_loader_(true),
                             own_cryptohome_lib_(true),
                             own_language_lib_(true),
                             own_login_lib_(true),
                             own_mount_lib_(true),
                             own_network_lib_(true),
                             own_power_lib_(true),
                             own_screen_lock_lib_(true),
                             own_speech_synthesis_lib_(true),
                             own_synaptics_lib_(true),
                             loaded_(false),
                             load_error_(false),
                             test_api_(NULL) {

}

CrosLibrary::~CrosLibrary() {
  if (own_library_loader_)
    delete library_loader_;
  if (own_cryptohome_lib_)
    delete crypto_lib_;
  if (own_language_lib_)
    delete language_lib_;
  if (own_login_lib_)
    delete login_lib_;
  if (own_mount_lib_)
    delete mount_lib_;
  if (own_network_lib_)
    delete network_lib_;
  if (own_power_lib_)
    delete power_lib_;
  if (own_screen_lock_lib_)
    delete screen_lock_lib_;
  if (own_speech_synthesis_lib_)
    delete speech_synthesis_lib_;
  if (own_synaptics_lib_)
    delete synaptics_lib_;
  delete test_api_;
}

// static
CrosLibrary* CrosLibrary::Get() {
  return Singleton<CrosLibrary>::get();
}

CryptohomeLibrary* CrosLibrary::GetCryptohomeLibrary() {
  if (!crypto_lib_)
    crypto_lib_ = new CryptohomeLibraryImpl();
  return crypto_lib_;
}

LanguageLibrary* CrosLibrary::GetLanguageLibrary() {
  if (!language_lib_)
    language_lib_ = new LanguageLibraryImpl();
  return language_lib_;
}

LoginLibrary* CrosLibrary::GetLoginLibrary() {
  if (!login_lib_)
    login_lib_ = new LoginLibraryImpl();
  return login_lib_;
}

MountLibrary* CrosLibrary::GetMountLibrary() {
  if (!mount_lib_)
    mount_lib_ = new MountLibraryImpl();
  return mount_lib_;
}

NetworkLibrary* CrosLibrary::GetNetworkLibrary() {
  if (!network_lib_)
    network_lib_ = new NetworkLibraryImpl();
  return network_lib_;
}

PowerLibrary* CrosLibrary::GetPowerLibrary() {
  if (!power_lib_)
    power_lib_ = new PowerLibraryImpl();
  return power_lib_;
}

ScreenLockLibrary* CrosLibrary::GetScreenLockLibrary() {
  if (!screen_lock_lib_)
    screen_lock_lib_ = new ScreenLockLibraryImpl();
  return screen_lock_lib_;
}

SpeechSynthesisLibrary* CrosLibrary::GetSpeechSynthesisLibrary() {
  if (!speech_synthesis_lib_)
    speech_synthesis_lib_ = new SpeechSynthesisLibraryImpl();
  return speech_synthesis_lib_;
}

SynapticsLibrary* CrosLibrary::GetSynapticsLibrary() {
  if (!synaptics_lib_)
    synaptics_lib_ = new SynapticsLibraryImpl();
  return synaptics_lib_;
}

bool CrosLibrary::EnsureLoaded() {
  if (!loaded_ && !load_error_) {
    if (!library_loader_)
      library_loader_ = new CrosLibraryLoader();
    loaded_ = library_loader_->Load(&load_error_string_);
    load_error_ = !loaded_;
  }
  return loaded_;
}

CrosLibrary::TestApi* CrosLibrary::GetTestApi() {
  if (!test_api_)
    test_api_ = new TestApi(this);
  return test_api_;
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

void CrosLibrary::TestApi::SetCryptohomeLibrary(CryptohomeLibrary* library,
                                                bool own) {
  if (library_->own_cryptohome_lib_)
    delete library_->crypto_lib_;
  library_->own_cryptohome_lib_ = own;
  library_->crypto_lib_ = library;
}

void CrosLibrary::TestApi::SetLanguageLibrary(LanguageLibrary* library,
                                              bool own) {
  if (library_->own_language_lib_)
    delete library_->language_lib_;
  library_->own_language_lib_ = own;
  library_->language_lib_ = library;
}

void CrosLibrary::TestApi::SetLoginLibrary(LoginLibrary* library, bool own) {
  if (library_->own_login_lib_)
    delete library_->login_lib_;
  library_->own_login_lib_ = own;
  library_->login_lib_ = library;
}

void CrosLibrary::TestApi::SetMountLibrary(MountLibrary* library, bool own) {
  if (library_->own_mount_lib_)
    delete library_->mount_lib_;
  library_->own_mount_lib_ = own;
  library_->mount_lib_ = library;
}

void CrosLibrary::TestApi::SetNetworkLibrary(NetworkLibrary* library,
                                             bool own) {
  if (library_->own_network_lib_)
    delete library_->network_lib_;
  library_->own_network_lib_ = own;
  library_->network_lib_ = library;
}

void CrosLibrary::TestApi::SetPowerLibrary(PowerLibrary* library, bool own) {
  if (library_->own_power_lib_)
    delete library_->power_lib_;
  library_->own_power_lib_ = own;
  library_->power_lib_ = library;
}

void CrosLibrary::TestApi::SetScreenLockLibrary(ScreenLockLibrary* library,
                                                bool own) {
  if (library_->own_screen_lock_lib_)
    delete library_->screen_lock_lib_;
  library_->own_screen_lock_lib_ = own;
  library_->screen_lock_lib_ = library;
}

void CrosLibrary::TestApi::SetSpeechSynthesisLibrary(
    SpeechSynthesisLibrary* library, bool own) {
  if (library_->own_speech_synthesis_lib_)
    delete library_->speech_synthesis_lib_;
  library_->own_speech_synthesis_lib_ = own;
  library_->speech_synthesis_lib_ = library;
}

void CrosLibrary::TestApi::SetSynapticsLibrary(SynapticsLibrary* library,
                                               bool own) {
  if (library_->own_synaptics_lib_)
    delete library_->synaptics_lib_;
  library_->own_synaptics_lib_ = own;
  library_->synaptics_lib_ = library;
}

}   // end namespace.
