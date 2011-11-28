// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INPUT_UI_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INPUT_UI_API_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/input_method/ibus_ui_controller.h"
#include "chrome/browser/extensions/extension_function.h"

class Profile;

class ExtensionInputUiEventRouter
    : public chromeos::input_method::IBusUiController::Observer {
 public:
  static ExtensionInputUiEventRouter* GetInstance();
  void Init();

 private:
  friend class CandidateClickedInputUiFunction;
  friend class CursorUpInputUiFunction;
  friend class CursorDownInputUiFunction;
  friend class PageUpInputUiFunction;
  friend class PageDownInputUiFunction;
  friend class RegisterInputUiFunction;
  friend struct DefaultSingletonTraits<ExtensionInputUiEventRouter>;

  ExtensionInputUiEventRouter();
  virtual ~ExtensionInputUiEventRouter();

  void Register(Profile* profile, const std::string& extension_id);
  void CandidateClicked(Profile* profile,
      const std::string& extension_id, int index, int button);
  void CursorUp(Profile* profile, const std::string& extension_id);
  void CursorDown(Profile* profile, const std::string& extension_id);
  void PageUp(Profile* profile, const std::string& extension_id);
  void PageDown(Profile* profile, const std::string& extension_id);

  // IBusUiController overrides.
  virtual void OnHideAuxiliaryText() OVERRIDE;
  virtual void OnHideLookupTable() OVERRIDE;
  virtual void OnHidePreeditText() OVERRIDE;
  virtual void OnSetCursorLocation(int x,
                                   int y,
                                   int width,
                                   int height) OVERRIDE;
  virtual void OnUpdateAuxiliaryText(const std::string& utf8_text,
                                     bool visible) OVERRIDE;
  virtual void OnUpdateLookupTable(
      const chromeos::input_method::InputMethodLookupTable& lookup_table)
          OVERRIDE;
  virtual void OnUpdatePreeditText(const std::string& utf8_text,
                                   unsigned int cursor,
                                   bool visible) OVERRIDE;
  virtual void OnConnectionChange(bool connected) OVERRIDE;

  void DispatchEvent(Profile* profile,
                     const char* event_name,
                     const std::string& json_args);

  Profile* profile_;
  std::string extension_id_;
  scoped_ptr<chromeos::input_method::IBusUiController> ibus_ui_controller_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInputUiEventRouter);
};

class RegisterInputUiFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.input.ui.register");
};

class CandidateClickedInputUiFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.input.ui.candidateClicked");
};

class CursorUpInputUiFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.input.ui.cursorUp");
};

class CursorDownInputUiFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.input.ui.cursorDown");
};

class PageUpInputUiFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.input.ui.pageUp");
};

class PageDownInputUiFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.input.ui.pageDown");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INPUT_UI_API_H_
