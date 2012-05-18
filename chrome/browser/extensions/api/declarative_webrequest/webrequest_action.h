// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_ACTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_ACTION_H_
#pragma once

#include <list>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "chrome/browser/extensions/api/declarative_webrequest/request_stages.h"
#include "chrome/common/extensions/api/events.h"
#include "googleurl/src/gurl.h"

namespace base {
class DictionaryValue;
class Time;
class Value;
}

namespace extension_web_request_api_helpers {
struct EventResponseDelta;
}

namespace net {
class URLRequest;
}

namespace extensions {

typedef linked_ptr<extension_web_request_api_helpers::EventResponseDelta>
    LinkedPtrEventResponseDelta;

// Base class for all WebRequestActions of the declarative Web Request API.
//
// TODO(battre): Add method that corresponds to executing the action.
class WebRequestAction {
 public:
  // Type identifiers for concrete WebRequestActions.
  enum Type {
    ACTION_CANCEL_REQUEST,
    ACTION_REDIRECT_REQUEST,
    ACTION_REDIRECT_TO_TRANSPARENT_IMAGE,
    ACTION_REDIRECT_TO_EMPTY_DOCUMENT,
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

  // Returns a description of the modification to |request| caused by this
  // action.
  virtual LinkedPtrEventResponseDelta CreateDelta(
      net::URLRequest* request,
      RequestStages request_stage,
      const std::string& extension_id,
      const base::Time& extension_install_time) const = 0;
};

// Immutable container for multiple actions.
//
// TODO(battre): As WebRequestActionSet can become the single owner of all
// actions, we can optimize here by making some of them singletons (e.g. Cancel
// actions).
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

  // Returns a description of the modifications to |request| caused by the
  // |actions_| that can be executed at |request_stage|.
  std::list<LinkedPtrEventResponseDelta> CreateDeltas(
      net::URLRequest* request,
      RequestStages request_stage,
      const std::string& extension_id,
      const base::Time& extension_install_time) const;

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
  virtual LinkedPtrEventResponseDelta CreateDelta(
      net::URLRequest* request,
      RequestStages request_stage,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRequestCancelAction);
};

// Action that instructs to redirect a network request.
class WebRequestRedirectAction : public WebRequestAction {
 public:
  explicit WebRequestRedirectAction(const GURL& redirect_url);
  virtual ~WebRequestRedirectAction();

  // Implementation of WebRequestAction:
  virtual int GetStages() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      net::URLRequest* request,
      RequestStages request_stage,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  GURL redirect_url_;  // Target to which the request shall be redirected.

  DISALLOW_COPY_AND_ASSIGN(WebRequestRedirectAction);
};

// Action that instructs to redirect a network request to a transparent image.
class WebRequestRedirectToTransparentImageAction : public WebRequestAction {
 public:
  explicit WebRequestRedirectToTransparentImageAction();
  virtual ~WebRequestRedirectToTransparentImageAction();

  // Implementation of WebRequestAction:
  virtual int GetStages() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      net::URLRequest* request,
      RequestStages request_stage,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRequestRedirectToTransparentImageAction);
};


// Action that instructs to redirect a network request to an empty document.
class WebRequestRedirectToEmptyDocumentAction : public WebRequestAction {
 public:
  explicit WebRequestRedirectToEmptyDocumentAction();
  virtual ~WebRequestRedirectToEmptyDocumentAction();

  // Implementation of WebRequestAction:
  virtual int GetStages() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      net::URLRequest* request,
      RequestStages request_stage,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRequestRedirectToEmptyDocumentAction);
};

// TODO(battre) Implement further actions:
// Redirect to constant url, Redirect by RegEx, Set header, Remove header, ...

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_ACTION_H_
