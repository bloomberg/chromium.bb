// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_KEY_REWRITER_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_KEY_REWRITER_H_
#pragma once

#include <map>
#include <string>

#include "ash/key_rewriter_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/root_window_observer.h"
#include "ui/base/keycodes/keyboard_codes.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/device_hierarchy_observer.h"
#endif

namespace aura {
class RootWindow;
}

class KeyRewriter : public ash::KeyRewriterDelegate,
                    public aura::RootWindowObserver
#if defined(OS_CHROMEOS)
                  , public chromeos::DeviceHierarchyObserver
#endif
{
 public:
  enum DeviceType {
    kDeviceUnknown = 0,
    kDeviceAppleKeyboard,
  };

  KeyRewriter();
  virtual ~KeyRewriter();

  // Calls DeviceAddedInternal.
  DeviceType DeviceAddedForTesting(int device_id,
                                   const std::string& device_name);
  // Calls Rewrite.
  void RewriteForTesting(aura::KeyEvent* event);

  const std::map<int, DeviceType>& device_id_to_type_for_testing() const {
    return device_id_to_type_;
  }
  void set_last_device_id_for_testing(int device_id) {
    last_device_id_ = device_id;
  }

  // Gets DeviceType from the |device_name|.
  static DeviceType GetDeviceType(const std::string& device_name);

 private:
  // ash::KeyRewriterDelegate overrides:
  virtual ash::KeyRewriterDelegate::Action RewriteOrFilterKeyEvent(
      aura::KeyEvent* event) OVERRIDE;

  // aura::RootWindowObserver overrides:
  virtual void OnKeyboardMappingChanged(const aura::RootWindow* root) OVERRIDE;

#if defined(OS_CHROMEOS)
  // chromeos::DeviceHierarchyObserver overrides:
  virtual void DeviceHierarchyChanged() OVERRIDE {}
  virtual void DeviceAdded(int device_id) OVERRIDE;
  virtual void DeviceRemoved(int device_id) OVERRIDE;
  virtual void DeviceKeyPressedOrReleased(int device_id) OVERRIDE;

  // Updates |*_xkeycode_| in response to a keyboard map change.
  void RefreshKeycodes();
#endif

  // Rewrites the |event| by applying all RewriteXXX functions as needed.
  void Rewrite(aura::KeyEvent* event);

  // Rewrites Comment-L/R key presses on an Apple keyboard to Control-L/R. Only
  // OS_CHROMEOS implementation is available at this point. Returns true when
  // |event| is rewritten.
  bool RewriteCommandToControl(aura::KeyEvent* event);

  // Rewrites a NumPad key press/release without Num Lock to a corresponding key
  // press/release with the lock.  Returns true when |event| is rewritten.
  bool RewriteNumPadKeys(aura::KeyEvent* event);

  // Overwrites |event| with the keycodes and flags.
  void OverwriteEvent(aura::KeyEvent* event,
                      unsigned int new_native_keycode,
                      unsigned int new_native_state,
                      ui::KeyboardCode new_keycode,
                      int new_flags);

  // Checks the type of the |device_name|, and inserts a new entry to
  // |device_id_to_type_|.
  DeviceType DeviceAddedInternal(int device_id, const std::string& device_name);

  std::map<int, DeviceType> device_id_to_type_;
  int last_device_id_;

#if defined(OS_CHROMEOS)
  // X keycodes corresponding to various keysyms.
  unsigned int control_l_xkeycode_;
  unsigned int control_r_xkeycode_;
  unsigned int kp_0_xkeycode_;
  unsigned int kp_1_xkeycode_;
  unsigned int kp_2_xkeycode_;
  unsigned int kp_3_xkeycode_;
  unsigned int kp_4_xkeycode_;
  unsigned int kp_5_xkeycode_;
  unsigned int kp_6_xkeycode_;
  unsigned int kp_7_xkeycode_;
  unsigned int kp_8_xkeycode_;
  unsigned int kp_9_xkeycode_;
  unsigned int kp_decimal_xkeycode_;
#endif

  DISALLOW_COPY_AND_ASSIGN(KeyRewriter);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_KEY_REWRITER_H_
