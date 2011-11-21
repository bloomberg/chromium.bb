// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INPUT_IME_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INPUT_IME_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

#include "base/memory/singleton.h"
#include "base/values.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/common/extensions/extension.h"

#include <map>
#include <string>
#include <vector>

class Profile;

namespace chromeos {
class InputMethodEngine;
class ImeObserver;
}

class ExtensionInputImeEventRouter {
 public:
  static ExtensionInputImeEventRouter* GetInstance();
  void Init();

  bool RegisterIme(Profile* profile,
                   const std::string& extension_id,
                   const Extension::InputComponentInfo& component);
  chromeos::InputMethodEngine* GetEngine(const std::string& extension_id,
                                         const std::string& engine_id);
  chromeos::InputMethodEngine* GetActiveEngine(const std::string& extension_id);


  // Called when a key event was handled.
  void OnEventHandled(const std::string& extension_id,
                      const std::string& request_id,
                      bool handled);

  std::string AddRequest(const std::string& engine_id,
                         chromeos::input_method::KeyEventHandle* key_data);

 private:
  friend struct DefaultSingletonTraits<ExtensionInputImeEventRouter>;
  typedef std::map<std::string, std::pair<std::string,
          chromeos::input_method::KeyEventHandle*> > RequestMap;

  ExtensionInputImeEventRouter();
  ~ExtensionInputImeEventRouter();

  std::map<std::string, std::map<std::string, chromeos::InputMethodEngine*> >
      engines_;
  std::map<std::string, std::map<std::string, chromeos::ImeObserver*> >
      observers_;

  unsigned int next_request_id_;
  RequestMap request_map_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInputImeEventRouter);
};

class SetCompositionFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.input.ime.setComposition");
};

class ClearCompositionFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.input.ime.clearComposition");
};

class CommitTextFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.input.ime.commitText");
};

class SetCandidateWindowPropertiesFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.input.ime.setCandidateWindowProperties");
};

class SetCandidatesFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.input.ime.setCandidates");
 private:
  bool ReadCandidates(
      ListValue* candidates,
      std::vector<chromeos::InputMethodEngine::Candidate>* output);
};

class SetCursorPositionFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.input.ime.setCursorPosition");
};

class SetMenuItemsFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.input.ime.setMenuItems");
};

class UpdateMenuItemsFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.input.ime.updateMenuItems");
};

class InputEventHandled : public AsyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.input.ime.eventHandled");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INPUT_IME_API_H_
