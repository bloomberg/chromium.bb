// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_API_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

namespace chromeos {
class ExtensionInputMethodEventRouter;
}

namespace extensions {

// Implements the experimental.inputMethod.get method.
class GetInputMethodFunction : public SyncExtensionFunction {
 public:
  GetInputMethodFunction();

 protected:
  virtual ~GetInputMethodFunction();

  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.get", INPUTMETHODPRIVATE_GET)
};

class InputMethodAPI : public ProfileKeyedService,
                       public extensions::EventRouter::Observer {
 public:
  explicit InputMethodAPI(Profile* profile);
  virtual ~InputMethodAPI();

  // Returns input method name for the given XKB (X keyboard extensions in X
  // Window System) id.
  static std::string GetInputMethodForXkb(const std::string& xkb_id);

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const extensions::EventListenerInfo& details)
      OVERRIDE;

 private:
  Profile* const profile_;

  // Created lazily upon OnListenerAdded.
  scoped_ptr<chromeos::ExtensionInputMethodEventRouter>
      input_method_event_router_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_API_H_
