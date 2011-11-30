// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_JS_INJECTION_READY_OBSERVER_H_
#define CHROME_TEST_BASE_JS_INJECTION_READY_OBSERVER_H_
#pragma once

class RenderViewHost;

// Interface to notify when JavaScript injection is possible.
class JsInjectionReadyObserver {
 public:
  // Called to indicate page entry committed and ready for JavaScript
  // injection. |render_view_host| may be used to route injection messages to
  // the appropriate RenderView.
  virtual void OnJsInjectionReady(RenderViewHost* render_view_host) = 0;

 protected:
  virtual ~JsInjectionReadyObserver() {}
};

#endif  // CHROME_TEST_BASE_JS_INJECTION_READY_OBSERVER_H_
