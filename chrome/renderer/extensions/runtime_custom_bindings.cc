// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/runtime_custom_bindings.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "content/public/renderer/v8_value_converter.h"

using content::V8ValueConverter;

namespace extensions {

RuntimeCustomBindings::RuntimeCustomBindings(ChromeV8Context* context)
    : ChromeV8Extension(NULL), context_(context) {
  RouteFunction("GetManifest",
      base::Bind(&RuntimeCustomBindings::GetManifest, base::Unretained(this)));
}

RuntimeCustomBindings::~RuntimeCustomBindings() {}

v8::Handle<v8::Value> RuntimeCustomBindings::GetManifest(
    const v8::Arguments& args) {
  CHECK(context_->extension());

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  return converter->ToV8Value(context_->extension()->manifest()->value(),
                              context_->v8_context());
}

}  // extensions
