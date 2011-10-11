// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_BINDINGS_POLICY_H_
#define CONTENT_PUBLIC_COMMON_BINDINGS_POLICY_H_
#pragma once

namespace content {

// This enum specifies flag values for the types of JavaScript bindings exposed
// to renderers.
enum BindingsPolicy {
  // HTML-based UI bindings that allows the JS content to send JSON-encoded
  // data back to the browser process.
  BINDINGS_POLICY_WEB_UI = 1 << 0,
  // DOM automation bindings that allows the JS content to send JSON-encoded
  // data back to automation in the parent process.  (By default this isn't
  // allowed unless the app has been started up with the --dom-automation
  // switch.)
  BINDINGS_POLICY_DOM_AUTOMATION = 1 << 1,
  // Bindings that allow access to the external host (through automation).
  BINDINGS_POLICY_EXTERNAL_HOST = 1 << 2,
};

}

#endif  // CONTENT_PUBLIC_COMMON_BINDINGS_POLICY_H_
