// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_MANAGER_H_

#include <map>
#include <stack>
#include <string>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/chromeos/login/screen_manager_handler.h"

namespace chromeos {

class BaseScreen;
class OobeDisplay;
class ScreenContext;
class ScreenFactory;
class ScreenFlow;

// Class that manages screen states and flow.
// TODO(antrim): add implementation details comments.
class ScreenManager : public ScreenManagerHandler::Delegate {
 public:
  ScreenManager(ScreenFactory* factory,
                OobeDisplay* oobe_display,
                ScreenFlow* initial_flow);
  virtual ~ScreenManager();

  // Creates and initializes screen, without showing it.
  void WarmupScreen(const std::string& id,
                    ScreenContext* context);

  // Creates, initializes and shows a screen identified by |id|.
  // Should be called when no popup screens are displayed.
  // Closes the previous screen.
  void ShowScreen(const std::string& id);

  // Creates, initializes with |context| and shows a screen identified by |id|.
  // Should be called when no popup screens are displayed.
  // Closes the previous screen.
  void ShowScreenWithParameters(const std::string& id,
                                ScreenContext* context);

  // Creates, initializes and shows a popup screen identified by |id|.
  void PopupScreen(const std::string& id);

  // Creates, initializes with |context| and shows a popup screen identified
  // by |id|.
  void PopupScreenWithParameters(const std::string& id,
                                 ScreenContext* context);

  // Hides the popup screen identified by |screen_id|.
  void HidePopupScreen(const std::string& screen_id);

  std::string GetCurrentScreenId();

  // Sets new screen flow.
  void SetScreenFlow(ScreenFlow* flow);

 private:
  void ShowScreenImpl(const std::string& id,
                      ScreenContext* context,
                      bool isPopup);
  void TransitionScreen(const std::string& from_id,
                        const std::string& to_id);

  void TearDownTopmostScreen();

  void OnDisplayIsReady();

  BaseScreen* GetTopmostScreen();
  BaseScreen* FindOrCreateScreen(const std::string& id);

  // Helper method which simply calls corresponding method on the
  // screen if it exists
  template<typename A1>
  void CallOnScreen(const std::string& screen_name,
                    void (BaseScreen::*method)(A1 arg1),
                    A1 arg1) {
    ScreenMap::const_iterator it = existing_screens_.find(screen_name);
    if (it != existing_screens_.end()) {
      BaseScreen* screen = it->second.get();
      (screen->*method)(arg1);
    } else {
      NOTREACHED();
    }
  }

  // ScreenManagerHandler::Delegate implementation:
  virtual void OnButtonPressed(const std::string& screen_name,
                               const std::string& button_id) OVERRIDE;
  virtual void OnContextChanged(const std::string& screen_name,
                                const base::DictionaryValue* diff) OVERRIDE;

  typedef std::map<std::string, linked_ptr<BaseScreen> > ScreenMap;

  // Factory of screens.
  scoped_ptr<ScreenFactory> factory_;

  // Root of all screen handlers.
  OobeDisplay* display_;

  // Current screen flow.
  scoped_ptr<ScreenFlow> flow_;

  base::WeakPtrFactory<ScreenManager> weak_factory_;

  // Map of existing screens. All screen instances are owned by screen manager.
  ScreenMap existing_screens_;

  // Current stack of screens (screen ids, all screens are assumed to have an
  // instance in |existing_screens_|. Only topmost screen is visible.
  std::stack<std::string> screen_stack_;

  // Flag that indicates if JS counterpart is fully initialized.
  bool js_is_ready_;

  // Capture of parameters for ShowScreen() if it was called before JS
  // counterpart is fully initialized.
  std::string start_screen_;
  scoped_ptr<ScreenContext> start_screen_params_;
  bool start_screen_popup_;

  DISALLOW_COPY_AND_ASSIGN(ScreenManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_MANAGER_H_
