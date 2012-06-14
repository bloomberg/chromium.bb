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
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rule.h"
#include "chrome/common/extensions/api/events.h"
#include "googleurl/src/gurl.h"
#include "unicode/regex.h"

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
    ACTION_REDIRECT_BY_REGEX_DOCUMENT,
    ACTION_SET_REQUEST_HEADER,
    ACTION_REMOVE_REQUEST_HEADER,
    ACTION_ADD_RESPONSE_HEADER,
    ACTION_REMOVE_RESPONSE_HEADER,
    ACTION_IGNORE_RULES,
  };

  WebRequestAction();
  virtual ~WebRequestAction();

  // Returns a bit vector representing extensions::RequestStages. The bit vector
  // contains a 1 for each request stage during which the condition can be
  // tested.
  virtual int GetStages() const = 0;

  virtual Type GetType() const = 0;

  // Returns the minimum priority of rules that may be evaluated after
  // this rule. Defaults to MIN_INT.
  virtual int GetMinimumPriority() const;

  // Factory method that instantiates a concrete WebRequestAction
  // implementation according to |json_action|, the representation of the
  // WebRequestAction as received from the extension API.
  // Sets |error| and returns NULL in case of a semantic error that cannot
  // be caught by schema validation. Sets |bad_message| and returns NULL
  // in case the input is syntactically unexpected.
  static scoped_ptr<WebRequestAction> Create(const base::Value& json_action,
                                             std::string* error,
                                             bool* bad_message);

  // Returns a description of the modification to |request| caused by this
  // action.
  virtual LinkedPtrEventResponseDelta CreateDelta(
      net::URLRequest* request,
      RequestStages request_stage,
      const WebRequestRule::OptionalRequestData& optional_request_data,
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
                                                std::string* error,
                                                bool* bad_message);

  // Returns a description of the modifications to |request| caused by the
  // |actions_| that can be executed at |request_stage|.
  std::list<LinkedPtrEventResponseDelta> CreateDeltas(
      net::URLRequest* request,
      RequestStages request_stage,
      const WebRequestRule::OptionalRequestData& optional_request_data,
      const std::string& extension_id,
      const base::Time& extension_install_time) const;

  // Returns the minimum priority of rules that may be evaluated after
  // this rule. Defaults to MIN_INT.
  int GetMinimumPriority() const;

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
      const WebRequestRule::OptionalRequestData& optional_request_data,
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
      const WebRequestRule::OptionalRequestData& optional_request_data,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  GURL redirect_url_;  // Target to which the request shall be redirected.

  DISALLOW_COPY_AND_ASSIGN(WebRequestRedirectAction);
};

// Action that instructs to redirect a network request to a transparent image.
class WebRequestRedirectToTransparentImageAction : public WebRequestAction {
 public:
  WebRequestRedirectToTransparentImageAction();
  virtual ~WebRequestRedirectToTransparentImageAction();

  // Implementation of WebRequestAction:
  virtual int GetStages() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      net::URLRequest* request,
      RequestStages request_stage,
      const WebRequestRule::OptionalRequestData& optional_request_data,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRequestRedirectToTransparentImageAction);
};


// Action that instructs to redirect a network request to an empty document.
class WebRequestRedirectToEmptyDocumentAction : public WebRequestAction {
 public:
  WebRequestRedirectToEmptyDocumentAction();
  virtual ~WebRequestRedirectToEmptyDocumentAction();

  // Implementation of WebRequestAction:
  virtual int GetStages() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      net::URLRequest* request,
      RequestStages request_stage,
      const WebRequestRule::OptionalRequestData& optional_request_data,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRequestRedirectToEmptyDocumentAction);
};

// Action that instructs to redirect a network request.
class WebRequestRedirectByRegExAction : public WebRequestAction {
 public:
  // The |to_pattern| has to be passed in ICU syntax.
  // TODO(battre): Change this to Perl style when migrated to RE2.
  explicit WebRequestRedirectByRegExAction(
      scoped_ptr<icu::RegexPattern> from_pattern,
      const std::string& to_pattern);
  virtual ~WebRequestRedirectByRegExAction();

  // Conversion of capture group styles between Perl style ($1, $2, ...) and
  // RE2 (\1, \2, ...).
  static std::string PerlToRe2Style(const std::string& perl);

  // Implementation of WebRequestAction:
  virtual int GetStages() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      net::URLRequest* request,
      RequestStages request_stage,
      const WebRequestRule::OptionalRequestData& optional_request_data,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  scoped_ptr<icu::RegexPattern> from_pattern_;
  icu::UnicodeString to_pattern_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestRedirectByRegExAction);
};

// Action that instructs to set a request header.
class WebRequestSetRequestHeaderAction : public WebRequestAction {
 public:
  WebRequestSetRequestHeaderAction(const std::string& name,
                                   const std::string& value);
  virtual ~WebRequestSetRequestHeaderAction();

  // Implementation of WebRequestAction:
  virtual int GetStages() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      net::URLRequest* request,
      RequestStages request_stage,
      const WebRequestRule::OptionalRequestData& optional_request_data,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  std::string name_;
  std::string value_;
  DISALLOW_COPY_AND_ASSIGN(WebRequestSetRequestHeaderAction);
};

// Action that instructs to remove a request header.
class WebRequestRemoveRequestHeaderAction : public WebRequestAction {
 public:
  explicit WebRequestRemoveRequestHeaderAction(const std::string& name);
  virtual ~WebRequestRemoveRequestHeaderAction();

  // Implementation of WebRequestAction:
  virtual int GetStages() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      net::URLRequest* request,
      RequestStages request_stage,
      const WebRequestRule::OptionalRequestData& optional_request_data,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  std::string name_;
  DISALLOW_COPY_AND_ASSIGN(WebRequestRemoveRequestHeaderAction);
};

// Action that instructs to add a response header.
class WebRequestAddResponseHeaderAction : public WebRequestAction {
 public:
  WebRequestAddResponseHeaderAction(const std::string& name,
                                    const std::string& value);
  virtual ~WebRequestAddResponseHeaderAction();

  // Implementation of WebRequestAction:
  virtual int GetStages() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      net::URLRequest* request,
      RequestStages request_stage,
      const WebRequestRule::OptionalRequestData& optional_request_data,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  std::string name_;
  std::string value_;
  DISALLOW_COPY_AND_ASSIGN(WebRequestAddResponseHeaderAction);
};

// Action that instructs to remove a response header.
class WebRequestRemoveResponseHeaderAction : public WebRequestAction {
 public:
  explicit WebRequestRemoveResponseHeaderAction(const std::string& name,
                                                const std::string& value,
                                                bool has_value);
  virtual ~WebRequestRemoveResponseHeaderAction();

  // Implementation of WebRequestAction:
  virtual int GetStages() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      net::URLRequest* request,
      RequestStages request_stage,
      const WebRequestRule::OptionalRequestData& optional_request_data,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  std::string name_;
  std::string value_;
  bool has_value_;
  DISALLOW_COPY_AND_ASSIGN(WebRequestRemoveResponseHeaderAction);
};

// Action that instructs to ignore rules below a certain priority.
class WebRequestIgnoreRulesAction : public WebRequestAction {
 public:
  explicit WebRequestIgnoreRulesAction(int minimum_priority);
  virtual ~WebRequestIgnoreRulesAction();

  // Implementation of WebRequestAction:
  virtual int GetStages() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual int GetMinimumPriority() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      net::URLRequest* request,
      RequestStages request_stage,
      const WebRequestRule::OptionalRequestData& optional_request_data,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  int minimum_priority_;
  DISALLOW_COPY_AND_ASSIGN(WebRequestIgnoreRulesAction);
};

// TODO(battre) Implement further actions:
// Redirect by RegEx, Cookie manipulations, ...

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_ACTION_H_
