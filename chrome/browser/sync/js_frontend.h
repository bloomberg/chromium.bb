// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_JS_FRONTEND_H_
#define CHROME_BROWSER_SYNC_JS_FRONTEND_H_
#pragma once

// See README.js for design comments.

#include <string>

namespace browser_sync {

class JsArgList;
class JsEventHandler;

// An interface for objects that interact directly with
// JsEventHandlers.
class JsFrontend {
 public:
  // Adds a handler which will start receiving JS events (not
  // immediately, so this can be called in the handler's constructor).
  // A handler must be added at most once.
  virtual void AddHandler(JsEventHandler* handler) = 0;

  // Removes the given handler if it has been added.  It will
  // immediately stop receiving any JS events.
  virtual void RemoveHandler(JsEventHandler* handler) = 0;

  // Sends a JS message.  The reply (if any) will be sent to |sender|
  // if it has been added.
  virtual void ProcessMessage(
      const std::string& name, const JsArgList& args,
      const JsEventHandler* sender) = 0;

 protected:
  virtual ~JsFrontend() {}
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_JS_FRONTEND_H_
