// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_action.h"

#include <limits>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_webrequest/request_stages.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "chrome/browser/extensions/api/web_request/web_request_api_helpers.h"
#include "net/url_request/url_request.h"

namespace extensions {

namespace keys = declarative_webrequest_constants;

namespace {
// Error messages.
const char kInvalidInstanceTypeError[] =
    "An action has an invalid instanceType: %s";

const char kTransparentImageUrl[] = "data:image/png;base64,iVBORw0KGgoAAAANSUh"
    "EUgAAAAEAAAABCAYAAAAfFcSJAAAACklEQVR4nGMAAQAABQABDQottAAAAABJRU5ErkJggg==";
const char kEmptyDocumentUrl[] = "data:text/html,";

#define INPUT_FORMAT_VALIDATE(test) do { \
    if (!(test)) { \
      *bad_message = true; \
      return scoped_ptr<WebRequestAction>(NULL); \
    } \
  } while (0)

// Helper function for WebRequestActions that can be instantiated by just
// calling the constructor.
template <class T>
scoped_ptr<WebRequestAction> CallConstructorFactoryMethod(
    const base::DictionaryValue* dict,
    std::string* error,
    bool* bad_message) {
  return scoped_ptr<WebRequestAction>(new T);
}

scoped_ptr<WebRequestAction> CreateRedirectRequestAction(
    const base::DictionaryValue* dict,
    std::string* error,
    bool* bad_message) {
  std::string redirect_url_string;
  INPUT_FORMAT_VALIDATE(
      dict->GetString(keys::kRedirectUrlKey, &redirect_url_string));
  GURL redirect_url(redirect_url_string);
  return scoped_ptr<WebRequestAction>(
      new WebRequestRedirectAction(redirect_url));
}

scoped_ptr<WebRequestAction> CreateSetRequestHeaderAction(
    const base::DictionaryValue* dict,
    std::string* error,
    bool* bad_message) {
  std::string name;
  std::string value;
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kNameKey, &name));
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kValueKey, &value));
  return scoped_ptr<WebRequestAction>(
      new WebRequestSetRequestHeaderAction(name, value));
}

scoped_ptr<WebRequestAction> CreateRemoveRequestHeaderAction(
    const base::DictionaryValue* dict,
    std::string* error,
    bool* bad_message) {
  std::string name;
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kNameKey, &name));
  return scoped_ptr<WebRequestAction>(
      new WebRequestRemoveRequestHeaderAction(name));
}

scoped_ptr<WebRequestAction> CreateAddResponseHeaderAction(
    const base::DictionaryValue* dict,
    std::string* error,
    bool* bad_message) {
  std::string name;
  std::string value;
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kNameKey, &name));
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kValueKey, &value));
  return scoped_ptr<WebRequestAction>(
      new WebRequestAddResponseHeaderAction(name, value));
}

scoped_ptr<WebRequestAction> CreateRemoveResponseHeaderAction(
    const base::DictionaryValue* dict,
    std::string* error,
    bool* bad_message) {
  std::string name;
  std::string value;
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kNameKey, &name));
  bool has_value = dict->GetString(keys::kValueKey, &value);
  return scoped_ptr<WebRequestAction>(
      new WebRequestRemoveResponseHeaderAction(name, value, has_value));
}

scoped_ptr<WebRequestAction> CreateIgnoreRulesAction(
    const base::DictionaryValue* dict,
    std::string* error,
    bool* bad_message) {
  int minium_priority;
  INPUT_FORMAT_VALIDATE(
      dict->GetInteger(keys::kLowerPriorityThanKey, &minium_priority));
  return scoped_ptr<WebRequestAction>(
      new WebRequestIgnoreRulesAction(minium_priority));
}

struct WebRequestActionFactory {
  // Factory methods for WebRequestAction instances. |dict| contains the json
  // dictionary that describes the action. |error| is used to return error
  // messages in case the extension passed an action that was syntactically
  // correct but semantically incorrect. |bad_message| is set to true in case
  // |dict| does not confirm to the validated JSON specification.
  typedef scoped_ptr<WebRequestAction>
      (* FactoryMethod)(const base::DictionaryValue* /* dict */ ,
                        std::string* /* error */,
                        bool* /* bad_message */);
  std::map<std::string, FactoryMethod> factory_methods;

  WebRequestActionFactory() {
    factory_methods[keys::kAddResponseHeaderType] =
        &CreateAddResponseHeaderAction;
    factory_methods[keys::kCancelRequestType] =
        &CallConstructorFactoryMethod<WebRequestCancelAction>;
    factory_methods[keys::kRedirectRequestType] =
        &CreateRedirectRequestAction;
    factory_methods[keys::kRedirectToTransparentImageType] =
        &CallConstructorFactoryMethod<
            WebRequestRedirectToTransparentImageAction>;
    factory_methods[keys::kRedirectToEmptyDocumentType] =
        &CallConstructorFactoryMethod<WebRequestRedirectToEmptyDocumentAction>;
    factory_methods[keys::kSetRequestHeaderType] =
        &CreateSetRequestHeaderAction;
    factory_methods[keys::kRemoveRequestHeaderType] =
        &CreateRemoveRequestHeaderAction;
    factory_methods[keys::kRemoveResponseHeaderType] =
        &CreateRemoveResponseHeaderAction;
    factory_methods[keys::kIgnoreRulesType] =
        &CreateIgnoreRulesAction;
  }
};

base::LazyInstance<WebRequestActionFactory>::Leaky
    g_web_request_action_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

//
// WebRequestAction
//

WebRequestAction::WebRequestAction() {}

WebRequestAction::~WebRequestAction() {}

int WebRequestAction::GetMinimumPriority() const {
  return std::numeric_limits<int>::min();
}

// static
scoped_ptr<WebRequestAction> WebRequestAction::Create(
    const base::Value& json_action,
    std::string* error,
    bool* bad_message) {
  *error = "";
  *bad_message = false;

  const base::DictionaryValue* action_dict = NULL;
  INPUT_FORMAT_VALIDATE(json_action.GetAsDictionary(&action_dict));

  std::string instance_type;
  INPUT_FORMAT_VALIDATE(
      action_dict->GetString(keys::kInstanceTypeKey, &instance_type));

  WebRequestActionFactory& factory = g_web_request_action_factory.Get();
  std::map<std::string, WebRequestActionFactory::FactoryMethod>::iterator
      factory_method_iter = factory.factory_methods.find(instance_type);
  if (factory_method_iter != factory.factory_methods.end())
    return (*factory_method_iter->second)(action_dict, error, bad_message);

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
    std::string* error,
    bool* bad_message) {
  *error = "";
  *bad_message = false;
  Actions result;

  for (AnyVector::const_iterator i = actions.begin();
       i != actions.end(); ++i) {
    CHECK(i->get());
    scoped_ptr<WebRequestAction> action =
        WebRequestAction::Create((*i)->value(), error, bad_message);
    if (!error->empty() || *bad_message)
      return scoped_ptr<WebRequestActionSet>(NULL);
    result.push_back(make_linked_ptr(action.release()));
  }

  return scoped_ptr<WebRequestActionSet>(new WebRequestActionSet(result));
}

std::list<LinkedPtrEventResponseDelta> WebRequestActionSet::CreateDeltas(
    net::URLRequest* request,
    RequestStages request_stage,
    const WebRequestRule::OptionalRequestData& optional_request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  std::list<LinkedPtrEventResponseDelta> result;
  for (Actions::const_iterator i = actions_.begin(); i != actions_.end(); ++i) {
    if ((*i)->GetStages() & request_stage) {
      LinkedPtrEventResponseDelta delta = (*i)->CreateDelta(request,
          request_stage, optional_request_data, extension_id,
          extension_install_time);
      if (delta.get())
        result.push_back(delta);
    }
  }
  return result;
}

int WebRequestActionSet::GetMinimumPriority() const {
  int minimum_priority = std::numeric_limits<int>::min();
  for (Actions::const_iterator i = actions_.begin(); i != actions_.end(); ++i) {
    minimum_priority = std::max(minimum_priority, (*i)->GetMinimumPriority());
  }
  return minimum_priority;
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
    const WebRequestRule::OptionalRequestData& optional_request_data,
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
    const WebRequestRule::OptionalRequestData& optional_request_data,
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
    const WebRequestRule::OptionalRequestData& optional_request_data,
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
    const WebRequestRule::OptionalRequestData& optional_request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_stage & GetStages());
  LinkedPtrEventResponseDelta result(
      new extension_web_request_api_helpers::EventResponseDelta(
          extension_id, extension_install_time));
  result->new_url = GURL(kEmptyDocumentUrl);
  return result;
}

//
// WebRequestSetRequestHeaderAction
//

WebRequestSetRequestHeaderAction::WebRequestSetRequestHeaderAction(
    const std::string& name,
    const std::string& value)
    : name_(name),
      value_(value) {
}

WebRequestSetRequestHeaderAction::~WebRequestSetRequestHeaderAction() {}

int WebRequestSetRequestHeaderAction::GetStages() const {
  return ON_BEFORE_SEND_HEADERS;
}

WebRequestAction::Type
WebRequestSetRequestHeaderAction::GetType() const {
  return WebRequestAction::ACTION_SET_REQUEST_HEADER;
}

LinkedPtrEventResponseDelta
WebRequestSetRequestHeaderAction::CreateDelta(
    net::URLRequest* request,
    RequestStages request_stage,
    const WebRequestRule::OptionalRequestData& optional_request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_stage & GetStages());
  LinkedPtrEventResponseDelta result(
      new extension_web_request_api_helpers::EventResponseDelta(
          extension_id, extension_install_time));
  result->modified_request_headers.SetHeader(name_, value_);
  return result;
}

//
// WebRequestRemoveRequestHeaderAction
//

WebRequestRemoveRequestHeaderAction::WebRequestRemoveRequestHeaderAction(
    const std::string& name)
    : name_(name) {
}

WebRequestRemoveRequestHeaderAction::~WebRequestRemoveRequestHeaderAction() {}

int WebRequestRemoveRequestHeaderAction::GetStages() const {
  return ON_BEFORE_SEND_HEADERS;
}

WebRequestAction::Type
WebRequestRemoveRequestHeaderAction::GetType() const {
  return WebRequestAction::ACTION_REMOVE_REQUEST_HEADER;
}

LinkedPtrEventResponseDelta
WebRequestRemoveRequestHeaderAction::CreateDelta(
    net::URLRequest* request,
    RequestStages request_stage,
    const WebRequestRule::OptionalRequestData& optional_request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_stage & GetStages());
  LinkedPtrEventResponseDelta result(
      new extension_web_request_api_helpers::EventResponseDelta(
          extension_id, extension_install_time));
  result->deleted_request_headers.push_back(name_);
  return result;
}

//
// WebRequestAddResponseHeaderAction
//

WebRequestAddResponseHeaderAction::WebRequestAddResponseHeaderAction(
    const std::string& name,
    const std::string& value)
    : name_(name),
      value_(value) {
}

WebRequestAddResponseHeaderAction::~WebRequestAddResponseHeaderAction() {}

int WebRequestAddResponseHeaderAction::GetStages() const {
  return ON_HEADERS_RECEIVED;
}

WebRequestAction::Type
WebRequestAddResponseHeaderAction::GetType() const {
  return WebRequestAction::ACTION_ADD_RESPONSE_HEADER;
}

LinkedPtrEventResponseDelta
WebRequestAddResponseHeaderAction::CreateDelta(
    net::URLRequest* request,
    RequestStages request_stage,
    const WebRequestRule::OptionalRequestData& optional_request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_stage & GetStages());
  LinkedPtrEventResponseDelta result(
      new extension_web_request_api_helpers::EventResponseDelta(
          extension_id, extension_install_time));

  net::HttpResponseHeaders* headers =
      optional_request_data.original_response_headers;
  if (!headers)
    return result;

  // Don't generate the header if it exists already.
  if (headers->HasHeaderValue(name_, value_))
    return result;

  result->added_response_headers.push_back(make_pair(name_, value_));
  return result;
}

//
// WebRequestRemoveResponseHeaderAction
//

WebRequestRemoveResponseHeaderAction::WebRequestRemoveResponseHeaderAction(
    const std::string& name,
    const std::string& value,
    bool has_value)
    : name_(name),
      value_(value),
      has_value_(has_value) {
}

WebRequestRemoveResponseHeaderAction::~WebRequestRemoveResponseHeaderAction() {}

int WebRequestRemoveResponseHeaderAction::GetStages() const {
  return ON_HEADERS_RECEIVED;
}

WebRequestAction::Type
WebRequestRemoveResponseHeaderAction::GetType() const {
  return WebRequestAction::ACTION_REMOVE_RESPONSE_HEADER;
}

LinkedPtrEventResponseDelta
WebRequestRemoveResponseHeaderAction::CreateDelta(
    net::URLRequest* request,
    RequestStages request_stage,
    const WebRequestRule::OptionalRequestData& optional_request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_stage & GetStages());
  LinkedPtrEventResponseDelta result(
      new extension_web_request_api_helpers::EventResponseDelta(
          extension_id, extension_install_time));
  net::HttpResponseHeaders* headers =
      optional_request_data.original_response_headers;
  if (!headers)
    return result;
  void* iter = NULL;
  std::string current_value;
  while (headers->EnumerateHeader(&iter, name_, &current_value)) {
    if (has_value_ &&
           (current_value.size() != value_.size() ||
            !std::equal(current_value.begin(), current_value.end(),
                        value_.begin(),
                        base::CaseInsensitiveCompare<char>()))) {
      continue;
    }
    result->deleted_response_headers.push_back(make_pair(name_, current_value));
  }
  return result;
}

//
// WebRequestIgnoreRulesAction
//

WebRequestIgnoreRulesAction::WebRequestIgnoreRulesAction(
    int minimum_priority)
    : minimum_priority_(minimum_priority) {
}

WebRequestIgnoreRulesAction::~WebRequestIgnoreRulesAction() {}

int WebRequestIgnoreRulesAction::GetStages() const {
  return ON_BEFORE_REQUEST | ON_BEFORE_SEND_HEADERS | ON_HEADERS_RECEIVED |
      ON_AUTH_REQUIRED;
}

WebRequestAction::Type WebRequestIgnoreRulesAction::GetType() const {
  return WebRequestAction::ACTION_IGNORE_RULES;
}

int WebRequestIgnoreRulesAction::GetMinimumPriority() const {
  return minimum_priority_;
}

LinkedPtrEventResponseDelta WebRequestIgnoreRulesAction::CreateDelta(
    net::URLRequest* request,
    RequestStages request_stage,
    const WebRequestRule::OptionalRequestData& optional_request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_stage & GetStages());
  return LinkedPtrEventResponseDelta(NULL);
}

}  // namespace extensions
