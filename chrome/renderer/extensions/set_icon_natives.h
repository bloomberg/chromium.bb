// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_SET_ICON_NATIVES_H_
#define CHROME_RENDERER_EXTENSIONS_SET_ICON_NATIVES_H_

#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/dispatcher.h"

#include "v8/include/v8.h"

namespace base {
class Value;
}

namespace extensions {
class RequestSender;

// Functions exposed to extension JS to implement the setIcon extension API.
class SetIconNatives : public ChromeV8Extension {
 public:
  SetIconNatives(Dispatcher* dispatcher, RequestSender* request_sender);

 private:
  bool ConvertImageDataToBitmapValue(const v8::Local<v8::Object> image_data,
                                     Value** bitmap_value);
  bool ConvertImageDataSetToBitmapValueSet(const v8::Arguments& args,
                                           DictionaryValue* bitmap_value);
  v8::Handle<v8::Value> SetIconCommon(const v8::Arguments& args);
  RequestSender* request_sender_;

  DISALLOW_COPY_AND_ASSIGN(SetIconNatives);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_SET_ICON_NATIVES_H_
