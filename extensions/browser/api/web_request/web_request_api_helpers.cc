// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_api_helpers.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/extension_messages.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/http/http_util.h"
#include "net/log/net_log_event_type.h"
#include "url/url_constants.h"

// TODO(battre): move all static functions into an anonymous namespace at the
// top of this file.

using base::Time;
using net::cookie_util::ParsedRequestCookie;
using net::cookie_util::ParsedRequestCookies;

namespace keys = extension_web_request_api_constants;

namespace extension_web_request_api_helpers {

namespace {

using ParsedResponseCookies = std::vector<linked_ptr<net::ParsedCookie>>;

void ClearCacheOnNavigationOnUI() {
  web_cache::WebCacheManager::GetInstance()->ClearCacheOnNavigation();
}

bool ParseCookieLifetime(net::ParsedCookie* cookie,
                         int64_t* seconds_till_expiry) {
  // 'Max-Age' is processed first because according to:
  // http://tools.ietf.org/html/rfc6265#section-5.3 'Max-Age' attribute
  // overrides 'Expires' attribute.
  if (cookie->HasMaxAge() &&
      base::StringToInt64(cookie->MaxAge(), seconds_till_expiry)) {
    return true;
  }

  Time parsed_expiry_time;
  if (cookie->HasExpires()) {
    parsed_expiry_time =
        net::cookie_util::ParseCookieExpirationTime(cookie->Expires());
  }

  if (!parsed_expiry_time.is_null()) {
    *seconds_till_expiry =
        ceil((parsed_expiry_time - Time::Now()).InSecondsF());
    return *seconds_till_expiry >= 0;
  }
  return false;
}

bool NullableEquals(const int* a, const int* b) {
  if ((a && !b) || (!a && b))
    return false;
  return (!a) || (*a == *b);
}

bool NullableEquals(const bool* a, const bool* b) {
  if ((a && !b) || (!a && b))
    return false;
  return (!a) || (*a == *b);
}

bool NullableEquals(const std::string* a, const std::string* b) {
  if ((a && !b) || (!a && b))
    return false;
  return (!a) || (*a == *b);
}

}  // namespace

bool ExtraInfoSpec::InitFromValue(const base::ListValue& value,
                                  int* extra_info_spec) {
  *extra_info_spec = 0;
  for (size_t i = 0; i < value.GetSize(); ++i) {
    std::string str;
    if (!value.GetString(i, &str))
      return false;

    if (str == "requestHeaders")
      *extra_info_spec |= REQUEST_HEADERS;
    else if (str == "responseHeaders")
      *extra_info_spec |= RESPONSE_HEADERS;
    else if (str == "blocking")
      *extra_info_spec |= BLOCKING;
    else if (str == "asyncBlocking")
      *extra_info_spec |= ASYNC_BLOCKING;
    else if (str == "requestBody")
      *extra_info_spec |= REQUEST_BODY;
    else
      return false;
  }
  // BLOCKING and ASYNC_BLOCKING are mutually exclusive.
  if ((*extra_info_spec & BLOCKING) && (*extra_info_spec & ASYNC_BLOCKING))
    return false;
  return true;
}

RequestCookie::RequestCookie() {}
RequestCookie::~RequestCookie() {}

bool NullableEquals(const RequestCookie* a, const RequestCookie* b) {
  if ((a && !b) || (!a && b))
    return false;
  if (!a)
    return true;
  return NullableEquals(a->name.get(), b->name.get()) &&
         NullableEquals(a->value.get(), b->value.get());
}

ResponseCookie::ResponseCookie() {}
ResponseCookie::~ResponseCookie() {}

bool NullableEquals(const ResponseCookie* a, const ResponseCookie* b) {
  if ((a && !b) || (!a && b))
    return false;
  if (!a)
    return true;
  return NullableEquals(a->name.get(), b->name.get()) &&
         NullableEquals(a->value.get(), b->value.get()) &&
         NullableEquals(a->expires.get(), b->expires.get()) &&
         NullableEquals(a->max_age.get(), b->max_age.get()) &&
         NullableEquals(a->domain.get(), b->domain.get()) &&
         NullableEquals(a->path.get(), b->path.get()) &&
         NullableEquals(a->secure.get(), b->secure.get()) &&
         NullableEquals(a->http_only.get(), b->http_only.get());
}

FilterResponseCookie::FilterResponseCookie() {}
FilterResponseCookie::~FilterResponseCookie() {}

bool NullableEquals(const FilterResponseCookie* a,
                    const FilterResponseCookie* b) {
  if ((a && !b) || (!a && b))
    return false;
  if (!a)
    return true;
  return NullableEquals(a->age_lower_bound.get(), b->age_lower_bound.get()) &&
         NullableEquals(a->age_upper_bound.get(), b->age_upper_bound.get()) &&
         NullableEquals(a->session_cookie.get(), b->session_cookie.get());
}

RequestCookieModification::RequestCookieModification() {}
RequestCookieModification::~RequestCookieModification() {}

bool NullableEquals(const RequestCookieModification* a,
                    const RequestCookieModification* b) {
  if ((a && !b) || (!a && b))
    return false;
  if (!a)
    return true;
  return NullableEquals(a->filter.get(), b->filter.get()) &&
         NullableEquals(a->modification.get(), b->modification.get());
}

ResponseCookieModification::ResponseCookieModification() : type(ADD) {}
ResponseCookieModification::~ResponseCookieModification() {}

bool NullableEquals(const ResponseCookieModification* a,
                    const ResponseCookieModification* b) {
  if ((a && !b) || (!a && b))
    return false;
  if (!a)
    return true;
  return a->type == b->type &&
         NullableEquals(a->filter.get(), b->filter.get()) &&
         NullableEquals(a->modification.get(), b->modification.get());
}

EventResponseDelta::EventResponseDelta(
    const std::string& extension_id, const base::Time& extension_install_time)
    : extension_id(extension_id),
      extension_install_time(extension_install_time),
      cancel(false) {
}

EventResponseDelta::~EventResponseDelta() {
}

// Creates NetLog parameters to indicate that an extension modified a request.
std::unique_ptr<base::Value> MakeHeaderModificationLogValue(
    const EventResponseDelta* delta) {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetString("extension_id", delta->extension_id);

  auto modified_headers = std::make_unique<base::ListValue>();
  net::HttpRequestHeaders::Iterator modification(
      delta->modified_request_headers);
  while (modification.GetNext()) {
    std::string line = modification.name() + ": " + modification.value();
    modified_headers->AppendString(line);
  }
  dict->Set("modified_headers", std::move(modified_headers));

  auto deleted_headers = std::make_unique<base::ListValue>();
  for (std::vector<std::string>::const_iterator key =
           delta->deleted_request_headers.begin();
       key != delta->deleted_request_headers.end();
       ++key) {
    deleted_headers->AppendString(*key);
  }
  dict->Set("deleted_headers", std::move(deleted_headers));
  return dict;
}

bool InDecreasingExtensionInstallationTimeOrder(
    const linked_ptr<EventResponseDelta>& a,
    const linked_ptr<EventResponseDelta>& b) {
  return a->extension_install_time > b->extension_install_time;
}

std::unique_ptr<base::ListValue> StringToCharList(const std::string& s) {
  auto result = std::make_unique<base::ListValue>();
  for (size_t i = 0, n = s.size(); i < n; ++i) {
    result->AppendInteger(*reinterpret_cast<const unsigned char*>(&s[i]));
  }
  return result;
}

bool CharListToString(const base::ListValue* list, std::string* out) {
  if (!list)
    return false;
  const size_t list_length = list->GetSize();
  out->resize(list_length);
  int value = 0;
  for (size_t i = 0; i < list_length; ++i) {
    if (!list->GetInteger(i, &value) || value < 0 || value > 255)
      return false;
    unsigned char tmp = static_cast<unsigned char>(value);
    (*out)[i] = *reinterpret_cast<char*>(&tmp);
  }
  return true;
}

EventResponseDelta* CalculateOnBeforeRequestDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    const GURL& new_url) {
  EventResponseDelta* result =
      new EventResponseDelta(extension_id, extension_install_time);
  result->cancel = cancel;
  result->new_url = new_url;
  return result;
}

EventResponseDelta* CalculateOnBeforeSendHeadersDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    net::HttpRequestHeaders* old_headers,
    net::HttpRequestHeaders* new_headers) {
  EventResponseDelta* result =
      new EventResponseDelta(extension_id, extension_install_time);
  result->cancel = cancel;

  // The event listener might not have passed any new headers if it
  // just wanted to cancel the request.
  if (new_headers) {
    // Find deleted headers.
    {
      net::HttpRequestHeaders::Iterator i(*old_headers);
      while (i.GetNext()) {
        if (!new_headers->HasHeader(i.name())) {
          result->deleted_request_headers.push_back(i.name());
        }
      }
    }

    // Find modified headers.
    {
      net::HttpRequestHeaders::Iterator i(*new_headers);
      while (i.GetNext()) {
        std::string value;
        if (!old_headers->GetHeader(i.name(), &value) || i.value() != value) {
          result->modified_request_headers.SetHeader(i.name(), i.value());
        }
      }
    }
  }
  return result;
}

EventResponseDelta* CalculateOnHeadersReceivedDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    const GURL& old_url,
    const GURL& new_url,
    const net::HttpResponseHeaders* old_response_headers,
    ResponseHeaders* new_response_headers) {
  EventResponseDelta* result =
      new EventResponseDelta(extension_id, extension_install_time);
  result->cancel = cancel;
  result->new_url = new_url;

  if (!new_response_headers)
    return result;

  extensions::ExtensionsAPIClient* api_client =
      extensions::ExtensionsAPIClient::Get();

  // Find deleted headers (header keys are treated case insensitively).
  {
    size_t iter = 0;
    std::string name;
    std::string value;
    while (old_response_headers->EnumerateHeaderLines(&iter, &name, &value)) {
      if (api_client->ShouldHideResponseHeader(old_url, name))
        continue;
      std::string name_lowercase = base::ToLowerASCII(name);
      bool header_found = false;
      for (const auto& i : *new_response_headers) {
        if (base::LowerCaseEqualsASCII(i.first, name_lowercase) &&
            value == i.second) {
          header_found = true;
          break;
        }
      }
      if (!header_found)
        result->deleted_response_headers.push_back(ResponseHeader(name, value));
    }
  }

  // Find added headers (header keys are treated case insensitively).
  {
    for (const auto& i : *new_response_headers) {
      if (api_client->ShouldHideResponseHeader(old_url, i.first))
        continue;
      std::string name_lowercase = base::ToLowerASCII(i.first);
      size_t iter = 0;
      std::string name;
      std::string value;
      bool header_found = false;
      while (old_response_headers->EnumerateHeaderLines(&iter, &name, &value)) {
        if (base::LowerCaseEqualsASCII(name, name_lowercase) &&
            value == i.second) {
          header_found = true;
          break;
        }
      }
      if (!header_found)
        result->added_response_headers.push_back(i);
    }
  }

  return result;
}

EventResponseDelta* CalculateOnAuthRequiredDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    std::unique_ptr<net::AuthCredentials>* auth_credentials) {
  EventResponseDelta* result =
      new EventResponseDelta(extension_id, extension_install_time);
  result->cancel = cancel;
  result->auth_credentials.swap(*auth_credentials);
  return result;
}

void MergeCancelOfResponses(const EventResponseDeltas& deltas,
                            bool* canceled,
                            extensions::WebRequestInfo::Logger* logger) {
  for (EventResponseDeltas::const_iterator i = deltas.begin();
       i != deltas.end(); ++i) {
    if ((*i)->cancel) {
      *canceled = true;
      logger->LogEvent(net::NetLogEventType::CHROME_EXTENSION_ABORTED_REQUEST,
                       (*i)->extension_id);
      break;
    }
  }
}

// Helper function for MergeRedirectUrlOfResponses() that allows ignoring
// all redirects but those to data:// urls and about:blank. This is important
// to treat these URLs as "cancel urls", i.e. URLs that extensions redirect
// to if they want to express that they want to cancel a request. This reduces
// the number of conflicts that we need to flag, as canceling is considered
// a higher precedence operation that redirects.
// Returns whether a redirect occurred.
static bool MergeRedirectUrlOfResponsesHelper(
    const GURL& url,
    const EventResponseDeltas& deltas,
    GURL* new_url,
    extensions::WarningSet* conflicting_extensions,
    extensions::WebRequestInfo::Logger* logger,
    bool consider_only_cancel_scheme_urls) {
  // Redirecting WebSocket handshake request is prohibited.
  if (url.SchemeIsWSOrWSS())
    return false;

  bool redirected = false;

  // Extension that determines the |new_url|.
  std::string winning_extension_id;
  EventResponseDeltas::const_iterator delta;
  for (delta = deltas.begin(); delta != deltas.end(); ++delta) {
    if ((*delta)->new_url.is_empty())
      continue;
    if (consider_only_cancel_scheme_urls &&
        !(*delta)->new_url.SchemeIs(url::kDataScheme) &&
        (*delta)->new_url.spec() != "about:blank") {
      continue;
    }

    if (!redirected || *new_url == (*delta)->new_url) {
      *new_url = (*delta)->new_url;
      winning_extension_id = (*delta)->extension_id;
      redirected = true;
      logger->LogEvent(
          net::NetLogEventType::CHROME_EXTENSION_REDIRECTED_REQUEST,
          (*delta)->extension_id);
    } else {
      conflicting_extensions->insert(
          extensions::Warning::CreateRedirectConflictWarning(
              (*delta)->extension_id,
              winning_extension_id,
              (*delta)->new_url,
              *new_url));
      logger->LogEvent(
          net::NetLogEventType::CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
          (*delta)->extension_id);
    }
  }
  return redirected;
}

void MergeRedirectUrlOfResponses(const GURL& url,
                                 const EventResponseDeltas& deltas,
                                 GURL* new_url,
                                 extensions::WarningSet* conflicting_extensions,
                                 extensions::WebRequestInfo::Logger* logger) {
  // First handle only redirects to data:// URLs and about:blank. These are a
  // special case as they represent a way of cancelling a request.
  if (MergeRedirectUrlOfResponsesHelper(url, deltas, new_url,
                                        conflicting_extensions, logger, true)) {
    // If any extension cancelled a request by redirecting to a data:// URL or
    // about:blank, we don't consider the other redirects.
    return;
  }

  // Handle all other redirects.
  MergeRedirectUrlOfResponsesHelper(url, deltas, new_url,
                                    conflicting_extensions, logger, false);
}

void MergeOnBeforeRequestResponses(
    const GURL& url,
    const EventResponseDeltas& deltas,
    GURL* new_url,
    extensions::WarningSet* conflicting_extensions,
    extensions::WebRequestInfo::Logger* logger) {
  MergeRedirectUrlOfResponses(url, deltas, new_url, conflicting_extensions,
                              logger);
}

static bool DoesRequestCookieMatchFilter(
    const ParsedRequestCookie& cookie,
    RequestCookie* filter) {
  if (!filter) return true;
  if (filter->name.get() && cookie.first != *filter->name) return false;
  if (filter->value.get() && cookie.second != *filter->value) return false;
  return true;
}

// Applies all CookieModificationType::ADD operations for request cookies of
// |deltas| to |cookies|. Returns whether any cookie was added.
static bool MergeAddRequestCookieModifications(
    const EventResponseDeltas& deltas,
    ParsedRequestCookies* cookies) {
  bool modified = false;
  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  EventResponseDeltas::const_reverse_iterator delta;
  for (delta = deltas.rbegin(); delta != deltas.rend(); ++delta) {
    const RequestCookieModifications& modifications =
        (*delta)->request_cookie_modifications;
    for (RequestCookieModifications::const_iterator mod = modifications.begin();
         mod != modifications.end(); ++mod) {
      if ((*mod)->type != ADD || !(*mod)->modification.get())
        continue;
      std::string* new_name = (*mod)->modification->name.get();
      std::string* new_value = (*mod)->modification->value.get();
      if (!new_name || !new_value)
        continue;

      bool cookie_with_same_name_found = false;
      for (ParsedRequestCookies::iterator cookie = cookies->begin();
           cookie != cookies->end() && !cookie_with_same_name_found; ++cookie) {
        if (cookie->first == *new_name) {
          if (cookie->second != *new_value) {
            cookie->second = *new_value;
            modified = true;
          }
          cookie_with_same_name_found = true;
        }
      }
      if (!cookie_with_same_name_found) {
        cookies->push_back(std::make_pair(base::StringPiece(*new_name),
                                          base::StringPiece(*new_value)));
        modified = true;
      }
    }
  }
  return modified;
}

// Applies all CookieModificationType::EDIT operations for request cookies of
// |deltas| to |cookies|. Returns whether any cookie was modified.
static bool MergeEditRequestCookieModifications(
    const EventResponseDeltas& deltas,
    ParsedRequestCookies* cookies) {
  bool modified = false;
  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  EventResponseDeltas::const_reverse_iterator delta;
  for (delta = deltas.rbegin(); delta != deltas.rend(); ++delta) {
    const RequestCookieModifications& modifications =
        (*delta)->request_cookie_modifications;
    for (RequestCookieModifications::const_iterator mod = modifications.begin();
         mod != modifications.end(); ++mod) {
      if ((*mod)->type != EDIT || !(*mod)->modification.get())
        continue;

      std::string* new_value = (*mod)->modification->value.get();
      RequestCookie* filter = (*mod)->filter.get();
      for (ParsedRequestCookies::iterator cookie = cookies->begin();
           cookie != cookies->end(); ++cookie) {
        if (!DoesRequestCookieMatchFilter(*cookie, filter))
          continue;
        // If the edit operation tries to modify the cookie name, we just ignore
        // this. We only modify the cookie value.
        if (new_value && cookie->second != *new_value) {
          cookie->second = *new_value;
          modified = true;
        }
      }
    }
  }
  return modified;
}

// Applies all CookieModificationType::REMOVE operations for request cookies of
// |deltas| to |cookies|. Returns whether any cookie was deleted.
static bool MergeRemoveRequestCookieModifications(
    const EventResponseDeltas& deltas,
    ParsedRequestCookies* cookies) {
  bool modified = false;
  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  EventResponseDeltas::const_reverse_iterator delta;
  for (delta = deltas.rbegin(); delta != deltas.rend(); ++delta) {
    const RequestCookieModifications& modifications =
        (*delta)->request_cookie_modifications;
    for (RequestCookieModifications::const_iterator mod = modifications.begin();
         mod != modifications.end(); ++mod) {
      if ((*mod)->type != REMOVE)
        continue;

      RequestCookie* filter = (*mod)->filter.get();
      ParsedRequestCookies::iterator i = cookies->begin();
      while (i != cookies->end()) {
        if (DoesRequestCookieMatchFilter(*i, filter)) {
          i = cookies->erase(i);
          modified = true;
        } else {
          ++i;
        }
      }
    }
  }
  return modified;
}

void MergeCookiesInOnBeforeSendHeadersResponses(
    const EventResponseDeltas& deltas,
    net::HttpRequestHeaders* request_headers,
    extensions::WarningSet* conflicting_extensions,
    extensions::WebRequestInfo::Logger* logger) {
  // Skip all work if there are no registered cookie modifications.
  bool cookie_modifications_exist = false;
  EventResponseDeltas::const_iterator delta;
  for (delta = deltas.begin(); delta != deltas.end(); ++delta) {
    cookie_modifications_exist |=
        !(*delta)->request_cookie_modifications.empty();
  }
  if (!cookie_modifications_exist)
    return;

  // Parse old cookie line.
  std::string cookie_header;
  request_headers->GetHeader(net::HttpRequestHeaders::kCookie, &cookie_header);
  ParsedRequestCookies cookies;
  net::cookie_util::ParseRequestCookieLine(cookie_header, &cookies);

  // Modify cookies.
  bool modified = false;
  modified |= MergeAddRequestCookieModifications(deltas, &cookies);
  modified |= MergeEditRequestCookieModifications(deltas, &cookies);
  modified |= MergeRemoveRequestCookieModifications(deltas, &cookies);

  // Reassemble and store new cookie line.
  if (modified) {
    std::string new_cookie_header =
        net::cookie_util::SerializeRequestCookieLine(cookies);
    request_headers->SetHeader(net::HttpRequestHeaders::kCookie,
                               new_cookie_header);
  }
}

// Returns the extension ID of the first extension in |deltas| that sets the
// request header identified by |key| to |value|.
static std::string FindSetRequestHeader(
    const EventResponseDeltas& deltas,
    const std::string& key,
    const std::string& value) {
  EventResponseDeltas::const_iterator delta;
  for (delta = deltas.begin(); delta != deltas.end(); ++delta) {
    net::HttpRequestHeaders::Iterator modification(
        (*delta)->modified_request_headers);
    while (modification.GetNext()) {
      if (base::EqualsCaseInsensitiveASCII(key, modification.name()) &&
          value == modification.value())
        return (*delta)->extension_id;
    }
  }
  return std::string();
}

// Returns the extension ID of the first extension in |deltas| that removes the
// request header identified by |key|.
static std::string FindRemoveRequestHeader(
    const EventResponseDeltas& deltas,
    const std::string& key) {
  EventResponseDeltas::const_iterator delta;
  for (delta = deltas.begin(); delta != deltas.end(); ++delta) {
    std::vector<std::string>::iterator i;
    for (i = (*delta)->deleted_request_headers.begin();
         i != (*delta)->deleted_request_headers.end();
         ++i) {
      if (base::EqualsCaseInsensitiveASCII(*i, key))
        return (*delta)->extension_id;
    }
  }
  return std::string();
}

void MergeOnBeforeSendHeadersResponses(
    const EventResponseDeltas& deltas,
    net::HttpRequestHeaders* request_headers,
    extensions::WarningSet* conflicting_extensions,
    extensions::WebRequestInfo::Logger* logger,
    bool* request_headers_modified) {
  DCHECK(request_headers_modified);
  *request_headers_modified = false;

  EventResponseDeltas::const_iterator delta;

  // Here we collect which headers we have removed or set to new values
  // so far due to extensions of higher precedence.
  std::set<std::string> removed_headers;
  std::set<std::string> set_headers;

  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  for (delta = deltas.begin(); delta != deltas.end(); ++delta) {
    if ((*delta)->modified_request_headers.IsEmpty() &&
        (*delta)->deleted_request_headers.empty()) {
      continue;
    }

    // Check whether any modification affects a request header that
    // has been modified differently before. As deltas is sorted by decreasing
    // extension installation order, this takes care of precedence.
    bool extension_conflicts = false;
    std::string winning_extension_id;
    std::string conflicting_header;
    {
      net::HttpRequestHeaders::Iterator modification(
          (*delta)->modified_request_headers);
      while (modification.GetNext() && !extension_conflicts) {
        // This modification sets |key| to |value|.
        const std::string& key = modification.name();
        const std::string& value = modification.value();

        // We must not delete anything that has been modified before.
        if (removed_headers.find(key) != removed_headers.end() &&
            !extension_conflicts) {
          winning_extension_id = FindRemoveRequestHeader(deltas, key);
          conflicting_header = key;
          extension_conflicts = true;
        }

        // We must not modify anything that has been set to a *different*
        // value before.
        if (set_headers.find(key) != set_headers.end() &&
            !extension_conflicts) {
          std::string current_value;
          if (!request_headers->GetHeader(key, &current_value) ||
              current_value != value) {
            winning_extension_id =
                FindSetRequestHeader(deltas, key, current_value);
            conflicting_header = key;
            extension_conflicts = true;
          }
        }
      }
    }

    // Check whether any deletion affects a request header that has been
    // modified before.
    {
      std::vector<std::string>::iterator key;
      for (key = (*delta)->deleted_request_headers.begin();
           key != (*delta)->deleted_request_headers.end() &&
               !extension_conflicts;
           ++key) {
        if (set_headers.find(*key) != set_headers.end()) {
          std::string current_value;
          request_headers->GetHeader(*key, &current_value);
          winning_extension_id =
              FindSetRequestHeader(deltas, *key, current_value);
          conflicting_header = *key;
          extension_conflicts = true;
        }
      }
    }

    // Now execute the modifications if there were no conflicts.
    if (!extension_conflicts) {
      // Copy all modifications into the original headers.
      request_headers->MergeFrom((*delta)->modified_request_headers);
      {
        // Record which keys were changed.
        net::HttpRequestHeaders::Iterator modification(
            (*delta)->modified_request_headers);
        while (modification.GetNext())
          set_headers.insert(modification.name());
      }

      // Perform all deletions and record which keys were deleted.
      {
        std::vector<std::string>::iterator key;
        for (key = (*delta)->deleted_request_headers.begin();
             key != (*delta)->deleted_request_headers.end();
             ++key) {
          request_headers->RemoveHeader(*key);
          removed_headers.insert(*key);
        }
      }
      logger->LogEvent(net::NetLogEventType::CHROME_EXTENSION_MODIFIED_HEADERS,
                       (*delta)->extension_id);
      *request_headers_modified = true;
    } else {
      conflicting_extensions->insert(
          extensions::Warning::CreateRequestHeaderConflictWarning(
              (*delta)->extension_id, winning_extension_id,
              conflicting_header));
      logger->LogEvent(
          net::NetLogEventType::CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
          (*delta)->extension_id);
    }
  }

  MergeCookiesInOnBeforeSendHeadersResponses(deltas, request_headers,
                                             conflicting_extensions, logger);
}

// Retrives all cookies from |override_response_headers|.
static ParsedResponseCookies GetResponseCookies(
    scoped_refptr<net::HttpResponseHeaders> override_response_headers) {
  ParsedResponseCookies result;

  size_t iter = 0;
  std::string value;
  while (override_response_headers->EnumerateHeader(&iter, "Set-Cookie",
                                                    &value)) {
    result.push_back(make_linked_ptr(new net::ParsedCookie(value)));
  }
  return result;
}

// Stores all |cookies| in |override_response_headers| deleting previously
// existing cookie definitions.
static void StoreResponseCookies(
    const ParsedResponseCookies& cookies,
    scoped_refptr<net::HttpResponseHeaders> override_response_headers) {
  override_response_headers->RemoveHeader("Set-Cookie");
  for (ParsedResponseCookies::const_iterator i = cookies.begin();
       i != cookies.end(); ++i) {
    override_response_headers->AddHeader("Set-Cookie: " + (*i)->ToCookieLine());
  }
}

// Modifies |cookie| according to |modification|. Each value that is set in
// |modification| is applied to |cookie|.
static bool ApplyResponseCookieModification(ResponseCookie* modification,
                                            net::ParsedCookie* cookie) {
  bool modified = false;
  if (modification->name.get())
    modified |= cookie->SetName(*modification->name);
  if (modification->value.get())
    modified |= cookie->SetValue(*modification->value);
  if (modification->expires.get())
    modified |= cookie->SetExpires(*modification->expires);
  if (modification->max_age.get())
    modified |= cookie->SetMaxAge(base::IntToString(*modification->max_age));
  if (modification->domain.get())
    modified |= cookie->SetDomain(*modification->domain);
  if (modification->path.get())
    modified |= cookie->SetPath(*modification->path);
  if (modification->secure.get())
    modified |= cookie->SetIsSecure(*modification->secure);
  if (modification->http_only.get())
    modified |= cookie->SetIsHttpOnly(*modification->http_only);
  return modified;
}

static bool DoesResponseCookieMatchFilter(net::ParsedCookie* cookie,
                                          FilterResponseCookie* filter) {
  if (!cookie->IsValid()) return false;
  if (!filter) return true;
  if (filter->name && cookie->Name() != *filter->name)
    return false;
  if (filter->value && cookie->Value() != *filter->value)
    return false;
  if (filter->expires) {
    std::string actual_value =
        cookie->HasExpires() ? cookie->Expires() : std::string();
    if (actual_value != *filter->expires)
      return false;
  }
  if (filter->max_age) {
    std::string actual_value =
        cookie->HasMaxAge() ? cookie->MaxAge() : std::string();
    if (actual_value != base::IntToString(*filter->max_age))
      return false;
  }
  if (filter->domain) {
    std::string actual_value =
        cookie->HasDomain() ? cookie->Domain() : std::string();
    if (actual_value != *filter->domain)
      return false;
  }
  if (filter->path) {
    std::string actual_value =
        cookie->HasPath() ? cookie->Path() : std::string();
    if (actual_value != *filter->path)
      return false;
  }
  if (filter->secure && cookie->IsSecure() != *filter->secure)
    return false;
  if (filter->http_only && cookie->IsHttpOnly() != *filter->http_only)
    return false;
  if (filter->age_upper_bound || filter->age_lower_bound ||
      (filter->session_cookie && *filter->session_cookie)) {
    int64_t seconds_to_expiry;
    bool lifetime_parsed = ParseCookieLifetime(cookie, &seconds_to_expiry);
    if (filter->age_upper_bound && seconds_to_expiry > *filter->age_upper_bound)
      return false;
    if (filter->age_lower_bound && seconds_to_expiry < *filter->age_lower_bound)
      return false;
    if (filter->session_cookie && *filter->session_cookie && lifetime_parsed)
      return false;
  }
  return true;
}

// Applies all CookieModificationType::ADD operations for response cookies of
// |deltas| to |cookies|. Returns whether any cookie was added.
static bool MergeAddResponseCookieModifications(
    const EventResponseDeltas& deltas,
    ParsedResponseCookies* cookies) {
  bool modified = false;
  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  EventResponseDeltas::const_reverse_iterator delta;
  for (delta = deltas.rbegin(); delta != deltas.rend(); ++delta) {
    const ResponseCookieModifications& modifications =
        (*delta)->response_cookie_modifications;
    for (ResponseCookieModifications::const_iterator mod =
             modifications.begin(); mod != modifications.end(); ++mod) {
      if ((*mod)->type != ADD || !(*mod)->modification.get())
        continue;
      // Cookie names are not unique in response cookies so we always append
      // and never override.
      linked_ptr<net::ParsedCookie> cookie(
          new net::ParsedCookie(std::string()));
      ApplyResponseCookieModification((*mod)->modification.get(), cookie.get());
      cookies->push_back(cookie);
      modified = true;
    }
  }
  return modified;
}

// Applies all CookieModificationType::EDIT operations for response cookies of
// |deltas| to |cookies|. Returns whether any cookie was modified.
static bool MergeEditResponseCookieModifications(
    const EventResponseDeltas& deltas,
    ParsedResponseCookies* cookies) {
  bool modified = false;
  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  EventResponseDeltas::const_reverse_iterator delta;
  for (delta = deltas.rbegin(); delta != deltas.rend(); ++delta) {
    const ResponseCookieModifications& modifications =
        (*delta)->response_cookie_modifications;
    for (ResponseCookieModifications::const_iterator mod =
             modifications.begin(); mod != modifications.end(); ++mod) {
      if ((*mod)->type != EDIT || !(*mod)->modification.get())
        continue;

      for (ParsedResponseCookies::iterator cookie = cookies->begin();
           cookie != cookies->end(); ++cookie) {
        if (DoesResponseCookieMatchFilter(cookie->get(),
                                          (*mod)->filter.get())) {
          modified |= ApplyResponseCookieModification(
              (*mod)->modification.get(), cookie->get());
        }
      }
    }
  }
  return modified;
}

// Applies all CookieModificationType::REMOVE operations for response cookies of
// |deltas| to |cookies|. Returns whether any cookie was deleted.
static bool MergeRemoveResponseCookieModifications(
    const EventResponseDeltas& deltas,
    ParsedResponseCookies* cookies) {
  bool modified = false;
  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  EventResponseDeltas::const_reverse_iterator delta;
  for (delta = deltas.rbegin(); delta != deltas.rend(); ++delta) {
    const ResponseCookieModifications& modifications =
        (*delta)->response_cookie_modifications;
    for (ResponseCookieModifications::const_iterator mod =
             modifications.begin(); mod != modifications.end(); ++mod) {
      if ((*mod)->type != REMOVE)
        continue;

      ParsedResponseCookies::iterator i = cookies->begin();
      while (i != cookies->end()) {
        if (DoesResponseCookieMatchFilter(i->get(),
                                          (*mod)->filter.get())) {
          i = cookies->erase(i);
          modified = true;
        } else {
          ++i;
        }
      }
    }
  }
  return modified;
}

void MergeCookiesInOnHeadersReceivedResponses(
    const EventResponseDeltas& deltas,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    extensions::WarningSet* conflicting_extensions,
    extensions::WebRequestInfo::Logger* logger) {
  // Skip all work if there are no registered cookie modifications.
  bool cookie_modifications_exist = false;
  EventResponseDeltas::const_reverse_iterator delta;
  for (delta = deltas.rbegin(); delta != deltas.rend(); ++delta) {
    cookie_modifications_exist |=
        !(*delta)->response_cookie_modifications.empty();
  }
  if (!cookie_modifications_exist)
    return;

  // Only create a copy if we really want to modify the response headers.
  if (override_response_headers->get() == NULL) {
    *override_response_headers = new net::HttpResponseHeaders(
        original_response_headers->raw_headers());
  }

  ParsedResponseCookies cookies =
      GetResponseCookies(*override_response_headers);

  bool modified = false;
  modified |= MergeAddResponseCookieModifications(deltas, &cookies);
  modified |= MergeEditResponseCookieModifications(deltas, &cookies);
  modified |= MergeRemoveResponseCookieModifications(deltas, &cookies);

  // Store new value.
  if (modified)
    StoreResponseCookies(cookies, *override_response_headers);
}

// Converts the key of the (key, value) pair to lower case.
static ResponseHeader ToLowerCase(const ResponseHeader& header) {
  return ResponseHeader(base::ToLowerASCII(header.first), header.second);
}

// Returns the extension ID of the first extension in |deltas| that removes the
// request header identified by |key|.
static std::string FindRemoveResponseHeader(
    const EventResponseDeltas& deltas,
    const std::string& key) {
  std::string lower_key = base::ToLowerASCII(key);
  for (const auto& delta : deltas) {
    for (const auto& deleted_hdr : delta->deleted_response_headers) {
      if (base::ToLowerASCII(deleted_hdr.first) == lower_key)
        return delta->extension_id;
    }
  }
  return std::string();
}

void MergeOnHeadersReceivedResponses(
    const GURL& url,
    const EventResponseDeltas& deltas,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url,
    extensions::WarningSet* conflicting_extensions,
    extensions::WebRequestInfo::Logger* logger,
    bool* response_headers_modified) {
  DCHECK(response_headers_modified);
  *response_headers_modified = false;

  EventResponseDeltas::const_iterator delta;

  // Here we collect which headers we have removed or added so far due to
  // extensions of higher precedence. Header keys are always stored as
  // lower case.
  std::set<ResponseHeader> removed_headers;
  std::set<ResponseHeader> added_headers;

  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  for (delta = deltas.begin(); delta != deltas.end(); ++delta) {
    if ((*delta)->added_response_headers.empty() &&
        (*delta)->deleted_response_headers.empty()) {
      continue;
    }

    // Only create a copy if we really want to modify the response headers.
    if (override_response_headers->get() == NULL) {
      *override_response_headers = new net::HttpResponseHeaders(
          original_response_headers->raw_headers());
    }

    // We consider modifications as pairs of (delete, add) operations.
    // If a header is deleted twice by different extensions we assume that the
    // intention was to modify it to different values and consider this a
    // conflict. As deltas is sorted by decreasing extension installation order,
    // this takes care of precedence.
    bool extension_conflicts = false;
    std::string conflicting_header;
    std::string winning_extension_id;
    ResponseHeaders::const_iterator i;
    for (i = (*delta)->deleted_response_headers.begin();
         i != (*delta)->deleted_response_headers.end(); ++i) {
      if (removed_headers.find(ToLowerCase(*i)) != removed_headers.end()) {
        winning_extension_id = FindRemoveResponseHeader(deltas, i->first);
        conflicting_header = i->first;
        extension_conflicts = true;
        break;
      }
    }

    // Now execute the modifications if there were no conflicts.
    if (!extension_conflicts) {
      // Delete headers
      {
        for (i = (*delta)->deleted_response_headers.begin();
             i != (*delta)->deleted_response_headers.end(); ++i) {
          (*override_response_headers)->RemoveHeaderLine(i->first, i->second);
          removed_headers.insert(ToLowerCase(*i));
        }
      }

      // Add headers.
      {
        for (i = (*delta)->added_response_headers.begin();
             i != (*delta)->added_response_headers.end(); ++i) {
          ResponseHeader lowercase_header(ToLowerCase(*i));
          if (added_headers.find(lowercase_header) != added_headers.end())
            continue;
          added_headers.insert(lowercase_header);
          (*override_response_headers)->AddHeader(i->first + ": " + i->second);
        }
      }
      logger->LogEvent(net::NetLogEventType::CHROME_EXTENSION_MODIFIED_HEADERS,
                       (*delta)->extension_id);
      *response_headers_modified = true;
    } else {
      conflicting_extensions->insert(
          extensions::Warning::CreateResponseHeaderConflictWarning(
              (*delta)->extension_id, winning_extension_id,
              conflicting_header));
      logger->LogEvent(
          net::NetLogEventType::CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
          (*delta)->extension_id);
    }
  }

  MergeCookiesInOnHeadersReceivedResponses(deltas, original_response_headers,
                                           override_response_headers,
                                           conflicting_extensions, logger);

  GURL new_url;
  MergeRedirectUrlOfResponses(url, deltas, &new_url, conflicting_extensions,
                              logger);
  if (new_url.is_valid()) {
    // Only create a copy if we really want to modify the response headers.
    if (override_response_headers->get() == NULL) {
      *override_response_headers = new net::HttpResponseHeaders(
          original_response_headers->raw_headers());
    }
    (*override_response_headers)->ReplaceStatusLine("HTTP/1.1 302 Found");
    (*override_response_headers)->RemoveHeader("location");
    (*override_response_headers)->AddHeader("Location: " + new_url.spec());
    // Explicitly mark the URL as safe for redirection, to prevent the request
    // from being blocked because of net::ERR_UNSAFE_REDIRECT.
    *allowed_unsafe_redirect_url = new_url;
  }
}

bool MergeOnAuthRequiredResponses(
    const EventResponseDeltas& deltas,
    net::AuthCredentials* auth_credentials,
    extensions::WarningSet* conflicting_extensions,
    extensions::WebRequestInfo::Logger* logger) {
  CHECK(auth_credentials);
  bool credentials_set = false;
  std::string winning_extension_id;

  for (EventResponseDeltas::const_iterator delta = deltas.begin();
       delta != deltas.end();
       ++delta) {
    if (!(*delta)->auth_credentials.get())
      continue;
    bool different =
        auth_credentials->username() !=
            (*delta)->auth_credentials->username() ||
        auth_credentials->password() != (*delta)->auth_credentials->password();
    if (credentials_set && different) {
      conflicting_extensions->insert(
          extensions::Warning::CreateCredentialsConflictWarning(
              (*delta)->extension_id, winning_extension_id));
      logger->LogEvent(
          net::NetLogEventType::CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
          (*delta)->extension_id);
    } else {
      logger->LogEvent(
          net::NetLogEventType::CHROME_EXTENSION_PROVIDE_AUTH_CREDENTIALS,
          (*delta)->extension_id);
      *auth_credentials = *(*delta)->auth_credentials;
      credentials_set = true;
      winning_extension_id = (*delta)->extension_id;
    }
  }
  return credentials_set;
}

void ClearCacheOnNavigation() {
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    ClearCacheOnNavigationOnUI();
  } else {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::Bind(&ClearCacheOnNavigationOnUI));
  }
}

// Converts the |name|, |value| pair of a http header to a HttpHeaders
// dictionary.
std::unique_ptr<base::DictionaryValue> CreateHeaderDictionary(
    const std::string& name,
    const std::string& value) {
  auto header = std::make_unique<base::DictionaryValue>();
  header->SetString(keys::kHeaderNameKey, name);
  if (base::IsStringUTF8(value)) {
    header->SetString(keys::kHeaderValueKey, value);
  } else {
    header->Set(keys::kHeaderBinaryValueKey,
                StringToCharList(value));
  }
  return header;
}

}  // namespace extension_web_request_api_helpers
