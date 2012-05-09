// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_POINTER_HANDLER2_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_POINTER_HANDLER2_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/ui/webui/options2/options_ui2.h"
#include "chrome/browser/chromeos/system/pointer_device_observer.h"

namespace chromeos {
namespace options2 {

// Pointer settings overlay page UI handler.
class PointerHandler
    : public ::options2::OptionsPageUIHandler,
      public chromeos::system::PointerDeviceObserver::Observer {
 public:
  PointerHandler();
  virtual ~PointerHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings) OVERRIDE;

 private:
  // PointerDeviceObserver implementation.
  virtual void TouchpadExists(bool exists) OVERRIDE;
  virtual void MouseExists(bool exists) OVERRIDE;

  // Set the title dynamically based on whether a touchpad and/or mouse is
  // detected.
  void UpdateTitle();

  bool has_touchpad_;
  bool has_mouse_;

  DISALLOW_COPY_AND_ASSIGN(PointerHandler);
};

}  // namespace options2
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_POINTER_HANDLER2_H_
