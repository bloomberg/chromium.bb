// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_v8_extension.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/resource/resource_bundle.h"

using blink::WebDocument;
using blink::WebFrame;
using blink::WebView;

namespace extensions {

ChromeV8Extension::ChromeV8Extension(Dispatcher* dispatcher,
                                     ChromeV8Context* context)
    : ObjectBackedNativeHandler(context),
      dispatcher_(dispatcher) {
  CHECK(dispatcher);
}

ChromeV8Extension::~ChromeV8Extension() {
}

content::RenderView* ChromeV8Extension::GetRenderView() {
  return context() ? context()->GetRenderView() : NULL;
}

const Extension* ChromeV8Extension::GetExtensionForRenderView() {
  return context() ? context()->extension() : NULL;
}

}  // namespace extensions
