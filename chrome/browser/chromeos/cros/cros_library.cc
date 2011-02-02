// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_library.h"

#include "base/lazy_instance.h"
#include "chrome/browser/chromeos/cros/brightness_library.h"
#include "chrome/browser/chromeos/cros/burn_library.h"
#include "chrome/browser/chromeos/cros/cros_library_loader.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/cros/keyboard_library.h"
#include "chrome/browser/chromeos/cros/libcros_service_library.h"
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

#define DEFINE_GET_LIBRARY_METHOD(class_prefix, var_prefix)                    \
class_prefix##Library* CrosLibrary::Get##class_prefix##Library() {             \
  return var_prefix##_lib_.GetDefaultImpl(use_stub_impl_);                     \
}

#define DEFINE_SET_LIBRARY_METHOD(class_prefix, var_prefix)                    \
void CrosLibrary::TestApi::Set##class_prefix##Library(                         \
    class_prefix##Library* library, bool own) {                                \
  library_->var_prefix##_lib_.SetImpl(library, own);                           \
}


namespace chromeos {

static base::LazyInstance<CrosLibrary> g_cros_library(
    base::LINKER_INITIALIZED);

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
  return g_cros_library.Pointer();
}

DEFINE_GET_LIBRARY_METHOD(Brightness, brightness);
DEFINE_GET_LIBRARY_METHOD(Burn, burn);
DEFINE_GET_LIBRARY_METHOD(Cryptohome, crypto);
DEFINE_GET_LIBRARY_METHOD(Keyboard, keyboard);
DEFINE_GET_LIBRARY_METHOD(InputMethod, input_method);
DEFINE_GET_LIBRARY_METHOD(LibCrosService, libcros_service);
DEFINE_GET_LIBRARY_METHOD(Login, login);
DEFINE_GET_LIBRARY_METHOD(Mount, mount);
DEFINE_GET_LIBRARY_METHOD(Network, network);
DEFINE_GET_LIBRARY_METHOD(Power, power);
DEFINE_GET_LIBRARY_METHOD(ScreenLock, screen_lock);
DEFINE_GET_LIBRARY_METHOD(SpeechSynthesis, speech_synthesis);
DEFINE_GET_LIBRARY_METHOD(Syslogs, syslogs);
DEFINE_GET_LIBRARY_METHOD(System, system);
DEFINE_GET_LIBRARY_METHOD(Touchpad, touchpad);
DEFINE_GET_LIBRARY_METHOD(Update, update);

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

DEFINE_SET_LIBRARY_METHOD(Brightness, brightness);
DEFINE_SET_LIBRARY_METHOD(Burn, burn);
DEFINE_SET_LIBRARY_METHOD(Cryptohome, crypto);
DEFINE_SET_LIBRARY_METHOD(Keyboard, keyboard);
DEFINE_SET_LIBRARY_METHOD(InputMethod, input_method);
DEFINE_SET_LIBRARY_METHOD(LibCrosService, libcros_service);
DEFINE_SET_LIBRARY_METHOD(Login, login);
DEFINE_SET_LIBRARY_METHOD(Mount, mount);
DEFINE_SET_LIBRARY_METHOD(Network, network);
DEFINE_SET_LIBRARY_METHOD(Power, power);
DEFINE_SET_LIBRARY_METHOD(ScreenLock, screen_lock);
DEFINE_SET_LIBRARY_METHOD(SpeechSynthesis, speech_synthesis);
DEFINE_SET_LIBRARY_METHOD(Syslogs, syslogs);
DEFINE_SET_LIBRARY_METHOD(System, system);
DEFINE_SET_LIBRARY_METHOD(Touchpad, touchpad);
DEFINE_SET_LIBRARY_METHOD(Update, update);

} // namespace chromeos
