// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROME_INTERFACE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_CHROME_INTERFACE_FACTORY_H_

#include "content/public/common/mojo_shell_connection.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "ui/keyboard/keyboard.mojom.h"

class ChromeLaunchable;
class KeyboardUIService;

namespace chromeos {

// InterfaceFactory for creating all services provided by chrome.
class ChromeInterfaceFactory
    : public content::MojoShellConnection::Listener,
      public shell::InterfaceFactory<mash::mojom::Launchable>,
      public shell::InterfaceFactory<keyboard::mojom::Keyboard> {
 public:
  ChromeInterfaceFactory();
  ~ChromeInterfaceFactory() override;

 private:
  // content::MojoShellConnection::Listener:
  bool AcceptConnection(shell::Connection* connection) override;

  // shell::InterfaceFactory<keyboard::Keyboard>:
  void Create(
      shell::Connection* connection,
      mojo::InterfaceRequest<keyboard::mojom::Keyboard> request) override;

  // mojo::InterfaceFactory<mash::mojom::Launchable>
  void Create(shell::Connection* connection,
              mash::mojom::LaunchableRequest request) override;

  std::unique_ptr<KeyboardUIService> keyboard_ui_service_;
  mojo::BindingSet<keyboard::mojom::Keyboard> keyboard_bindings_;
  std::unique_ptr<ChromeLaunchable> launchable_;

  DISALLOW_COPY_AND_ASSIGN(ChromeInterfaceFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHROME_INTERFACE_FACTORY_H_
