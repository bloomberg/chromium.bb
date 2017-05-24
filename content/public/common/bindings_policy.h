// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_BINDINGS_POLICY_H_
#define CONTENT_PUBLIC_COMMON_BINDINGS_POLICY_H_

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
  // Bindings that allows the JS content to retrieve a variety of internal
  // metrics. (By default this isn't allowed unless the app has been started up
  // with the --enable-stats-collection-bindings switch.)
  BINDINGS_POLICY_STATS_COLLECTION = 1 << 2,
  // Bindings that allows the JS content to access Mojo system API and
  // ServiceRegistry modules. The system API modules are defined in
  // //mojo/public/js and provide the ability to create Mojo primitives such as
  // message and data pipes. The ServiceRegistry module (see
  // //content/renderer/mojo/service_registry_js_wrapper.h) in turn allows these
  // Mojo primitives to be used to connect to named services exposed either by
  // the browser or testing code. These bindings should not be exposed to
  // normal web contents and are intended only for use with WebUI and layout
  // tests.
  BINDINGS_POLICY_MOJO = 1 << 3,
  // Similar to BINDINGS_POLICY_MOJO except it's intended for use by
  // HeadlessWebContents.
  BINDINGS_POLICY_HEADLESS = 1 << 4,
};

}

#endif  // CONTENT_PUBLIC_COMMON_BINDINGS_POLICY_H_
