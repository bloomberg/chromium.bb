// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_ACTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_ACTION_H_

#include <list>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "chrome/browser/extensions/api/declarative/declarative_rule.h"
#include "chrome/browser/extensions/api/declarative_webrequest/request_stage.h"
#include "chrome/browser/extensions/api/web_request/web_request_api_helpers.h"
#include "chrome/common/extensions/api/events.h"
#include "googleurl/src/gurl.h"

class ExtensionInfoMap;
class WebRequestPermission;

namespace base {
class DictionaryValue;
class Time;
class Value;
}

namespace extension_web_request_api_helpers {
struct EventResponseDelta;
}

namespace extensions {
class Extension;
struct DeclarativeWebRequestData;
}

namespace net {
class URLRequest;
}

namespace re2 {
class RE2;
}

namespace extensions {

typedef linked_ptr<extension_web_request_api_helpers::EventResponseDelta>
    LinkedPtrEventResponseDelta;

// Base class for all WebRequestActions of the declarative Web Request API.
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
    ACTION_MODIFY_REQUEST_COOKIE,
    ACTION_MODIFY_RESPONSE_COOKIE,
    ACTION_SEND_MESSAGE_TO_EXTENSION,
  };

  // Strategies for checking host permissions.
  enum HostPermissionsStrategy {
    STRATEGY_NONE,  // Do not check host permissions.
    STRATEGY_DEFAULT,  // Check host permissions in HasPermission,
                       // before creating the delta.
    STRATEGY_ALLOW_SAME_DOMAIN,  // Skip host permission checks if the request
                                 // URL and new URL have the same domain.
                                 // Do these checks in DeltaHasPermission,
                                 // after creating the delta.
  };

  // Information necessary to decide how to apply a WebRequestAction
  // inside a matching rule.
  struct ApplyInfo {
    const ExtensionInfoMap* extension_info_map;
    const DeclarativeWebRequestData& request_data;
    bool crosses_incognito;
    // Modified by each applied action:
    std::list<LinkedPtrEventResponseDelta>* deltas;
  };

  WebRequestAction();
  virtual ~WebRequestAction();

  // Returns a bit vector representing extensions::RequestStage. The bit vector
  // contains a 1 for each request stage during which the condition can be
  // tested.
  virtual int GetStages() const = 0;

  virtual Type GetType() const = 0;

  // Returns the minimum priority of rules that may be evaluated after
  // this rule. Defaults to MIN_INT.
  virtual int GetMinimumPriority() const;

  // Returns whether host permissions checks depend on the resulting delta
  // and therefore must be checked in DeltaHasPermission, after the delta
  // is created, rather than in HasPermission, before it is created.
  // Defaults to STRATEGY_DEFAULT.
  virtual HostPermissionsStrategy GetHostPermissionsStrategy() const;

  // Returns whether the specified extension has permission to execute this
  // action on |request|. Checks the host permission if the host permissions
  // strategy is STRATEGY_DEFAULT.
  // |extension_info_map| may only be NULL for during testing, in which case
  // host permissions are ignored. |crosses_incognito| specifies
  // whether the request comes from a different profile than |extension_id|
  // but was processed because the extension is in spanning mode.
  virtual bool HasPermission(const ExtensionInfoMap* extension_info_map,
                             const std::string& extension_id,
                             const net::URLRequest* request,
                             bool crosses_incognito) const;

  // Returns whether the specified extension has permission to modify the
  // |request| with this |delta|. This check is in addition to HasPermission;
  // if either fails, the request will not be modified. Unlike HasPermission,
  // it runs after the change is created, so it can use the full information
  // about what the change would be. Checks the host permission if the strategy
  // is STRATEGY_ALLOW_SAME_DOMAIN.
  virtual bool DeltaHasPermission(
      const ExtensionInfoMap* extension_info_map,
      const std::string& extension_id,
      const net::URLRequest* request,
      bool crosses_incognito,
      const LinkedPtrEventResponseDelta& delta) const;

  // Factory method that instantiates a concrete WebRequestAction
  // implementation according to |json_action|, the representation of the
  // WebRequestAction as received from the extension API.
  // Sets |error| and returns NULL in case of a semantic error that cannot
  // be caught by schema validation. Sets |bad_message| and returns NULL
  // in case the input is syntactically unexpected.
  static scoped_ptr<WebRequestAction> Create(const base::Value& json_action,
                                             std::string* error,
                                             bool* bad_message);

  // Returns a description of the modification to the request caused by
  // this action.
  virtual LinkedPtrEventResponseDelta CreateDelta(
      const DeclarativeWebRequestData& request_data,
      const std::string& extension_id,
      const base::Time& extension_install_time) const = 0;

  // Applies this action to a request, recording the results into
  // apply_info.deltas.
  void Apply(const std::string& extension_id,
             base::Time extension_install_time,
             ApplyInfo* apply_info) const;
};

typedef DeclarativeActionSet<WebRequestAction> WebRequestActionSet;

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
  virtual HostPermissionsStrategy GetHostPermissionsStrategy() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      const DeclarativeWebRequestData& request_data,
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
  virtual HostPermissionsStrategy GetHostPermissionsStrategy() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      const DeclarativeWebRequestData& request_data,
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
  virtual HostPermissionsStrategy GetHostPermissionsStrategy() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      const DeclarativeWebRequestData& request_data,
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
  virtual HostPermissionsStrategy GetHostPermissionsStrategy() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      const DeclarativeWebRequestData& request_data,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRequestRedirectToEmptyDocumentAction);
};

// Action that instructs to redirect a network request.
class WebRequestRedirectByRegExAction : public WebRequestAction {
 public:
  // The |to_pattern| has to be passed in RE2 syntax with the exception that
  // capture groups are referenced in Perl style ($1, $2, ...).
  explicit WebRequestRedirectByRegExAction(scoped_ptr<re2::RE2> from_pattern,
                                           const std::string& to_pattern);
  virtual ~WebRequestRedirectByRegExAction();

  // Conversion of capture group styles between Perl style ($1, $2, ...) and
  // RE2 (\1, \2, ...).
  static std::string PerlToRe2Style(const std::string& perl);

  // Implementation of WebRequestAction:
  virtual int GetStages() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual HostPermissionsStrategy GetHostPermissionsStrategy() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      const DeclarativeWebRequestData& request_data,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  scoped_ptr<re2::RE2> from_pattern_;
  std::string to_pattern_;

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
      const DeclarativeWebRequestData& request_data,
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
      const DeclarativeWebRequestData& request_data,
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
      const DeclarativeWebRequestData& request_data,
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
      const DeclarativeWebRequestData& request_data,
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
  virtual HostPermissionsStrategy GetHostPermissionsStrategy() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      const DeclarativeWebRequestData& request_data,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  int minimum_priority_;
  DISALLOW_COPY_AND_ASSIGN(WebRequestIgnoreRulesAction);
};

// Action that instructs to modify (add, edit, remove) a request cookie.
class WebRequestRequestCookieAction : public WebRequestAction {
 public:
  typedef extension_web_request_api_helpers::RequestCookieModification
      RequestCookieModification;

  explicit WebRequestRequestCookieAction(
      linked_ptr<RequestCookieModification> request_cookie_modification);
  virtual ~WebRequestRequestCookieAction();

  // Implementation of WebRequestAction:
  virtual int GetStages() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      const DeclarativeWebRequestData& request_data,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  linked_ptr<RequestCookieModification> request_cookie_modification_;
  DISALLOW_COPY_AND_ASSIGN(WebRequestRequestCookieAction);
};

// Action that instructs to modify (add, edit, remove) a response cookie.
class WebRequestResponseCookieAction : public WebRequestAction {
 public:
  typedef extension_web_request_api_helpers::ResponseCookieModification
      ResponseCookieModification;

  explicit WebRequestResponseCookieAction(
      linked_ptr<ResponseCookieModification> response_cookie_modification);
  virtual ~WebRequestResponseCookieAction();

  // Implementation of WebRequestAction:
  virtual int GetStages() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      const DeclarativeWebRequestData& request_data,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  linked_ptr<ResponseCookieModification> response_cookie_modification_;
  DISALLOW_COPY_AND_ASSIGN(WebRequestResponseCookieAction);
};

// Action that triggers the chrome.declarativeWebRequest.onMessage event in
// the background/event/... pages of the extension.
class WebRequestSendMessageToExtensionAction : public WebRequestAction {
 public:
  explicit WebRequestSendMessageToExtensionAction(const std::string& message);
  virtual ~WebRequestSendMessageToExtensionAction();

  // Implementation of WebRequestAction:
  virtual int GetStages() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual LinkedPtrEventResponseDelta CreateDelta(
      const DeclarativeWebRequestData& request_data,
      const std::string& extension_id,
      const base::Time& extension_install_time) const OVERRIDE;

 private:
  std::string message_;
  DISALLOW_COPY_AND_ASSIGN(WebRequestSendMessageToExtensionAction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_ACTION_H_
