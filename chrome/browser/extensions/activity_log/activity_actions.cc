// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/activity_actions.h"

#include <algorithm>  // for std::find.
#include <string>

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/ad_network_database.h"
#include "chrome/browser/extensions/activity_log/fullstream_ui_policy.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/ad_injection_constants.h"
#include "chrome/common/extensions/dom_action_types.h"
#include "components/rappor/rappor_service.h"
#include "content/public/browser/web_contents.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace constants = activity_log_constants;

namespace extensions {

namespace {

namespace keys = ad_injection_constants::keys;

// The list of APIs for which we upload the URL to RAPPOR.
const char* kApisForRapporMetric[] = {
  "HTMLIFrameElement.src",
  "HTMLEmbedElement.src",
  "HTMLAnchorElement.href",
};

const char* kExtensionAdInjectionRapporMetricName =
    "Extensions.PossibleAdInjection";

// The elements for which we check the 'src' attribute to look for ads.
const char* kSrcElements[] = {
  "HTMLIFrameElement",
  "HTMLEmbedElement"
};

// The elements for which we check the 'href' attribute to look for ads.
const char* kHrefElements[] = {
  "HTMLAnchorElement",
};

bool IsSrcElement(const std::string& str) {
  static const char** end = kSrcElements + arraysize(kSrcElements);
  return std::find(kSrcElements, end, str) != end;
}

bool IsHrefElement(const std::string& str) {
  static const char** end = kHrefElements + arraysize(kHrefElements);
  return std::find(kHrefElements, end, str) != end;
}

std::string Serialize(const base::Value* value) {
  std::string value_as_text;
  if (!value) {
    value_as_text = "null";
  } else {
    JSONStringValueSerializer serializer(&value_as_text);
    serializer.SerializeAndOmitBinaryValues(*value);
  }
  return value_as_text;
}

Action::InjectionType CheckDomObject(const base::DictionaryValue* object) {
  std::string type;
  object->GetString(keys::kType, &type);

  std::string url_key;
  if (IsSrcElement(type))
    url_key = keys::kSrc;
  else if (IsHrefElement(type))
    url_key = keys::kHref;

  if (!url_key.empty()) {
    std::string url;
    if (object->GetString(url_key, &url) &&
        AdNetworkDatabase::Get()->IsAdNetwork(GURL(url))) {
      return Action::INJECTION_NEW_AD;
    }
  }

  const base::ListValue* children = NULL;
  if (object->GetList(keys::kChildren, &children)) {
    const base::DictionaryValue* child = NULL;
    for (size_t i = 0;
         i < children->GetSize() &&
             i < ad_injection_constants::kMaximumChildrenToCheck;
         ++i) {
      if (children->GetDictionary(i, &child) && CheckDomObject(child))
        return Action::INJECTION_NEW_AD;
    }
  }

  return Action::NO_AD_INJECTION;
}

}  // namespace

using api::activity_log_private::ExtensionActivity;

Action::Action(const std::string& extension_id,
               const base::Time& time,
               const ActionType action_type,
               const std::string& api_name,
               int64 action_id)
    : extension_id_(extension_id),
      time_(time),
      action_type_(action_type),
      api_name_(api_name),
      page_incognito_(false),
      arg_incognito_(false),
      count_(0),
      action_id_(action_id) {}

Action::~Action() {}

// TODO(mvrable): As an optimization, we might return this directly if the
// refcount is one.  However, there are likely to be other stray references in
// many cases that will prevent this optimization.
scoped_refptr<Action> Action::Clone() const {
  scoped_refptr<Action> clone(
      new Action(
          extension_id(), time(), action_type(), api_name(), action_id()));
  if (args())
    clone->set_args(make_scoped_ptr(args()->DeepCopy()));
  clone->set_page_url(page_url());
  clone->set_page_title(page_title());
  clone->set_page_incognito(page_incognito());
  clone->set_arg_url(arg_url());
  clone->set_arg_incognito(arg_incognito());
  if (other())
    clone->set_other(make_scoped_ptr(other()->DeepCopy()));
  return clone;
}

Action::InjectionType Action::DidInjectAd(
    rappor::RapporService* rappor_service) const {
  MaybeUploadUrl(rappor_service);

  // Currently, we do not have the list of ad networks, so we exit immediately
  // with NO_AD_INJECTION (unless the database has been set by a test).
  if (!AdNetworkDatabase::Get())
    return NO_AD_INJECTION;

  if (api_name_ == ad_injection_constants::kHtmlIframeSrcApiName ||
      api_name_ == ad_injection_constants::kHtmlEmbedSrcApiName) {
    return CheckSrcModification();
  } else if (EndsWith(api_name_,
                      ad_injection_constants::kAppendChildApiSuffix,
                      true /* case senstive */)) {
    return CheckAppendChild();
  }

  return NO_AD_INJECTION;
}

void Action::set_args(scoped_ptr<base::ListValue> args) {
  args_.reset(args.release());
}

base::ListValue* Action::mutable_args() {
  if (!args_.get()) {
    args_.reset(new base::ListValue());
  }
  return args_.get();
}

void Action::set_page_url(const GURL& page_url) {
  page_url_ = page_url;
}

void Action::set_arg_url(const GURL& arg_url) {
  arg_url_ = arg_url;
}

void Action::set_other(scoped_ptr<base::DictionaryValue> other) {
  other_.reset(other.release());
}

base::DictionaryValue* Action::mutable_other() {
  if (!other_.get()) {
    other_.reset(new base::DictionaryValue());
  }
  return other_.get();
}

std::string Action::SerializePageUrl() const {
  return (page_incognito() ? constants::kIncognitoUrl : "") + page_url().spec();
}

void Action::ParsePageUrl(const std::string& url) {
  set_page_incognito(StartsWithASCII(url, constants::kIncognitoUrl, true));
  if (page_incognito())
    set_page_url(GURL(url.substr(strlen(constants::kIncognitoUrl))));
  else
    set_page_url(GURL(url));
}

std::string Action::SerializeArgUrl() const {
  return (arg_incognito() ? constants::kIncognitoUrl : "") + arg_url().spec();
}

void Action::ParseArgUrl(const std::string& url) {
  set_arg_incognito(StartsWithASCII(url, constants::kIncognitoUrl, true));
  if (arg_incognito())
    set_arg_url(GURL(url.substr(strlen(constants::kIncognitoUrl))));
  else
    set_arg_url(GURL(url));
}

scoped_ptr<ExtensionActivity> Action::ConvertToExtensionActivity() {
  scoped_ptr<ExtensionActivity> result(new ExtensionActivity);

  // We do this translation instead of using the same enum because the database
  // values need to be stable; this allows us to change the extension API
  // without affecting the database.
  switch (action_type()) {
    case ACTION_API_CALL:
      result->activity_type = ExtensionActivity::ACTIVITY_TYPE_API_CALL;
      break;
    case ACTION_API_EVENT:
      result->activity_type = ExtensionActivity::ACTIVITY_TYPE_API_EVENT;
      break;
    case ACTION_CONTENT_SCRIPT:
      result->activity_type = ExtensionActivity::ACTIVITY_TYPE_CONTENT_SCRIPT;
      break;
    case ACTION_DOM_ACCESS:
      result->activity_type = ExtensionActivity::ACTIVITY_TYPE_DOM_ACCESS;
      break;
    case ACTION_DOM_EVENT:
      result->activity_type = ExtensionActivity::ACTIVITY_TYPE_DOM_EVENT;
      break;
    case ACTION_WEB_REQUEST:
      result->activity_type = ExtensionActivity::ACTIVITY_TYPE_WEB_REQUEST;
      break;
    case UNUSED_ACTION_API_BLOCKED:
    case ACTION_ANY:
    default:
      // This shouldn't be reached, but some people might have old or otherwise
      // weird db entries. Treat it like an API call if that happens.
      result->activity_type = ExtensionActivity::ACTIVITY_TYPE_API_CALL;
      break;
  }

  result->extension_id.reset(new std::string(extension_id()));
  result->time.reset(new double(time().ToJsTime()));
  result->count.reset(new double(count()));
  result->api_call.reset(new std::string(api_name()));
  result->args.reset(new std::string(Serialize(args())));
  if (action_id() != -1)
    result->activity_id.reset(
        new std::string(base::StringPrintf("%" PRId64, action_id())));
  if (page_url().is_valid()) {
    if (!page_title().empty())
      result->page_title.reset(new std::string(page_title()));
    result->page_url.reset(new std::string(SerializePageUrl()));
  }
  if (arg_url().is_valid())
    result->arg_url.reset(new std::string(SerializeArgUrl()));

  if (other()) {
    scoped_ptr<ExtensionActivity::Other> other_field(
        new ExtensionActivity::Other);
    bool prerender;
    if (other()->GetBooleanWithoutPathExpansion(constants::kActionPrerender,
                                                &prerender)) {
      other_field->prerender.reset(new bool(prerender));
    }
    const base::DictionaryValue* web_request;
    if (other()->GetDictionaryWithoutPathExpansion(constants::kActionWebRequest,
                                                   &web_request)) {
      other_field->web_request.reset(new std::string(
          ActivityLogPolicy::Util::Serialize(web_request)));
    }
    std::string extra;
    if (other()->GetStringWithoutPathExpansion(constants::kActionExtra, &extra))
      other_field->extra.reset(new std::string(extra));
    int dom_verb;
    if (other()->GetIntegerWithoutPathExpansion(constants::kActionDomVerb,
                                                &dom_verb)) {
      switch (static_cast<DomActionType::Type>(dom_verb)) {
        case DomActionType::GETTER:
          other_field->dom_verb = ExtensionActivity::Other::DOM_VERB_GETTER;
          break;
        case DomActionType::SETTER:
          other_field->dom_verb = ExtensionActivity::Other::DOM_VERB_SETTER;
          break;
        case DomActionType::METHOD:
          other_field->dom_verb = ExtensionActivity::Other::DOM_VERB_METHOD;
          break;
        case DomActionType::INSERTED:
          other_field->dom_verb = ExtensionActivity::Other::DOM_VERB_INSERTED;
          break;
        case DomActionType::XHR:
          other_field->dom_verb = ExtensionActivity::Other::DOM_VERB_XHR;
          break;
        case DomActionType::WEBREQUEST:
          other_field->dom_verb = ExtensionActivity::Other::DOM_VERB_WEBREQUEST;
          break;
        case DomActionType::MODIFIED:
          other_field->dom_verb = ExtensionActivity::Other::DOM_VERB_MODIFIED;
          break;
        default:
          other_field->dom_verb = ExtensionActivity::Other::DOM_VERB_NONE;
      }
    } else {
      other_field->dom_verb = ExtensionActivity::Other::DOM_VERB_NONE;
    }
    result->other.reset(other_field.release());
  }

  return result.Pass();
}

std::string Action::PrintForDebug() const {
  std::string result = base::StringPrintf("ACTION ID=%" PRId64, action_id());
  result += " EXTENSION ID=" + extension_id() + " CATEGORY=";
  switch (action_type_) {
    case ACTION_API_CALL:
      result += "api_call";
      break;
    case ACTION_API_EVENT:
      result += "api_event_callback";
      break;
    case ACTION_WEB_REQUEST:
      result += "webrequest";
      break;
    case ACTION_CONTENT_SCRIPT:
      result += "content_script";
      break;
    case UNUSED_ACTION_API_BLOCKED:
      // This is deprecated.
      result += "api_blocked";
      break;
    case ACTION_DOM_EVENT:
      result += "dom_event";
      break;
    case ACTION_DOM_ACCESS:
      result += "dom_access";
      break;
    default:
      result += base::StringPrintf("type%d", static_cast<int>(action_type_));
  }

  result += " API=" + api_name_;
  if (args_.get()) {
    result += " ARGS=" + Serialize(args_.get());
  }
  if (page_url_.is_valid()) {
    if (page_incognito_)
      result += " PAGE_URL=(incognito)" + page_url_.spec();
    else
      result += " PAGE_URL=" + page_url_.spec();
  }
  if (!page_title_.empty()) {
    base::StringValue title(page_title_);
    result += " PAGE_TITLE=" + Serialize(&title);
  }
  if (arg_url_.is_valid()) {
    if (arg_incognito_)
      result += " ARG_URL=(incognito)" + arg_url_.spec();
    else
      result += " ARG_URL=" + arg_url_.spec();
  }
  if (other_.get()) {
    result += " OTHER=" + Serialize(other_.get());
  }

  result += base::StringPrintf(" COUNT=%d", count_);
  return result;
}

void Action::MaybeUploadUrl(rappor::RapporService* rappor_service) const {
  // If there's no given |rappor_service|, abort immediately.
  if (!rappor_service)
    return;

  // If the action has no url, or the url is empty, then return.
  if (!arg_url_.is_valid() || arg_url_.is_empty())
    return;
  std::string host = arg_url_.host();
  if (host.empty())
    return;

  bool can_inject_ads = false;
  for (size_t i = 0; i < arraysize(kApisForRapporMetric); ++i) {
    if (api_name_ == kApisForRapporMetric[i]) {
      can_inject_ads = true;
      break;
    }
  }

  if (!can_inject_ads)
    return;

  // Record the URL - an ad *may* have been injected.
  rappor_service->RecordSample(kExtensionAdInjectionRapporMetricName,
                               rappor::ETLD_PLUS_ONE_RAPPOR_TYPE,
                               host);
}

Action::InjectionType Action::CheckSrcModification() const {
  bool injected_ad = arg_url_.is_valid() &&
                     !arg_url_.is_empty() &&
                     AdNetworkDatabase::Get()->IsAdNetwork(arg_url_);
  return injected_ad ? INJECTION_NEW_AD : NO_AD_INJECTION;
}

Action::InjectionType Action::CheckAppendChild() const {
  const base::DictionaryValue* child = NULL;
  if (!args_->GetDictionary(0u, &child))
    return NO_AD_INJECTION;

  return CheckDomObject(child);
}

bool ActionComparator::operator()(
    const scoped_refptr<Action>& lhs,
    const scoped_refptr<Action>& rhs) const {
  if (lhs->time() != rhs->time())
    return lhs->time() < rhs->time();
  else if (lhs->action_id() != rhs->action_id())
    return lhs->action_id() < rhs->action_id();
  else
    return ActionComparatorExcludingTimeAndActionId()(lhs, rhs);
}

bool ActionComparatorExcludingTimeAndActionId::operator()(
    const scoped_refptr<Action>& lhs,
    const scoped_refptr<Action>& rhs) const {
  if (lhs->extension_id() != rhs->extension_id())
    return lhs->extension_id() < rhs->extension_id();
  if (lhs->action_type() != rhs->action_type())
    return lhs->action_type() < rhs->action_type();
  if (lhs->api_name() != rhs->api_name())
    return lhs->api_name() < rhs->api_name();

  // args might be null; treat a null value as less than all non-null values,
  // including the empty string.
  if (!lhs->args() && rhs->args())
    return true;
  if (lhs->args() && !rhs->args())
    return false;
  if (lhs->args() && rhs->args()) {
    std::string lhs_args = ActivityLogPolicy::Util::Serialize(lhs->args());
    std::string rhs_args = ActivityLogPolicy::Util::Serialize(rhs->args());
    if (lhs_args != rhs_args)
      return lhs_args < rhs_args;
  }

  // Compare URLs as strings, and treat the incognito flag as a separate field.
  if (lhs->page_url().spec() != rhs->page_url().spec())
    return lhs->page_url().spec() < rhs->page_url().spec();
  if (lhs->page_incognito() != rhs->page_incognito())
    return lhs->page_incognito() < rhs->page_incognito();

  if (lhs->page_title() != rhs->page_title())
    return lhs->page_title() < rhs->page_title();

  if (lhs->arg_url().spec() != rhs->arg_url().spec())
    return lhs->arg_url().spec() < rhs->arg_url().spec();
  if (lhs->arg_incognito() != rhs->arg_incognito())
    return lhs->arg_incognito() < rhs->arg_incognito();

  // other is treated much like the args field.
  if (!lhs->other() && rhs->other())
    return true;
  if (lhs->other() && !rhs->other())
    return false;
  if (lhs->other() && rhs->other()) {
    std::string lhs_other = ActivityLogPolicy::Util::Serialize(lhs->other());
    std::string rhs_other = ActivityLogPolicy::Util::Serialize(rhs->other());
    if (lhs_other != rhs_other)
      return lhs_other < rhs_other;
  }

  // All fields compare as equal if this point is reached.
  return false;
}

}  // namespace extensions
