// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_POINTER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_POINTER_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/chromeos/system/pointer_device_observer.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace chromeos {
namespace options {

// Pointer settings overlay page UI handler.
class PointerHandler
    : public ::options::OptionsPageUIHandler,
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

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_POINTER_HANDLER_H_
