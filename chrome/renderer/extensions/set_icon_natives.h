// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_SET_ICON_NATIVES_H_
#define CHROME_RENDERER_EXTENSIONS_SET_ICON_NATIVES_H_
#pragma once

#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"

#include "v8/include/v8.h"

class ExtensionRequestSender;

namespace base {
class Value;
}

namespace extensions {

// Functions exposed to extension JS to implement the setIcon extension API.
class SetIconNatives : public ChromeV8Extension {
 public:
  SetIconNatives(ExtensionDispatcher* extension_dispatcher,
                 ExtensionRequestSender* request_sender);

 private:
  bool ConvertImageDataToBitmapValue(const v8::Arguments& args,
                                     Value** bitmap_value);
  v8::Handle<v8::Value> SetIconCommon(const v8::Arguments& args);
  ExtensionRequestSender* request_sender_;

  DISALLOW_COPY_AND_ASSIGN(SetIconNatives);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_SET_ICON_NATIVES_H_
