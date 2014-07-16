// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_external_loader.h"

#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"

namespace chromeos {

KioskAppExternalLoader::KioskAppExternalLoader() {
}

KioskAppExternalLoader::~KioskAppExternalLoader() {
}

void KioskAppExternalLoader::SetCurrentAppExtensions(
    scoped_ptr<base::DictionaryValue> prefs) {
  kiosk_apps_.Swap(prefs.get());
  StartLoading();
}

void KioskAppExternalLoader::StartLoading() {
  prefs_.reset(kiosk_apps_.DeepCopy());
  LoadFinished();
}

}  // namespace chromeos
