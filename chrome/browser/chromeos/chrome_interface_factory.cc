// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chrome_interface_factory.h"

#include "chrome/browser/ui/ash/keyboard_ui_service.h"
#include "mojo/shell/public/cpp/connection.h"

namespace chromeos {

ChromeInterfaceFactory::ChromeInterfaceFactory() {}
ChromeInterfaceFactory::~ChromeInterfaceFactory() {}

bool ChromeInterfaceFactory::AcceptConnection(mojo::Connection* connection) {
  connection->AddInterface(this);
  return true;
}

void ChromeInterfaceFactory::Create(
    mojo::Connection* connection,
    mojo::InterfaceRequest<keyboard::mojom::Keyboard> request) {
  if (!keyboard_ui_service_)
    keyboard_ui_service_.reset(new KeyboardUIService);
  keyboard_bindings_.AddBinding(keyboard_ui_service_.get(), std::move(request));
}

}  // namespace chromeos
