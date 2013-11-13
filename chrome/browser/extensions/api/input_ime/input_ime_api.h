// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/singleton.h"
#include "base/values.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/common/extensions/extension.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_function.h"

class Profile;

namespace chromeos {
class InputMethodEngine;
class ImeObserver;
}

namespace extensions {
struct InputComponentInfo;

class InputImeEventRouter {
 public:
  static InputImeEventRouter* GetInstance();

  bool RegisterIme(Profile* profile,
                   const std::string& extension_id,
                   const extensions::InputComponentInfo& component);
  void UnregisterAllImes(Profile* profile, const std::string& extension_id);
  chromeos::InputMethodEngine* GetEngine(const std::string& extension_id,
                                         const std::string& engine_id);
  chromeos::InputMethodEngine* GetActiveEngine(const std::string& extension_id);


  // Called when a key event was handled.
  void OnKeyEventHandled(const std::string& extension_id,
                         const std::string& request_id,
                         bool handled);

  std::string AddRequest(const std::string& engine_id,
                         chromeos::input_method::KeyEventHandle* key_data);

 private:
  friend struct DefaultSingletonTraits<InputImeEventRouter>;
  typedef std::map<std::string, std::pair<std::string,
          chromeos::input_method::KeyEventHandle*> > RequestMap;

  InputImeEventRouter();
  ~InputImeEventRouter();

  std::map<std::string, std::map<std::string, chromeos::InputMethodEngine*> >
      engines_;
  std::map<std::string, std::map<std::string, chromeos::ImeObserver*> >
      observers_;

  unsigned int next_request_id_;
  RequestMap request_map_;

  DISALLOW_COPY_AND_ASSIGN(InputImeEventRouter);
};

class InputImeSetCompositionFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setComposition",
                             INPUT_IME_SETCOMPOSITION)

 protected:
  virtual ~InputImeSetCompositionFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class InputImeClearCompositionFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.clearComposition",
                             INPUT_IME_CLEARCOMPOSITION)

 protected:
  virtual ~InputImeClearCompositionFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class InputImeCommitTextFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.commitText", INPUT_IME_COMMITTEXT)

 protected:
  virtual ~InputImeCommitTextFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class InputImeSetCandidateWindowPropertiesFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setCandidateWindowProperties",
                             INPUT_IME_SETCANDIDATEWINDOWPROPERTIES)

 protected:
  virtual ~InputImeSetCandidateWindowPropertiesFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class InputImeSetCandidatesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setCandidates", INPUT_IME_SETCANDIDATES)

 protected:
  virtual ~InputImeSetCandidatesFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class InputImeSetCursorPositionFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setCursorPosition",
                             INPUT_IME_SETCURSORPOSITION)

 protected:
  virtual ~InputImeSetCursorPositionFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class InputImeSetMenuItemsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setMenuItems", INPUT_IME_SETMENUITEMS)

 protected:
  virtual ~InputImeSetMenuItemsFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class InputImeUpdateMenuItemsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.updateMenuItems",
                             INPUT_IME_UPDATEMENUITEMS)

 protected:
  virtual ~InputImeUpdateMenuItemsFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class InputImeDeleteSurroundingTextFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.deleteSurroundingText",
                             INPUT_IME_DELETESURROUNDINGTEXT)
 protected:
  virtual ~InputImeDeleteSurroundingTextFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class InputImeKeyEventHandledFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.keyEventHandled",
                             INPUT_IME_KEYEVENTHANDLED)

 protected:
  virtual ~InputImeKeyEventHandledFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class InputImeAPI : public ProfileKeyedAPI,
                    public content::NotificationObserver {
 public:
  explicit InputImeAPI(Profile* profile);
  virtual ~InputImeAPI();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<InputImeAPI>* GetFactoryInstance();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class ProfileKeyedAPIFactory<InputImeAPI>;
  InputImeEventRouter* input_ime_event_router();

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "InputImeAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  Profile* const profile_;
  content::NotificationRegistrar registrar_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_H_
