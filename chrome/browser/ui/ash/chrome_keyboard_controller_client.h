// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_CONTROLLER_CLIENT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "ui/keyboard/public/keyboard_controller.mojom.h"

// This class implements mojom::KeyboardControllerObserver and makes calls
// into the mojom::KeyboardController service.
class ChromeKeyboardControllerClient
    : public keyboard::mojom::KeyboardControllerObserver {
 public:
  // Convenience observer allowing UI classes to observe the global instance of
  // this class instead of setting up mojo bindings.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Forwards the 'OnKeyboardVisibilityChanged' mojo observer method.
    // This is used by oobe and login to adjust the UI.
    virtual void OnKeyboardVisibilityChanged(bool visible) = 0;
  };

  // This class uses a static getter and only supports a single instance.
  ChromeKeyboardControllerClient();
  ~ChromeKeyboardControllerClient() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Static getter. The single instance must be instantiated first.
  static ChromeKeyboardControllerClient* Get();

 private:
  void OnGetInitialKeyboardConfig(keyboard::mojom::KeyboardConfigPtr config);

  // keyboard::mojom::KeyboardControllerObserver:
  void OnKeyboardEnabledChanged(bool enabled) override;
  void OnKeyboardVisibilityChanged(bool visible) override;
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& bounds) override;
  void OnKeyboardConfigChanged(
      keyboard::mojom::KeyboardConfigPtr config) override;

  keyboard::mojom::KeyboardControllerPtr keyboard_controller_;
  mojo::AssociatedBinding<keyboard::mojom::KeyboardControllerObserver>
      keyboard_controller_observer_binding_{this};

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<ChromeKeyboardControllerClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeKeyboardControllerClient);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_CONTROLLER_CLIENT_H_
