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
#include "chrome/browser/extensions/api/declarative_webrequest/request_stage.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "chrome/browser/extensions/api/web_request/web_request_api_helpers.h"
#include "chrome/browser/extensions/api/web_request/web_request_permissions.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/common/url_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/url_request/url_request.h"
#include "third_party/re2/re2/re2.h"

namespace extensions {

namespace helpers = extension_web_request_api_helpers;
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

scoped_ptr<helpers::RequestCookie> ParseRequestCookie(
    const DictionaryValue* dict) {
  scoped_ptr<helpers::RequestCookie> result(new helpers::RequestCookie);
  std::string tmp;
  if (dict->GetString(keys::kNameKey, &tmp))
    result->name.reset(new std::string(tmp));
  if (dict->GetString(keys::kValueKey, &tmp))
    result->value.reset(new std::string(tmp));
  return result.Pass();
}

void ParseResponseCookieImpl(const DictionaryValue* dict,
                             helpers::ResponseCookie* cookie) {
  std::string string_tmp;
  int int_tmp = 0;
  bool bool_tmp = false;
  if (dict->GetString(keys::kNameKey, &string_tmp))
    cookie->name.reset(new std::string(string_tmp));
  if (dict->GetString(keys::kValueKey, &string_tmp))
    cookie->value.reset(new std::string(string_tmp));
  if (dict->GetString(keys::kExpiresKey, &string_tmp))
    cookie->expires.reset(new std::string(string_tmp));
  if (dict->GetInteger(keys::kMaxAgeKey, &int_tmp))
    cookie->max_age.reset(new int(int_tmp));
  if (dict->GetString(keys::kDomainKey, &string_tmp))
    cookie->domain.reset(new std::string(string_tmp));
  if (dict->GetString(keys::kPathKey, &string_tmp))
    cookie->path.reset(new std::string(string_tmp));
  if (dict->GetBoolean(keys::kSecureKey, &bool_tmp))
    cookie->secure.reset(new bool(bool_tmp));
  if (dict->GetBoolean(keys::kHttpOnlyKey, &bool_tmp))
    cookie->http_only.reset(new bool(bool_tmp));
}

scoped_ptr<helpers::ResponseCookie> ParseResponseCookie(
    const DictionaryValue* dict) {
  scoped_ptr<helpers::ResponseCookie> result(new helpers::ResponseCookie);
  ParseResponseCookieImpl(dict, result.get());
  return result.Pass();
}

scoped_ptr<helpers::FilterResponseCookie> ParseFilterResponseCookie(
    const DictionaryValue* dict) {
  scoped_ptr<helpers::FilterResponseCookie> result(
      new helpers::FilterResponseCookie);
  ParseResponseCookieImpl(dict, result.get());

  int int_tmp = 0;
  bool bool_tmp = false;
  if (dict->GetInteger(keys::kAgeUpperBoundKey, &int_tmp))
    result->age_upper_bound.reset(new int(int_tmp));
  if (dict->GetInteger(keys::kAgeLowerBoundKey, &int_tmp))
    result->age_lower_bound.reset(new int(int_tmp));
  if (dict->GetBoolean(keys::kSessionCookieKey, &bool_tmp))
    result->session_cookie.reset(new bool(bool_tmp));
  return result.Pass();
}

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

scoped_ptr<WebRequestAction> CreateRedirectRequestByRegExAction(
    const base::DictionaryValue* dict,
    std::string* error,
    bool* bad_message) {
  std::string from;
  std::string to;
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kFromKey, &from));
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kToKey, &to));

  to = WebRequestRedirectByRegExAction::PerlToRe2Style(to);

  RE2::Options options;
  options.set_case_sensitive(false);
  scoped_ptr<RE2> from_pattern(new RE2(from, options));

  if (!from_pattern->ok()) {
    *error = "Invalid pattern '" + from + "' -> '" + to + "'";
    return scoped_ptr<WebRequestAction>(NULL);
  }
  return scoped_ptr<WebRequestAction>(
      new WebRequestRedirectByRegExAction(from_pattern.Pass(), to));
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

scoped_ptr<WebRequestAction> CreateRequestCookieAction(
    const base::DictionaryValue* dict,
    std::string* error,
    bool* bad_message) {
  using extension_web_request_api_helpers::RequestCookieModification;

  linked_ptr<RequestCookieModification> modification(
      new RequestCookieModification);

  // Get modification type.
  std::string instance_type;
  INPUT_FORMAT_VALIDATE(
      dict->GetString(keys::kInstanceTypeKey, &instance_type));
  if (instance_type == keys::kAddRequestCookieType)
    modification->type = helpers::ADD;
  else if (instance_type == keys::kEditRequestCookieType)
    modification->type = helpers::EDIT;
  else if (instance_type == keys::kRemoveRequestCookieType)
    modification->type = helpers::REMOVE;
  else
    INPUT_FORMAT_VALIDATE(false);

  // Get filter.
  if (modification->type == helpers::EDIT ||
      modification->type == helpers::REMOVE) {
    const DictionaryValue* filter = NULL;
    INPUT_FORMAT_VALIDATE(dict->GetDictionary(keys::kFilterKey, &filter));
    modification->filter = ParseRequestCookie(filter);
  }

  // Get new value.
  if (modification->type == helpers::ADD) {
    const DictionaryValue* value = NULL;
    INPUT_FORMAT_VALIDATE(dict->GetDictionary(keys::kCookieKey, &value));
    modification->modification = ParseRequestCookie(value);
  } else if (modification->type == helpers::EDIT) {
    const DictionaryValue* value = NULL;
    INPUT_FORMAT_VALIDATE(dict->GetDictionary(keys::kModificationKey, &value));
    modification->modification = ParseRequestCookie(value);
  }

  return scoped_ptr<WebRequestAction>(
      new WebRequestRequestCookieAction(modification));
}

scoped_ptr<WebRequestAction> CreateResponseCookieAction(
    const base::DictionaryValue* dict,
    std::string* error,
    bool* bad_message) {
  using extension_web_request_api_helpers::ResponseCookieModification;

  linked_ptr<ResponseCookieModification> modification(
      new ResponseCookieModification);

  // Get modification type.
  std::string instance_type;
  INPUT_FORMAT_VALIDATE(
      dict->GetString(keys::kInstanceTypeKey, &instance_type));
  if (instance_type == keys::kAddResponseCookieType)
    modification->type = helpers::ADD;
  else if (instance_type == keys::kEditResponseCookieType)
    modification->type = helpers::EDIT;
  else if (instance_type == keys::kRemoveResponseCookieType)
    modification->type = helpers::REMOVE;
  else
    INPUT_FORMAT_VALIDATE(false);

  // Get filter.
  if (modification->type == helpers::EDIT ||
      modification->type == helpers::REMOVE) {
    const DictionaryValue* filter = NULL;
    INPUT_FORMAT_VALIDATE(dict->GetDictionary(keys::kFilterKey, &filter));
    modification->filter = ParseFilterResponseCookie(filter);
  }

  // Get new value.
  if (modification->type == helpers::ADD) {
    const DictionaryValue* value = NULL;
    INPUT_FORMAT_VALIDATE(dict->GetDictionary(keys::kCookieKey, &value));
    modification->modification = ParseResponseCookie(value);
  } else if (modification->type == helpers::EDIT) {
    const DictionaryValue* value = NULL;
    INPUT_FORMAT_VALIDATE(dict->GetDictionary(keys::kModificationKey, &value));
    modification->modification = ParseResponseCookie(value);
  }

  return scoped_ptr<WebRequestAction>(
      new WebRequestResponseCookieAction(modification));
}

scoped_ptr<WebRequestAction> CreateSendMessageToExtensionAction(
    const base::DictionaryValue* dict,
    std::string* error,
    bool* bad_message) {
  std::string message;
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kMessageKey, &message));
  return scoped_ptr<WebRequestAction>(
      new WebRequestSendMessageToExtensionAction(message));
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
  // Maps the name of a declarativeWebRequest action type to the factory
  // function creating it.
  std::map<std::string, FactoryMethod> factory_methods;

  WebRequestActionFactory() {
    factory_methods[keys::kAddRequestCookieType] =
        &CreateRequestCookieAction;
    factory_methods[keys::kAddResponseCookieType] =
        &CreateResponseCookieAction;
    factory_methods[keys::kAddResponseHeaderType] =
        &CreateAddResponseHeaderAction;
    factory_methods[keys::kCancelRequestType] =
        &CallConstructorFactoryMethod<WebRequestCancelAction>;
    factory_methods[keys::kEditRequestCookieType] =
        &CreateRequestCookieAction;
    factory_methods[keys::kEditResponseCookieType] =
        &CreateResponseCookieAction;
    factory_methods[keys::kRedirectByRegExType] =
        &CreateRedirectRequestByRegExAction;
    factory_methods[keys::kRedirectRequestType] =
        &CreateRedirectRequestAction;
    factory_methods[keys::kRedirectToTransparentImageType] =
        &CallConstructorFactoryMethod<
            WebRequestRedirectToTransparentImageAction>;
    factory_methods[keys::kRedirectToEmptyDocumentType] =
        &CallConstructorFactoryMethod<WebRequestRedirectToEmptyDocumentAction>;
    factory_methods[keys::kRemoveRequestCookieType] =
        &CreateRequestCookieAction;
    factory_methods[keys::kRemoveResponseCookieType] =
        &CreateResponseCookieAction;
    factory_methods[keys::kSetRequestHeaderType] =
        &CreateSetRequestHeaderAction;
    factory_methods[keys::kRemoveRequestHeaderType] =
        &CreateRemoveRequestHeaderAction;
    factory_methods[keys::kRemoveResponseHeaderType] =
        &CreateRemoveResponseHeaderAction;
    factory_methods[keys::kIgnoreRulesType] =
        &CreateIgnoreRulesAction;
    factory_methods[keys::kSendMessageToExtensionType] =
        &CreateSendMessageToExtensionAction;
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

WebRequestAction::HostPermissionsStrategy
WebRequestAction::GetHostPermissionsStrategy() const {
  return STRATEGY_DEFAULT;
}

bool WebRequestAction::HasPermission(const ExtensionInfoMap* extension_info_map,
                                     const std::string& extension_id,
                                     const net::URLRequest* request,
                                     bool crosses_incognito) const {
  if (WebRequestPermissions::HideRequest(extension_info_map, request))
    return false;

  // In unit tests we don't have an extension_info_map object here and skip host
  // permission checks.
  if (!extension_info_map)
    return true;

  HostPermissionsStrategy strategy = GetHostPermissionsStrategy();
  if (strategy == STRATEGY_NONE || strategy == STRATEGY_DEFAULT) {
    bool check_host_permissions = strategy != STRATEGY_NONE;
    return WebRequestPermissions::CanExtensionAccessURL(
        extension_info_map, extension_id, request->url(), crosses_incognito,
        check_host_permissions);
  }
  return true;
}

bool WebRequestAction::DeltaHasPermission(
    const ExtensionInfoMap* extension_info_map,
    const std::string& extension_id,
    const net::URLRequest* request,
    bool crosses_incognito,
    const LinkedPtrEventResponseDelta& delta) const {
  if (GetHostPermissionsStrategy() == STRATEGY_ALLOW_SAME_DOMAIN) {
    return
        net::RegistryControlledDomainService::SameDomainOrHost(
            request->url(), delta->new_url) ||
        WebRequestPermissions::CanExtensionAccessURL(
            extension_info_map, extension_id, request->url(), crosses_incognito,
            true);
  }
  return true;
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

void WebRequestAction::Apply(const std::string& extension_id,
                             base::Time extension_install_time,
                             ApplyInfo* apply_info) const {
  if (!HasPermission(apply_info->extension_info_map, extension_id,
                     apply_info->request_data.request,
                     apply_info->crosses_incognito))
    return;
  if (GetStages() & apply_info->request_data.stage) {
    LinkedPtrEventResponseDelta delta = CreateDelta(
        apply_info->request_data, extension_id, extension_install_time);
    if (delta.get()) {
      if (DeltaHasPermission(apply_info->extension_info_map, extension_id,
                             apply_info->request_data.request,
                             apply_info->crosses_incognito,
                             delta))
        apply_info->deltas->push_back(delta);
    }
  }
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

WebRequestAction::HostPermissionsStrategy
WebRequestCancelAction::GetHostPermissionsStrategy() const {
  return WebRequestAction::STRATEGY_NONE;
}

LinkedPtrEventResponseDelta WebRequestCancelAction::CreateDelta(
    const DeclarativeWebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & GetStages());
  LinkedPtrEventResponseDelta result(
      new helpers::EventResponseDelta(extension_id, extension_install_time));
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

WebRequestAction::HostPermissionsStrategy
WebRequestRedirectAction::GetHostPermissionsStrategy() const {
  return WebRequestAction::STRATEGY_ALLOW_SAME_DOMAIN;
}

LinkedPtrEventResponseDelta WebRequestRedirectAction::CreateDelta(
    const DeclarativeWebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & GetStages());
  if (request_data.request->url() == redirect_url_)
    return LinkedPtrEventResponseDelta(NULL);
  LinkedPtrEventResponseDelta result(
      new helpers::EventResponseDelta(extension_id, extension_install_time));
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

WebRequestAction::HostPermissionsStrategy
WebRequestRedirectToTransparentImageAction::GetHostPermissionsStrategy() const {
  return WebRequestAction::STRATEGY_NONE;
}

LinkedPtrEventResponseDelta
WebRequestRedirectToTransparentImageAction::CreateDelta(
    const DeclarativeWebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & GetStages());
  LinkedPtrEventResponseDelta result(
      new helpers::EventResponseDelta(extension_id, extension_install_time));
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

WebRequestAction::HostPermissionsStrategy
WebRequestRedirectToEmptyDocumentAction::GetHostPermissionsStrategy() const {
  return WebRequestAction::STRATEGY_NONE;
}

LinkedPtrEventResponseDelta
WebRequestRedirectToEmptyDocumentAction::CreateDelta(
    const DeclarativeWebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & GetStages());
  LinkedPtrEventResponseDelta result(
      new helpers::EventResponseDelta(extension_id, extension_install_time));
  result->new_url = GURL(kEmptyDocumentUrl);
  return result;
}

//
// WebRequestRedirectByRegExAction
//

WebRequestRedirectByRegExAction::WebRequestRedirectByRegExAction(
    scoped_ptr<RE2> from_pattern,
    const std::string& to_pattern)
    : from_pattern_(from_pattern.Pass()),
      to_pattern_(to_pattern.data(), to_pattern.size()) {}

WebRequestRedirectByRegExAction::~WebRequestRedirectByRegExAction() {}

// About the syntax of the two languages:
//
// ICU (Perl) states:
// $n The text of capture group n will be substituted for $n. n must be >= 0
//    and not greater than the number of capture groups. A $ not followed by a
//    digit has no special meaning, and will appear in the substitution text
//    as itself, a $.
// \  Treat the following character as a literal, suppressing any special
//    meaning. Backslash escaping in substitution text is only required for
//    '$' and '\', but may be used on any other character without bad effects.
//
// RE2, derived from RE2::Rewrite()
// \  May only be followed by a digit or another \. If followed by a single
//    digit, both characters represent the respective capture group. If followed
//    by another \, it is used as an escape sequence.

// static
std::string WebRequestRedirectByRegExAction::PerlToRe2Style(
    const std::string& perl) {
  std::string::const_iterator i = perl.begin();
  std::string result;
  while (i != perl.end()) {
    if (*i == '$') {
      ++i;
      if (i == perl.end()) {
        result += '$';
        return result;
      } else if (isdigit(*i)) {
        result += '\\';
        result += *i;
      } else {
        result += '$';
        result += *i;
      }
    } else if (*i == '\\') {
      ++i;
      if (i == perl.end()) {
        result += '\\';
      } else if (*i == '$') {
        result += '$';
      } else if (*i == '\\') {
        result += "\\\\";
      } else {
        result += *i;
      }
    } else {
      result += *i;
    }
    ++i;
  }
  return result;
}

int WebRequestRedirectByRegExAction::GetStages() const {
  return ON_BEFORE_REQUEST;
}

WebRequestAction::Type WebRequestRedirectByRegExAction::GetType() const {
  return WebRequestAction::ACTION_REDIRECT_BY_REGEX_DOCUMENT;
}

WebRequestAction::HostPermissionsStrategy
WebRequestRedirectByRegExAction::GetHostPermissionsStrategy() const {
  return WebRequestAction::STRATEGY_ALLOW_SAME_DOMAIN;
}

LinkedPtrEventResponseDelta WebRequestRedirectByRegExAction::CreateDelta(
    const DeclarativeWebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & GetStages());
  CHECK(from_pattern_.get());

  const std::string& old_url = request_data.request->url().spec();
  std::string new_url = old_url;
  if (!RE2::Replace(&new_url, *from_pattern_, to_pattern_) ||
      new_url == old_url) {
    return LinkedPtrEventResponseDelta(NULL);
  }

  LinkedPtrEventResponseDelta result(
      new extension_web_request_api_helpers::EventResponseDelta(
          extension_id, extension_install_time));
  result->new_url = GURL(new_url);
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
    const DeclarativeWebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & GetStages());
  LinkedPtrEventResponseDelta result(
      new helpers::EventResponseDelta(extension_id, extension_install_time));
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
    const DeclarativeWebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & GetStages());
  LinkedPtrEventResponseDelta result(
      new helpers::EventResponseDelta(extension_id, extension_install_time));
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
    const DeclarativeWebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & GetStages());
  const net::HttpResponseHeaders* headers =
      request_data.original_response_headers;
  if (!headers)
    return LinkedPtrEventResponseDelta(NULL);

  // Don't generate the header if it exists already.
  if (headers->HasHeaderValue(name_, value_))
    return LinkedPtrEventResponseDelta(NULL);

  LinkedPtrEventResponseDelta result(
      new helpers::EventResponseDelta(extension_id, extension_install_time));
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
    const DeclarativeWebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & GetStages());
  const net::HttpResponseHeaders* headers =
      request_data.original_response_headers;
  if (!headers)
    return LinkedPtrEventResponseDelta(NULL);

  LinkedPtrEventResponseDelta result(
      new helpers::EventResponseDelta(extension_id, extension_install_time));
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

WebRequestAction::HostPermissionsStrategy
WebRequestIgnoreRulesAction::GetHostPermissionsStrategy() const {
  return WebRequestAction::STRATEGY_NONE;
}

LinkedPtrEventResponseDelta WebRequestIgnoreRulesAction::CreateDelta(
    const DeclarativeWebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & GetStages());
  return LinkedPtrEventResponseDelta(NULL);
}

//
// WebRequestRequestCookieAction
//

WebRequestRequestCookieAction::WebRequestRequestCookieAction(
    linked_ptr<RequestCookieModification> request_cookie_modification)
    : request_cookie_modification_(request_cookie_modification) {
  CHECK(request_cookie_modification_.get());
}

WebRequestRequestCookieAction::~WebRequestRequestCookieAction() {}

int WebRequestRequestCookieAction::GetStages() const {
  return ON_BEFORE_SEND_HEADERS;
}

WebRequestAction::Type WebRequestRequestCookieAction::GetType() const {
  return WebRequestAction::ACTION_MODIFY_REQUEST_COOKIE;
}

LinkedPtrEventResponseDelta WebRequestRequestCookieAction::CreateDelta(
    const DeclarativeWebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & GetStages());
  LinkedPtrEventResponseDelta result(
      new extension_web_request_api_helpers::EventResponseDelta(
          extension_id, extension_install_time));
  result->request_cookie_modifications.push_back(
      request_cookie_modification_);
  return result;
}

//
// WebRequestResponseCookieAction
//

WebRequestResponseCookieAction::WebRequestResponseCookieAction(
    linked_ptr<ResponseCookieModification> response_cookie_modification)
    : response_cookie_modification_(response_cookie_modification) {
  CHECK(response_cookie_modification_.get());
}

WebRequestResponseCookieAction::~WebRequestResponseCookieAction() {}

int WebRequestResponseCookieAction::GetStages() const {
  return ON_HEADERS_RECEIVED;
}

WebRequestAction::Type WebRequestResponseCookieAction::GetType() const {
  return WebRequestAction::ACTION_MODIFY_RESPONSE_COOKIE;
}

LinkedPtrEventResponseDelta WebRequestResponseCookieAction::CreateDelta(
    const DeclarativeWebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & GetStages());
  LinkedPtrEventResponseDelta result(
      new extension_web_request_api_helpers::EventResponseDelta(
          extension_id, extension_install_time));
  result->response_cookie_modifications.push_back(
      response_cookie_modification_);
  return result;
}

//
// WebRequestSendMessageToExtensionAction
//

WebRequestSendMessageToExtensionAction::WebRequestSendMessageToExtensionAction(
    const std::string& message)
    : message_(message) {
}

WebRequestSendMessageToExtensionAction::
~WebRequestSendMessageToExtensionAction() {}

int WebRequestSendMessageToExtensionAction::GetStages() const {
  return ON_BEFORE_REQUEST | ON_BEFORE_SEND_HEADERS | ON_HEADERS_RECEIVED |
      ON_AUTH_REQUIRED;
}

WebRequestAction::Type WebRequestSendMessageToExtensionAction::GetType() const {
  return WebRequestAction::ACTION_SEND_MESSAGE_TO_EXTENSION;
}

LinkedPtrEventResponseDelta WebRequestSendMessageToExtensionAction::CreateDelta(
    const DeclarativeWebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & GetStages());
  LinkedPtrEventResponseDelta result(
      new extension_web_request_api_helpers::EventResponseDelta(
          extension_id, extension_install_time));
  result->messages_to_extension.insert(message_);
  return result;
}

}  // namespace extensions
