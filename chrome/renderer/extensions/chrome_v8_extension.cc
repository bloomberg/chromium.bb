// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_v8_extension.h"

#include "base/logging.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "content/public/renderer/render_view.h"

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
