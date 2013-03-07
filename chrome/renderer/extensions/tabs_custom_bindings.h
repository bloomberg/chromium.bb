// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_TABS_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_TABS_CUSTOM_BINDINGS_H_

#include "chrome/renderer/extensions/chrome_v8_extension.h"

namespace extensions {

// Implements custom bindings for the tabs API.
class TabsCustomBindings : public ChromeV8Extension {
 public:
  TabsCustomBindings();

 private:
  // Creates a new messaging channel to the tab with the given ID.
  static v8::Handle<v8::Value> OpenChannelToTab(const v8::Arguments& args);
};

}  // extensions

#endif  // CHROME_RENDERER_EXTENSIONS_TABS_CUSTOM_BINDINGS_H_
