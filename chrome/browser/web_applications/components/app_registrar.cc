// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/app_registrar.h"

#include "chrome/browser/web_applications/components/app_registrar_observer.h"

namespace web_app {

AppRegistrar::AppRegistrar() = default;

AppRegistrar::~AppRegistrar() = default;

void AppRegistrar::AddObserver(AppRegistrarObserver* observer) {
  observers_.AddObserver(observer);
}

void AppRegistrar::RemoveObserver(const AppRegistrarObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace web_app
