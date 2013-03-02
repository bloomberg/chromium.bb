// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_v8_extension.h"

#include "base/logging.h"
#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/api/extension_api.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/resource/resource_bundle.h"

using WebKit::WebDocument;
using WebKit::WebFrame;
using WebKit::WebView;

namespace extensions {

ChromeV8Extension::ChromeV8Extension(Dispatcher* dispatcher)
    : ObjectBackedNativeHandler(v8::Context::GetCurrent()),
      dispatcher_(dispatcher) {
}

ChromeV8Extension::ChromeV8Extension(Dispatcher* dispatcher,
                                     v8::Handle<v8::Context> context)
    : ObjectBackedNativeHandler(context),
      dispatcher_(dispatcher) {
}

ChromeV8Extension::~ChromeV8Extension() {
}

ChromeV8Context* ChromeV8Extension::GetContext() {
  CHECK(dispatcher_);
  return dispatcher_->v8_context_set().GetByV8Context(v8_context());
}

content::RenderView* ChromeV8Extension::GetRenderView() {
  ChromeV8Context* context = GetContext();
  return context ? context->GetRenderView() : NULL;
}

const Extension* ChromeV8Extension::GetExtensionForRenderView() {
  ChromeV8Context* context = GetContext();
  return context ? context->extension() : NULL;
}

}  // namespace extensions
