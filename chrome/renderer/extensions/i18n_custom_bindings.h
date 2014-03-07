// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_I18N_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_I18N_CUSTOM_BINDINGS_H_

#include "chrome/renderer/extensions/chrome_v8_extension.h"

namespace extensions {

// Implements custom bindings for the i18n API.
class I18NCustomBindings : public ChromeV8Extension {
 public:
  I18NCustomBindings(Dispatcher* dispatcher, ChromeV8Context* context);

 private:
  void GetL10nMessage(const v8::FunctionCallbackInfo<v8::Value>& args);
  void GetL10nUILanguage(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_I18N_CUSTOM_BINDINGS_H_
