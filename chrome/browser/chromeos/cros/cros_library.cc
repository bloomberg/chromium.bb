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
#include "chrome/browser/chromeos/cros/synaptics_library.h"

namespace chromeos {

CrosLibrary::CrosLibrary() : library_loader_(NULL),
                             crypto_lib_(NULL),
                             language_lib_(NULL),
                             login_lib_(NULL),
                             mount_lib_(NULL),
                             network_lib_(NULL),
                             power_lib_(NULL),
                             synaptics_lib_(NULL),
                             loaded_(false),
                             load_error_(false),
                             test_api_(NULL) {

}

CrosLibrary::~CrosLibrary() {
  if (library_loader_)
    delete library_loader_;
  if (crypto_lib_)
    delete crypto_lib_;
  if (language_lib_)
    delete language_lib_;
  if (login_lib_)
    delete login_lib_;
  if (mount_lib_)
    delete mount_lib_;
  if (network_lib_)
    delete network_lib_;
  if (power_lib_)
    delete power_lib_;
  if (synaptics_lib_)
    delete synaptics_lib_;
  if (test_api_)
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

void CrosLibrary::TestApi::SetLibraryLoader(LibraryLoader* loader) {
  if (library_->library_loader_)
    delete library_->library_loader_;
  library_->library_loader_ = loader;
}

void CrosLibrary::TestApi::SetCryptohomeLibrary(CryptohomeLibrary* library) {
  if (library_->crypto_lib_)
    delete library_->crypto_lib_;
  library_->crypto_lib_ = library;
}

void CrosLibrary::TestApi::SetLanguageLibrary(LanguageLibrary* library) {
  if (library_->language_lib_)
    delete library_->language_lib_;
  library_->language_lib_ = library;
}

void CrosLibrary::TestApi::SetLoginLibrary(LoginLibrary* library) {
  if (library_->login_lib_)
    delete library_->login_lib_;
  library_->login_lib_ = library;
}

void CrosLibrary::TestApi::SetMountLibrary(MountLibrary* library) {
  if (library_->mount_lib_)
    delete library_->mount_lib_;
  library_->mount_lib_ = library;
}

void CrosLibrary::TestApi::SetNetworkLibrary(NetworkLibrary* library) {
  if (library_->network_lib_)
    delete library_->network_lib_;
  library_->network_lib_ = library;
}

void CrosLibrary::TestApi::SetPowerLibrary(PowerLibrary* library) {
  if (library_->power_lib_)
    delete library_->power_lib_;
  library_->power_lib_ = library;
}

void CrosLibrary::TestApi::SetSynapticsLibrary(SynapticsLibrary* library) {
  if (library_->synaptics_lib_)
    delete library_->synaptics_lib_;
  library_->synaptics_lib_ = library;
}

}   // end namespace.

