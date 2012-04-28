// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/experimental.usb_custom_bindings.h"

#include "base/logging.h"
#include "v8/include/v8.h"

namespace {

v8::Handle<v8::Value> GetNextUsbEventId(const v8::Arguments &arguments) {
  static int next_event_id = 1;
  return v8::Integer::New(next_event_id++);
}

}  // namespace

namespace extensions {

ExperimentalUsbCustomBindings::ExperimentalUsbCustomBindings()
    : ChromeV8Extension(NULL) {
  RouteStaticFunction("GetNextUsbEventId", &GetNextUsbEventId);
}

}  // namespace extensions
