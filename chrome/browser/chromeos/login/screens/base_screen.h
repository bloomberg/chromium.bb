// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_H_

#include <string>

#include "base/basictypes.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class ScreenContext;

// Base class for the all OOBE/login/before-session screens.
// Screens are identified by ID, screen and it's JS counterpart must have same
// id.
// Most of the screens will be re-created for each appearance with Initialize()
// method called just once. However if initialization is too expensive, screen
// can override result of IsPermanent() method, and do clean-up upon subsequent
// Initialize() method calls.
class BaseScreen {
 public:
  BaseScreen();
  virtual ~BaseScreen();

  // ---- Old implementation ----

  virtual void PrepareToShow() = 0;

  // Makes wizard screen visible.
  virtual void Show() = 0;

  // Makes wizard screen invisible.
  virtual void Hide() = 0;

  // Returns the screen name.
  virtual std::string GetName() const = 0;

  // ---- New Implementation ----

  // Called to perform initialization of the screen. UI is guaranteed to exist
  // at this point. Screen can alter context, resulting context will be passed
  // to JS. This method will be called once per instance of the Screen object,
  // unless |IsPermanent()| returns |true|.
  virtual void Initialize(ScreenContext* context);

  // Called when screen appears.
  virtual void OnShow();
  // Called when screen disappears, either because it finished it's work, or
  // because some other screen pops up.
  virtual void OnHide();

  // Called when we navigate from screen so that we will never return to it.
  // This is a last chance to call JS counterpart, this object will be deleted
  // soon.
  virtual void OnClose();

  // Indicates whether status area should be displayed while this screen is
  // displayed.
  virtual bool IsStatusAreaDisplayed();

  // If this method returns |true|, screen will not be deleted once we leave it.
  // However, Initialize() might be called several times in this case.
  virtual bool IsPermanent();

  // Returns the identifier of the screen.
  virtual std::string GetID() const;

 protected:
  // Screen can call this method to notify framework that it have finished
  // it's work with |outcome|.
  void Finish(const std::string& outcome);

  // Called when button with |button_id| was pressed. Notification
  // about this event comes from the JS counterpart.
  virtual void OnButtonPressed(const std::string& button_id);

  // Called when context for the currenct screen was
  // changed. Notification about this event comes from the JS
  // counterpart.
  virtual void OnContextChanged(const base::DictionaryValue* diff);

 private:
  friend class ScreenManager;
  void SetContext(ScreenContext* context);

  DISALLOW_COPY_AND_ASSIGN(BaseScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_H_
