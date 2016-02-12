// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROME_INTERFACE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_CHROME_INTERFACE_FACTORY_H_

#include "content/public/common/mojo_shell_connection.h"
#include "mojo/public/cpp/bindings/weak_binding_set.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "ui/keyboard/keyboard.mojom.h"

class KeyboardUIService;

namespace chromeos {

// InterfaceFactory for creating all services provided by chrome.
class ChromeInterfaceFactory
    : public content::MojoShellConnection::Listener,
      public mojo::InterfaceFactory<keyboard::mojom::Keyboard> {
 public:
  ChromeInterfaceFactory();
  ~ChromeInterfaceFactory() override;

 private:
  // content::MojoShellConnection::Listener:
  bool AcceptConnection(mojo::Connection* connection) override;

  // mojo::InterfaceFactory<keyboard::Keyboard>:
  void Create(
      mojo::Connection* connection,
      mojo::InterfaceRequest<keyboard::mojom::Keyboard> request) override;

  scoped_ptr<KeyboardUIService> keyboard_ui_service_;
  mojo::WeakBindingSet<keyboard::mojom::Keyboard> keyboard_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ChromeInterfaceFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHROME_INTERFACE_FACTORY_H_
