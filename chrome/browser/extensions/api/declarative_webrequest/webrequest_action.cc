// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_action.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_webrequest/request_stages.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "chrome/browser/extensions/api/web_request/web_request_api_helpers.h"
#include "net/url_request/url_request.h"

namespace extensions {

namespace keys = declarative_webrequest_constants;

namespace {
// Error messages.
const char kExpectedDictionary[] = "Expected a dictionary as action.";
const char kInvalidInstanceTypeError[] =
    "An action has an invalid instanceType: %s";
const char kMissingRedirectUrl[] = "No redirection target specified.";

const char kTransparentImageUrl[] = "data:image/png;base64,iVBORw0KGgoAAAANSUh"
    "EUgAAAAEAAAABCAYAAAAfFcSJAAAACklEQVR4nGMAAQAABQABDQottAAAAABJRU5ErkJggg==";
const char kEmptyDocumentUrl[] = "data:text/html,";
}  // namespace

//
// WebRequestAction
//

WebRequestAction::WebRequestAction() {}

WebRequestAction::~WebRequestAction() {}

// static
scoped_ptr<WebRequestAction> WebRequestAction::Create(
    const base::Value& json_action,
    std::string* error) {
  const base::DictionaryValue* action_dict = NULL;
  if (!json_action.GetAsDictionary(&action_dict)) {
    *error = kExpectedDictionary;
    return scoped_ptr<WebRequestAction>(NULL);
  }

  std::string instance_type = "No instanceType";
  if (!action_dict->GetString(keys::kInstanceTypeKey, &instance_type)) {
    *error = base::StringPrintf(kInvalidInstanceTypeError,
                                instance_type.c_str());
    return scoped_ptr<WebRequestAction>(NULL);
  }

  // TODO(battre): Change this into a proper factory.
  *error = "";
  if (instance_type == keys::kCancelRequestType) {
    return scoped_ptr<WebRequestAction>(new WebRequestCancelAction);
  } else if (instance_type == keys::kRedirectRequestType) {
    std::string redirect_url_string;
    if (!action_dict->GetString(keys::kRedirectUrlKey, &redirect_url_string)) {
      *error = kMissingRedirectUrl;
      return scoped_ptr<WebRequestAction>(NULL);
    }
    GURL redirect_url(redirect_url_string);
    return scoped_ptr<WebRequestAction>(
        new WebRequestRedirectAction(redirect_url));
  } else if (instance_type == keys::kRedirectToTransparentImageType) {
    return scoped_ptr<WebRequestAction>(
        new WebRequestRedirectToTransparentImageAction);
  } else if (instance_type == keys::kRedirectToEmptyDocumentType) {
    return scoped_ptr<WebRequestAction>(
        new WebRequestRedirectToEmptyDocumentAction);
  }

  *error = base::StringPrintf(kInvalidInstanceTypeError, instance_type.c_str());
  return scoped_ptr<WebRequestAction>();
}


//
// WebRequestActionSet
//

WebRequestActionSet::WebRequestActionSet(const Actions& actions)
    : actions_(actions) {}

WebRequestActionSet::~WebRequestActionSet() {}

// static
scoped_ptr<WebRequestActionSet> WebRequestActionSet::Create(
    const AnyVector& actions,
    std::string* error) {
  Actions result;

  for (AnyVector::const_iterator i = actions.begin();
       i != actions.end(); ++i) {
    CHECK(i->get());
    scoped_ptr<WebRequestAction> action =
        WebRequestAction::Create((*i)->value(), error);
    if (!error->empty())
      return scoped_ptr<WebRequestActionSet>(NULL);
    result.push_back(make_linked_ptr(action.release()));
  }

  return scoped_ptr<WebRequestActionSet>(new WebRequestActionSet(result));
}

std::list<LinkedPtrEventResponseDelta> WebRequestActionSet::CreateDeltas(
    net::URLRequest* request,
    RequestStages request_stage,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  std::list<LinkedPtrEventResponseDelta> result;
  for (Actions::const_iterator i = actions_.begin(); i != actions_.end(); ++i) {
    if ((*i)->GetStages() & request_stage) {
      LinkedPtrEventResponseDelta delta = (*i)->CreateDelta(request,
          request_stage, extension_id, extension_install_time);
      if (delta.get())
        result.push_back(delta);
    }
  }
  return result;
}

//
// WebRequestCancelAction
//

WebRequestCancelAction::WebRequestCancelAction() {}

WebRequestCancelAction::~WebRequestCancelAction() {}

int WebRequestCancelAction::GetStages() const {
  return ON_BEFORE_REQUEST | ON_BEFORE_SEND_HEADERS | ON_HEADERS_RECEIVED |
      ON_AUTH_REQUIRED;
}

WebRequestAction::Type WebRequestCancelAction::GetType() const {
  return WebRequestAction::ACTION_CANCEL_REQUEST;
}

LinkedPtrEventResponseDelta WebRequestCancelAction::CreateDelta(
    net::URLRequest* request,
    RequestStages request_stage,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_stage & GetStages());
  LinkedPtrEventResponseDelta result(
      new extension_web_request_api_helpers::EventResponseDelta(
          extension_id, extension_install_time));
  result->cancel = true;
  return result;
}

//
// WebRequestRedirectAction
//

WebRequestRedirectAction::WebRequestRedirectAction(const GURL& redirect_url)
    : redirect_url_(redirect_url) {}

WebRequestRedirectAction::~WebRequestRedirectAction() {}

int WebRequestRedirectAction::GetStages() const {
  return ON_BEFORE_REQUEST;
}

WebRequestAction::Type WebRequestRedirectAction::GetType() const {
  return WebRequestAction::ACTION_REDIRECT_REQUEST;
}

LinkedPtrEventResponseDelta WebRequestRedirectAction::CreateDelta(
    net::URLRequest* request,
    RequestStages request_stage,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_stage & GetStages());
  if (request->url() == redirect_url_)
    return LinkedPtrEventResponseDelta(NULL);
  LinkedPtrEventResponseDelta result(
      new extension_web_request_api_helpers::EventResponseDelta(
          extension_id, extension_install_time));
  result->new_url = redirect_url_;
  return result;
}

//
// WebRequestRedirectToTransparentImageAction
//

WebRequestRedirectToTransparentImageAction::
WebRequestRedirectToTransparentImageAction() {}

WebRequestRedirectToTransparentImageAction::
~WebRequestRedirectToTransparentImageAction() {}

int WebRequestRedirectToTransparentImageAction::GetStages() const {
  return ON_BEFORE_REQUEST;
}

WebRequestAction::Type
WebRequestRedirectToTransparentImageAction::GetType() const {
  return WebRequestAction::ACTION_REDIRECT_TO_TRANSPARENT_IMAGE;
}

LinkedPtrEventResponseDelta
WebRequestRedirectToTransparentImageAction::CreateDelta(
    net::URLRequest* request,
    RequestStages request_stage,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_stage & GetStages());
  LinkedPtrEventResponseDelta result(
      new extension_web_request_api_helpers::EventResponseDelta(
          extension_id, extension_install_time));
  result->new_url = GURL(kTransparentImageUrl);
  return result;
}

//
// WebRequestRedirectToEmptyDocumentAction
//

WebRequestRedirectToEmptyDocumentAction::
WebRequestRedirectToEmptyDocumentAction() {}

WebRequestRedirectToEmptyDocumentAction::
~WebRequestRedirectToEmptyDocumentAction() {}

int WebRequestRedirectToEmptyDocumentAction::GetStages() const {
  return ON_BEFORE_REQUEST;
}

WebRequestAction::Type
WebRequestRedirectToEmptyDocumentAction::GetType() const {
  return WebRequestAction::ACTION_REDIRECT_TO_EMPTY_DOCUMENT;
}

LinkedPtrEventResponseDelta
WebRequestRedirectToEmptyDocumentAction::CreateDelta(
    net::URLRequest* request,
    RequestStages request_stage,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_stage & GetStages());
  LinkedPtrEventResponseDelta result(
      new extension_web_request_api_helpers::EventResponseDelta(
          extension_id, extension_install_time));
  result->new_url = GURL(kEmptyDocumentUrl);
  return result;
}

}  // namespace extensions
