// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_POINTER_HANDLER2_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_POINTER_HANDLER2_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/device_hierarchy_observer.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/ui/webui/options2/options_ui2.h"

namespace chromeos {
namespace options2 {

// Pointer settings overlay page UI handler.
class PointerHandler : public OptionsPageUIHandler,
                       public chromeos::DeviceHierarchyObserver,
                       public base::SupportsWeakPtr<PointerHandler> {
 public:
  PointerHandler();
  virtual ~PointerHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void InitializePage() OVERRIDE;

  // DeviceHierarchyObserver implementation.
  virtual void DeviceHierarchyChanged() OVERRIDE;

 private:
  // Check for input devices.
  void CheckTouchpadExists();
  void CheckMouseExists();

  // Callback for input device checks.
  void TouchpadExists(bool* exists);
  void MouseExists(bool* exists);

  DISALLOW_COPY_AND_ASSIGN(PointerHandler);
};

}  // namespace options2
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_POINTER_HANDLER2_H_
