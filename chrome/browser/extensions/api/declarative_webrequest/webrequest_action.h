// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_ACTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_ACTION_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "tools/json_schema_compiler/any.h"

namespace base {
class Value;
}

namespace extensions {

// Base class for all WebRequestActions of the declarative Web Request API.
//
// TODO(battre): Add method that corresponds to executing the action.
class WebRequestAction {
 public:
  // Type identifiers for concrete WebRequestActions.
  enum Type {
    ACTION_CANCEL_REQUEST
  };

  WebRequestAction();
  virtual ~WebRequestAction();

  // Returns a bit vector representing extensions::RequestStages. The bit vector
  // contains a 1 for each request stage during which the condition can be
  // tested.
  virtual int GetStages() const = 0;

  virtual Type GetType() const = 0;

  // Factory method that instantiates a concrete WebRequestAction
  // implementation according to |json_action|, the representation of the
  // WebRequestAction as received from the extension API.
  // Sets |error| and returns NULL in case of an error.
  static scoped_ptr<WebRequestAction> Create(const base::Value& json_action,
                                             std::string* error);
};

// Immutable container for multiple actions.
//
// TODO(battre): As WebRequestActionSet can become the single owner of all
// actions, we can optimize here by making some of them singletons (e.g. Cancel
// actions).
//
// TODO(battre): Add method that corresponds to executing the action.
class WebRequestActionSet {
 public:
  typedef std::vector<linked_ptr<json_schema_compiler::any::Any> > AnyVector;
  typedef std::vector<linked_ptr<WebRequestAction> > Actions;

  explicit WebRequestActionSet(const Actions& actions);
  virtual ~WebRequestActionSet();

  // Factory method that instantiates a WebRequestActionSet according to
  // |actions| which represents the array of actions received from the
  // extension API.
  static scoped_ptr<WebRequestActionSet> Create(const AnyVector& actions,
                                                std::string* error);

  const Actions& actions() const { return actions_; }

 private:
  Actions actions_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestActionSet);
};

//
// The following are concrete actions.
//

// Action that instructs to cancel a network request.
class WebRequestCancelAction : public WebRequestAction {
 public:
  WebRequestCancelAction();
  virtual ~WebRequestCancelAction();

  // Implementation of WebRequestAction:
  virtual int GetStages() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRequestCancelAction);
};

// TODO(battre) Implement further actions:
// Redirect to constant url, Redirect by RegEx, Set header, Remove header, ...

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_ACTION_H_
