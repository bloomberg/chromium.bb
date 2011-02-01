// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_JS_BACKEND_H_
#define CHROME_BROWSER_SYNC_JS_BACKEND_H_
#pragma once

// See README.js for design comments.

#include <string>

namespace browser_sync {

class JsArgList;
class JsEventHandler;
class JsEventRouter;

class JsBackend {
 public:
  // Sets the JS event router to which all backend events will be
  // sent.
  virtual void SetParentJsEventRouter(JsEventRouter* router) = 0;

  // Removes any existing JS event router.
  virtual void RemoveParentJsEventRouter() = 0;

  // Gets the crurent JS event router, or NULL if there is none.  Used
  // for testing.
  virtual const JsEventRouter* GetParentJsEventRouter() const = 0;

  // Processes the given message.  All reply events are sent to the
  // parent JS event router (if set).
  virtual void ProcessMessage(
      const std::string& name, const JsArgList& args,
      const JsEventHandler* sender) = 0;

 protected:
  virtual ~JsBackend() {}
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_JS_BACKEND_H_
