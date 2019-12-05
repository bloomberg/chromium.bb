// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_DELEGATE_H_
#define ASH_SHELL_DELEGATE_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/strings/string16.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom-forward.h"
#include "ui/gfx/native_widget_types.h"

namespace aura {
class Window;
}

namespace ash {

class AccessibilityDelegate;
class ScreenshotDelegate;

// Delegate of the Shell.
class ASH_EXPORT ShellDelegate {
 public:
  // The Shell owns the delegate.
  virtual ~ShellDelegate() {}

  // Returns true if |window| can be shown for the delegate's concept of current
  // user.
  virtual bool CanShowWindowForUser(const aura::Window* window) const = 0;

  // TODO(jamescook): Replace with a mojo-compatible interface.
  virtual std::unique_ptr<ScreenshotDelegate> CreateScreenshotDelegate() = 0;

  // Creates a accessibility delegate. Shell takes ownership of the delegate.
  virtual AccessibilityDelegate* CreateAccessibilityDelegate() = 0;

  // Check whether the current tab of the browser window can go back.
  virtual bool CanGoBack(gfx::NativeWindow window) const = 0;

  // Binds a NavigableContentsFactory receiver for the current active user.
  virtual void BindNavigableContentsFactory(
      mojo::PendingReceiver<content::mojom::NavigableContentsFactory>
          receiver) = 0;

  // Binds a MultiDeviceSetup receiver for the primary profile.
  virtual void BindMultiDeviceSetup(
      mojo::PendingReceiver<
          chromeos::multidevice_setup::mojom::MultiDeviceSetup> receiver) = 0;

  virtual void OpenKeyboardShortcutHelpPage() const {}
};

}  // namespace ash

#endif  // ASH_SHELL_DELEGATE_H_
