// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_SHELL_DELEGATE_H_
#define ASH_TEST_SHELL_DELEGATE_H_

#include <memory>

#include "ash/shell_delegate.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

class TestShellDelegate : public ShellDelegate {
 public:
  TestShellDelegate();
  ~TestShellDelegate() override;

  // Allows tests to override the MultiDeviceSetup binding behavior for this
  // TestShellDelegate.
  using MultiDeviceSetupBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<
          chromeos::multidevice_setup::mojom::MultiDeviceSetup>)>;
  void SetMultiDeviceSetupBinder(MultiDeviceSetupBinder binder) {
    multidevice_setup_binder_ = std::move(binder);
  }

  // Overridden from ShellDelegate:
  bool CanShowWindowForUser(const aura::Window* window) const override;
  std::unique_ptr<ScreenshotDelegate> CreateScreenshotDelegate() override;
  AccessibilityDelegate* CreateAccessibilityDelegate() override;
  std::unique_ptr<BackGestureContextualNudgeDelegate>
  CreateBackGestureContextualNudgeDelegate(
      BackGestureContextualNudgeController* controller) override;
  bool CanGoBack(gfx::NativeWindow window) const override;
  void BindNavigableContentsFactory(
      mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver)
      override;
  void BindMultiDeviceSetup(
      mojo::PendingReceiver<
          chromeos::multidevice_setup::mojom::MultiDeviceSetup> receiver)
      override;

  void SetCanGoBack(bool can_go_back);

 private:
  // True if the current top window can back.
  bool can_go_back_ = true;

  MultiDeviceSetupBinder multidevice_setup_binder_;

  DISALLOW_COPY_AND_ASSIGN(TestShellDelegate);
};

}  // namespace ash

#endif  // ASH_TEST_SHELL_DELEGATE_H_
