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
  I18NCustomBindings(Dispatcher* dispatcher, v8::Handle<v8::Context> context);

 private:
  static v8::Handle<v8::Value> GetL10nMessage(const v8::Arguments& args);
};

}  // extensions

#endif  // CHROME_RENDERER_EXTENSIONS_I18N_CUSTOM_BINDINGS_H_

