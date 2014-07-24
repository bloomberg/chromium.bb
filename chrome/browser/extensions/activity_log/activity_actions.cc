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
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/ad_network_database.h"
#include "chrome/browser/extensions/activity_log/fullstream_ui_policy.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "components/rappor/rappor_service.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "extensions/common/dom_action_types.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace constants = activity_log_constants;

namespace extensions {

namespace {

// The "Extensions.PossibleAdInjection2" metric uses different Rappor
// parameters than the original metric.
const char* kExtensionAdInjectionRapporMetricName =
    "Extensions.PossibleAdInjection2";

const char kBlinkSetAttributeEvent[] = "blinkSetAttribute";
const char kBlinkAddElementEvent[] = "blinkAddElement";

const char kIframe[] = "iframe";
const char kAnchor[] = "a";
const char kScript[] = "script";

const char kSrc[] = "src";
const char kHref[] = "href";

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

  // We should always have an AdNetworkDatabase, but, on the offchance we don't,
  // don't crash in a release build.
  if (!AdNetworkDatabase::Get()) {
    NOTREACHED();
    return NO_AD_INJECTION;
  }

  AdType ad_type = AD_TYPE_NONE;
  InjectionType injection_type = NO_AD_INJECTION;

  if (api_name_ == kBlinkSetAttributeEvent) {
    std::string element_name;
    std::string attr_name;
    if (args_.get()) {
      args_->GetString(0u, &element_name);
      args_->GetString(1u, &attr_name);
    }
    if (attr_name == kSrc) {
      if (element_name == kIframe)
        ad_type = AD_TYPE_IFRAME;
      else if (element_name == kScript)
        ad_type = AD_TYPE_SCRIPT;
    } else if (element_name == kAnchor && attr_name == kHref) {
      ad_type = AD_TYPE_ANCHOR;
    }

    if (ad_type != AD_TYPE_NONE)
      injection_type = CheckAttrModification();
  } else if (api_name_ == kBlinkAddElementEvent) {
    std::string element_name;
    if (args_.get())
      args_->GetString(0u, &element_name);
    if (element_name == kIframe)
      ad_type = AD_TYPE_IFRAME;
    else if (element_name == kAnchor)
      ad_type = AD_TYPE_ANCHOR;
    else if (element_name == kScript)
      ad_type = AD_TYPE_SCRIPT;

    if (ad_type != AD_TYPE_NONE)
      injection_type = CheckElementAddition();
  }

  if (injection_type != NO_AD_INJECTION) {
    UMA_HISTOGRAM_ENUMERATION(
        "Extensions.AdInjection.AdType", ad_type, Action::NUM_AD_TYPES);
  }

  return injection_type;
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

bool Action::UrlCouldBeAd(const GURL& url) const {
  // Ads can only be valid urls that don't match the page's host (linking to the
  // current page should be considered valid use), and aren't local to the
  // extension.
  return url.is_valid() &&
         !url.is_empty() &&
         url.host() != page_url_.host() &&
         !url.SchemeIs(kExtensionScheme);
}

void Action::MaybeUploadUrl(rappor::RapporService* rappor_service) const {
  // Don't bother recording if the url is innocuous (or no |rappor_service|).
  if (!rappor_service)
    return;

  GURL url;

  if (api_name_ == kBlinkSetAttributeEvent) {
    std::string element_name;
    std::string attr_name;
    std::string url_string;
    if (args_.get()) {
      args_->GetString(0u, &element_name);
      args_->GetString(1u, &attr_name);
    }
    if (element_name == kIframe && attr_name == kSrc) {
      args_->GetString(3u, &url_string);
      url = GURL(url_string);
    } else if (element_name == kAnchor && attr_name == kHref) {
      args_->GetString(3u, &url_string);
      url = GURL(url_string);
    }
  } else if (api_name_ == kBlinkAddElementEvent) {
    std::string element_name;
    std::string url_string;
    if (args_.get())
      args_->GetString(0u, &element_name);
    if (element_name == kIframe) {
      args_->GetString(1u, &url_string);
      url = GURL(url_string);
    } else if (element_name == kAnchor) {
      args_->GetString(1u, &url_string);
      url = GURL(url_string);
    }
  }

  if (!UrlCouldBeAd(url))
    return;

  // Record the URL - an ad *may* have been injected.
  rappor_service->RecordSample(kExtensionAdInjectionRapporMetricName,
                               rappor::ETLD_PLUS_ONE_RAPPOR_TYPE,
                               url.host());
}

Action::InjectionType Action::CheckAttrModification() const {
  if (api_name_ != kBlinkSetAttributeEvent)
    return NO_AD_INJECTION;

  const AdNetworkDatabase* database = AdNetworkDatabase::Get();

  GURL prev_url;
  std::string prev_url_string;
  if (args_.get() && args_->GetString(2u, &prev_url_string))
    prev_url = GURL(prev_url_string);

  GURL new_url;
  std::string new_url_string;
  if (args_.get() && args_->GetString(3u, &new_url_string))
    new_url = GURL(new_url_string);

  bool new_url_could_be_ad = UrlCouldBeAd(new_url);
  bool prev_url_valid = prev_url.is_valid() && !prev_url.is_empty();

  bool injected_ad = new_url_could_be_ad && database->IsAdNetwork(new_url);
  bool replaced_ad = prev_url_valid && database->IsAdNetwork(prev_url);

  if (injected_ad && replaced_ad)
    return INJECTION_REPLACED_AD;
  if (injected_ad)
    return INJECTION_NEW_AD;
  if (replaced_ad)
    return INJECTION_REMOVED_AD;

  // If the extension modified the URL with an external, valid URL then there's
  // a good chance it's ad injection. Log it as a likely one, which also helps
  // us determine the effectiveness of our IsAdNetwork() recognition.
  if (new_url_could_be_ad) {
    if (prev_url_valid)
      return INJECTION_LIKELY_REPLACED_AD;
    return INJECTION_LIKELY_NEW_AD;
  }

  return NO_AD_INJECTION;
}

Action::InjectionType Action::CheckElementAddition() const {
  DCHECK_EQ(kBlinkAddElementEvent, api_name_);

  GURL url;
  std::string url_string;
  if (args_.get() && args_->GetString(1u, &url_string))
    url = GURL(url_string);

  if (UrlCouldBeAd(url)) {
    if (AdNetworkDatabase::Get()->IsAdNetwork(url))
      return INJECTION_NEW_AD;
    // If the extension injected an URL which is not local to itself or the
    // page, there is a good chance it could be a new ad, and our database
    // missed it.
    return INJECTION_LIKELY_NEW_AD;
  }
  return NO_AD_INJECTION;
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
