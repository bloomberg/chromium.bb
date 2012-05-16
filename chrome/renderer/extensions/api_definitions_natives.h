// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_API_DEFINITIONS_NATIVES_H_
#define CHROME_RENDERER_EXTENSIONS_API_DEFINITIONS_NATIVES_H_
#pragma once

#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"

#include "v8/include/v8.h"

namespace extensions {

// Native functions for JS to get access to the schemas for extension APIs.
class ApiDefinitionsNatives : public ChromeV8Extension {
 public:
  explicit ApiDefinitionsNatives(ExtensionDispatcher* extension_dispatcher);

 private:
  // Returns the list of schemas that are available to the calling context
  // and have their names listed in |args|. If |args| is empty, returns the list
  // of all schemas that are available to the calling context.
  v8::Handle<v8::Value> GetExtensionAPIDefinition(const v8::Arguments& args);

  DISALLOW_COPY_AND_ASSIGN(ApiDefinitionsNatives);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_API_DEFINITIONS_NATIVES_H_
