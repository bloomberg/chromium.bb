// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_JS_EVENT_HANDLER_H_
#define CHROME_BROWSER_SYNC_JS_EVENT_HANDLER_H_
#pragma once

// See README.js for design comments.

#include <string>

namespace browser_sync {

class JsArgList;

// An interface for objects that handle Javascript events (e.g.,
// WebUIs).
class JsEventHandler {
 public:
  virtual void HandleJsEvent(
      const std::string& name, const JsArgList& args) = 0;

 protected:
  virtual ~JsEventHandler() {}
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_JS_EVENT_HANDLER_H_
