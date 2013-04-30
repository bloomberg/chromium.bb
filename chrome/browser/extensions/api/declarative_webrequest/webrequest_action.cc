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
const char kIgnoreRulesRequiresParameterError[] =
    "IgnoreRules requires at least one parameter.";

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
  bool has_parameter = false;
  int minimum_priority = std::numeric_limits<int>::min();
  std::string ignore_tag;
  if (dict->HasKey(keys::kLowerPriorityThanKey)) {
    INPUT_FORMAT_VALIDATE(
        dict->GetInteger(keys::kLowerPriorityThanKey, &minimum_priority));
    has_parameter = true;
  }
  if (dict->HasKey(keys::kHasTagKey)) {
    INPUT_FORMAT_VALIDATE(dict->GetString(keys::kHasTagKey, &ignore_tag));
    has_parameter = true;
  }
  if (!has_parameter) {
    *error = kIgnoreRulesRequiresParameterError;
    return scoped_ptr<WebRequestAction>(NULL);
  }
  return scoped_ptr<WebRequestAction>(
      new WebRequestIgnoreRulesAction(minimum_priority, ignore_tag));
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

  typedef std::map<WebRequestAction::Type, const std::string> ActionNames;

  // Maps the name of a declarativeWebRequest action type to the factory
  // function creating it.
  std::map<std::string, FactoryMethod> factory_methods;

  // Translates action types into the corresponding JavaScript names.
  ActionNames action_names;

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

#define INSERT_ACTION_NAME(type, name) \
    action_names.insert(ActionNames::value_type(type, name));
    std::vector<std::string> names_buffer;
    names_buffer.push_back(keys::kAddRequestCookieType);
    names_buffer.push_back(keys::kEditRequestCookieType);
    names_buffer.push_back(keys::kRemoveRequestCookieType);
    INSERT_ACTION_NAME(WebRequestAction::ACTION_MODIFY_REQUEST_COOKIE,
                       JoinString(names_buffer, ", "));
    names_buffer.clear();
    names_buffer.push_back(keys::kAddResponseCookieType);
    names_buffer.push_back(keys::kEditResponseCookieType);
    names_buffer.push_back(keys::kRemoveResponseCookieType);
    INSERT_ACTION_NAME(WebRequestAction::ACTION_MODIFY_RESPONSE_COOKIE,
                       JoinString(names_buffer, ", "));
    INSERT_ACTION_NAME(WebRequestAction::ACTION_ADD_RESPONSE_HEADER,
                       keys::kAddResponseHeaderType);
    INSERT_ACTION_NAME(WebRequestAction::ACTION_CANCEL_REQUEST,
                       keys::kCancelRequestType);
    INSERT_ACTION_NAME(WebRequestAction::ACTION_REDIRECT_BY_REGEX_DOCUMENT,
                       keys::kRedirectByRegExType);
    INSERT_ACTION_NAME(WebRequestAction::ACTION_REDIRECT_REQUEST,
                       keys::kRedirectRequestType);
    INSERT_ACTION_NAME(WebRequestAction::ACTION_REDIRECT_TO_TRANSPARENT_IMAGE,
                       keys::kRedirectToTransparentImageType);
    INSERT_ACTION_NAME(WebRequestAction::ACTION_REDIRECT_TO_EMPTY_DOCUMENT,
                       keys::kRedirectToEmptyDocumentType);
    INSERT_ACTION_NAME(WebRequestAction::ACTION_SET_REQUEST_HEADER,
                       keys::kSetRequestHeaderType);
    INSERT_ACTION_NAME(WebRequestAction::ACTION_REMOVE_REQUEST_HEADER,
                       keys::kRemoveRequestHeaderType);
    INSERT_ACTION_NAME(WebRequestAction::ACTION_REMOVE_RESPONSE_HEADER,
                       keys::kRemoveResponseHeaderType);
    INSERT_ACTION_NAME(WebRequestAction::ACTION_IGNORE_RULES,
                       keys::kIgnoreRulesType);
    INSERT_ACTION_NAME(WebRequestAction::ACTION_SEND_MESSAGE_TO_EXTENSION,
                       keys::kSendMessageToExtensionType);
#undef INSERT_ACTION_NAME
  }
};

base::LazyInstance<WebRequestActionFactory>::Leaky
    g_web_request_action_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

//
// WebRequestAction
//

WebRequestAction::~WebRequestAction() {}

const std::string& WebRequestAction::GetName() const {
  const WebRequestActionFactory::ActionNames& names =
      g_web_request_action_factory.Get().action_names;
  std::map<WebRequestAction::Type, const std::string>::const_iterator it =
      names.find(type());
  CHECK(it != names.end());
  return it->second;
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

  WebRequestPermissions::HostPermissionsCheck permission_check =
      WebRequestPermissions::REQUIRE_ALL_URLS;
  switch (host_permissions_strategy()) {
    case STRATEGY_DEFAULT:  // Default value is already set.
      break;
    case STRATEGY_NONE:
      permission_check = WebRequestPermissions::DO_NOT_CHECK_HOST;
      break;
    case STRATEGY_HOST:
      permission_check = WebRequestPermissions::REQUIRE_HOST_PERMISSION;
      break;
  }
  return WebRequestPermissions::CanExtensionAccessURL(
      extension_info_map, extension_id, request->url(), crosses_incognito,
      permission_check);
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
  if (stages() & apply_info->request_data.stage) {
    LinkedPtrEventResponseDelta delta = CreateDelta(
        apply_info->request_data, extension_id, extension_install_time);
    if (delta.get())
      apply_info->deltas->push_back(delta);
    if (type() == WebRequestAction::ACTION_IGNORE_RULES) {
      const WebRequestIgnoreRulesAction* ignore_action =
          static_cast<const WebRequestIgnoreRulesAction*>(this);
      if (!ignore_action->ignore_tag().empty())
        apply_info->ignored_tags->insert(ignore_action->ignore_tag());
    }
  }
}

WebRequestAction::WebRequestAction(int stages,
                                   Type type,
                                   int minimum_priority,
                                   HostPermissionsStrategy strategy)
    : stages_(stages),
      type_(type),
      minimum_priority_(minimum_priority),
      host_permissions_strategy_(strategy) {}

//
// WebRequestCancelAction
//

WebRequestCancelAction::WebRequestCancelAction()
    : WebRequestAction(ON_BEFORE_REQUEST | ON_BEFORE_SEND_HEADERS |
                           ON_HEADERS_RECEIVED | ON_AUTH_REQUIRED,
                       ACTION_CANCEL_REQUEST,
                       std::numeric_limits<int>::min(),
                       STRATEGY_NONE) {}

WebRequestCancelAction::~WebRequestCancelAction() {}

LinkedPtrEventResponseDelta WebRequestCancelAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
  LinkedPtrEventResponseDelta result(
      new helpers::EventResponseDelta(extension_id, extension_install_time));
  result->cancel = true;
  return result;
}

//
// WebRequestRedirectAction
//

WebRequestRedirectAction::WebRequestRedirectAction(const GURL& redirect_url)
    : WebRequestAction(ON_BEFORE_REQUEST,
                       ACTION_REDIRECT_REQUEST,
                       std::numeric_limits<int>::min(),
                       STRATEGY_DEFAULT),
      redirect_url_(redirect_url) {}

WebRequestRedirectAction::~WebRequestRedirectAction() {}

LinkedPtrEventResponseDelta WebRequestRedirectAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
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
    WebRequestRedirectToTransparentImageAction()
    : WebRequestAction(ON_BEFORE_REQUEST,
                       ACTION_REDIRECT_TO_TRANSPARENT_IMAGE,
                       std::numeric_limits<int>::min(),
                       STRATEGY_NONE) {}

WebRequestRedirectToTransparentImageAction::
~WebRequestRedirectToTransparentImageAction() {}

LinkedPtrEventResponseDelta
WebRequestRedirectToTransparentImageAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
  LinkedPtrEventResponseDelta result(
      new helpers::EventResponseDelta(extension_id, extension_install_time));
  result->new_url = GURL(kTransparentImageUrl);
  return result;
}

//
// WebRequestRedirectToEmptyDocumentAction
//

WebRequestRedirectToEmptyDocumentAction::
    WebRequestRedirectToEmptyDocumentAction()
    : WebRequestAction(ON_BEFORE_REQUEST,
                       ACTION_REDIRECT_TO_EMPTY_DOCUMENT,
                       std::numeric_limits<int>::min(),
                       STRATEGY_NONE) {}

WebRequestRedirectToEmptyDocumentAction::
~WebRequestRedirectToEmptyDocumentAction() {}

LinkedPtrEventResponseDelta
WebRequestRedirectToEmptyDocumentAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
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
    : WebRequestAction(ON_BEFORE_REQUEST,
                       ACTION_REDIRECT_BY_REGEX_DOCUMENT,
                       std::numeric_limits<int>::min(),
                       STRATEGY_DEFAULT),
      from_pattern_(from_pattern.Pass()),
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

LinkedPtrEventResponseDelta WebRequestRedirectByRegExAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
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
    : WebRequestAction(ON_BEFORE_SEND_HEADERS,
                       ACTION_SET_REQUEST_HEADER,
                       std::numeric_limits<int>::min(),
                       STRATEGY_DEFAULT),
      name_(name),
      value_(value) {}

WebRequestSetRequestHeaderAction::~WebRequestSetRequestHeaderAction() {}

LinkedPtrEventResponseDelta
WebRequestSetRequestHeaderAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
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
    : WebRequestAction(ON_BEFORE_SEND_HEADERS,
                       ACTION_REMOVE_REQUEST_HEADER,
                       std::numeric_limits<int>::min(),
                       STRATEGY_DEFAULT),
      name_(name) {}

WebRequestRemoveRequestHeaderAction::~WebRequestRemoveRequestHeaderAction() {}

LinkedPtrEventResponseDelta
WebRequestRemoveRequestHeaderAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
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
    : WebRequestAction(ON_HEADERS_RECEIVED,
                       ACTION_ADD_RESPONSE_HEADER,
                       std::numeric_limits<int>::min(),
                       STRATEGY_DEFAULT),
      name_(name),
      value_(value) {}

WebRequestAddResponseHeaderAction::~WebRequestAddResponseHeaderAction() {}

LinkedPtrEventResponseDelta
WebRequestAddResponseHeaderAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
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
    : WebRequestAction(ON_HEADERS_RECEIVED,
                       ACTION_REMOVE_RESPONSE_HEADER,
                       std::numeric_limits<int>::min(),
                       STRATEGY_DEFAULT),
      name_(name),
      value_(value),
      has_value_(has_value) {}

WebRequestRemoveResponseHeaderAction::~WebRequestRemoveResponseHeaderAction() {}

LinkedPtrEventResponseDelta
WebRequestRemoveResponseHeaderAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
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
    int minimum_priority,
    const std::string& ignore_tag)
    : WebRequestAction(ON_BEFORE_REQUEST | ON_BEFORE_SEND_HEADERS |
                           ON_HEADERS_RECEIVED | ON_AUTH_REQUIRED,
                       ACTION_IGNORE_RULES,
                       minimum_priority,
                       STRATEGY_NONE),
      ignore_tag_(ignore_tag) {}

WebRequestIgnoreRulesAction::~WebRequestIgnoreRulesAction() {}

LinkedPtrEventResponseDelta WebRequestIgnoreRulesAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
  return LinkedPtrEventResponseDelta(NULL);
}

//
// WebRequestRequestCookieAction
//

WebRequestRequestCookieAction::WebRequestRequestCookieAction(
    linked_ptr<RequestCookieModification> request_cookie_modification)
    : WebRequestAction(ON_BEFORE_SEND_HEADERS,
                       ACTION_MODIFY_REQUEST_COOKIE,
                       std::numeric_limits<int>::min(),
                       STRATEGY_DEFAULT),
      request_cookie_modification_(request_cookie_modification) {
  CHECK(request_cookie_modification_.get());
}

WebRequestRequestCookieAction::~WebRequestRequestCookieAction() {}

LinkedPtrEventResponseDelta WebRequestRequestCookieAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
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
    : WebRequestAction(ON_HEADERS_RECEIVED,
                       ACTION_MODIFY_RESPONSE_COOKIE,
                       std::numeric_limits<int>::min(),
                       STRATEGY_DEFAULT),
      response_cookie_modification_(response_cookie_modification) {
  CHECK(response_cookie_modification_.get());
}

WebRequestResponseCookieAction::~WebRequestResponseCookieAction() {}

LinkedPtrEventResponseDelta WebRequestResponseCookieAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
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
    : WebRequestAction(ON_BEFORE_REQUEST | ON_BEFORE_SEND_HEADERS |
                           ON_HEADERS_RECEIVED | ON_AUTH_REQUIRED,
                       ACTION_SEND_MESSAGE_TO_EXTENSION,
                       std::numeric_limits<int>::min(),
                       STRATEGY_HOST),
      message_(message) {}

WebRequestSendMessageToExtensionAction::
~WebRequestSendMessageToExtensionAction() {}

LinkedPtrEventResponseDelta WebRequestSendMessageToExtensionAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
  LinkedPtrEventResponseDelta result(
      new extension_web_request_api_helpers::EventResponseDelta(
          extension_id, extension_install_time));
  result->messages_to_extension.insert(message_);
  return result;
}

}  // namespace extensions
