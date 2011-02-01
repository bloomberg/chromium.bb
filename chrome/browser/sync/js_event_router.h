// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_JS_EVENT_ROUTER_H_
#define CHROME_BROWSER_SYNC_JS_EVENT_ROUTER_H_
#pragma once

// See README.js for design comments.

#include <string>

namespace browser_sync {

class JsArgList;
class JsEventHandler;

// An interface for objects that don't directly handle Javascript
// events but can pass them to JsEventHandlers or route them to other
// JsEventRouters.
class JsEventRouter {
 public:
  // If |target| is NULL that means the event is intended for every
  // handler.  Otherwise the event is meant for the given target only.
  // |target| is const because it shouldn't be used except by the
  // router that directly knows about it (which can then safely cast
  // away the constness).
  virtual void RouteJsEvent(const std::string& name, const JsArgList& args,
                            const JsEventHandler* target) = 0;

 protected:
  virtual ~JsEventRouter() {}
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_JS_EVENT_ROUTER_H_
