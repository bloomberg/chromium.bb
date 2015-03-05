// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/oobe/oobe.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/pref_names.h"

namespace chromeos {

// static
void Oobe::Register() {
  gen::OobeViewModel::SetFactory(
      [](content::BrowserContext* context)
          -> gen::OobeViewModel* { return new Oobe(); });
}

Oobe::Oobe() {
}

Oobe::~Oobe() {
}

void Oobe::Initialize() {
}

void Oobe::OnButtonClicked() {
  g_browser_process->local_state()->SetBoolean(prefs::kNewOobe, false);
  chrome::AttemptRestart();
}

}  // namespace chromeos
