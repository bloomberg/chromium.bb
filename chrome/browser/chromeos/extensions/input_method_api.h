// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_API_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"

namespace chromeos {
class ExtensionDictionaryEventRouter;
class ExtensionInputMethodEventRouter;
}

namespace extensions {

// Implements the inputMethodPrivate.getInputMethodConfig  method.
class GetInputMethodConfigFunction : public UIThreadExtensionFunction {
 public:
  GetInputMethodConfigFunction() {}

 protected:
  ~GetInputMethodConfigFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getInputMethodConfig",
                             INPUTMETHODPRIVATE_GETINPUTMETHODCONFIG)
  DISALLOW_COPY_AND_ASSIGN(GetInputMethodConfigFunction);
};

// Implements the inputMethodPrivate.getCurrentInputMethod method.
class GetCurrentInputMethodFunction : public UIThreadExtensionFunction {
 public:
  GetCurrentInputMethodFunction() {}

 protected:
  ~GetCurrentInputMethodFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getCurrentInputMethod",
                             INPUTMETHODPRIVATE_GETCURRENTINPUTMETHOD)
  DISALLOW_COPY_AND_ASSIGN(GetCurrentInputMethodFunction);
};

// Implements the inputMethodPrivate.setCurrentInputMethod method.
class SetCurrentInputMethodFunction : public UIThreadExtensionFunction {
 public:
  SetCurrentInputMethodFunction() {}

 protected:
  ~SetCurrentInputMethodFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.setCurrentInputMethod",
                             INPUTMETHODPRIVATE_SETCURRENTINPUTMETHOD)
  DISALLOW_COPY_AND_ASSIGN(SetCurrentInputMethodFunction);
};

// Implements the inputMethodPrivate.getInputMethods method.
class GetInputMethodsFunction : public UIThreadExtensionFunction {
 public:
  GetInputMethodsFunction() {}

 protected:
  ~GetInputMethodsFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getInputMethods",
                             INPUTMETHODPRIVATE_GETINPUTMETHODS)
  DISALLOW_COPY_AND_ASSIGN(GetInputMethodsFunction);
};

// Implements the inputMethodPrivate.fetchAllDictionaryWords method.
class FetchAllDictionaryWordsFunction : public UIThreadExtensionFunction {
 public:
  FetchAllDictionaryWordsFunction() {}

 protected:
  ~FetchAllDictionaryWordsFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.fetchAllDictionaryWords",
                             INPUTMETHODPRIVATE_FETCHALLDICTIONARYWORDS)
  DISALLOW_COPY_AND_ASSIGN(FetchAllDictionaryWordsFunction);
};

// Implements the inputMethodPrivate.addWordToDictionary method.
class AddWordToDictionaryFunction : public UIThreadExtensionFunction {
 public:
  AddWordToDictionaryFunction() {}

 protected:
  ~AddWordToDictionaryFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.addWordToDictionary",
                             INPUTMETHODPRIVATE_ADDWORDTODICTIONARY)
  DISALLOW_COPY_AND_ASSIGN(AddWordToDictionaryFunction);
};

class InputMethodAPI : public BrowserContextKeyedAPI,
                       public extensions::EventRouter::Observer {
 public:
  static const char kOnDictionaryChanged[];
  static const char kOnDictionaryLoaded[];
  static const char kOnInputMethodChanged[];

  explicit InputMethodAPI(content::BrowserContext* context);
  ~InputMethodAPI() override;

  // Returns input method name for the given XKB (X keyboard extensions in X
  // Window System) id.
  static std::string GetInputMethodForXkb(const std::string& xkb_id);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<InputMethodAPI>* GetFactoryInstance();

  // BrowserContextKeyedAPI implementation.
  void Shutdown() override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const extensions::EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<InputMethodAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "InputMethodAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  content::BrowserContext* const context_;

  // Created lazily upon OnListenerAdded.
  scoped_ptr<chromeos::ExtensionInputMethodEventRouter>
      input_method_event_router_;
  scoped_ptr<chromeos::ExtensionDictionaryEventRouter>
      dictionary_event_router_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodAPI);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_API_H_
