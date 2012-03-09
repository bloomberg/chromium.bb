// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/experimental.socket_custom_bindings.h"

#include <string>

#include "chrome/common/extensions/extension_action.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "grit/renderer_resources.h"
#include "v8/include/v8.h"

namespace {

v8::Handle<v8::Value> GetNextSocketEventId(const v8::Arguments& args) {
  // Note: this works because the socket API only works in the extension
  // process, not content scripts.
  static int next_event_id = 1;
  return v8::Integer::New(next_event_id++);
}

}  // namespace

namespace extensions {

ExperimentalSocketCustomBindings::ExperimentalSocketCustomBindings()
    : ChromeV8Extension(NULL) {
  RouteStaticFunction("GetNextSocketEventId", &GetNextSocketEventId);
}

}  // extensions
