// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_request/web_request_api_helpers.h"

#include <cmath>

#include "base/bind.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/extension_warning_set.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_log.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"

// TODO(battre): move all static functions into an anonymous namespace at the
// top of this file.

using base::Time;
using extensions::ExtensionWarning;

namespace extension_web_request_api_helpers {

namespace {

// A ParsedRequestCookie consists of the key and value of the cookie.
typedef std::pair<base::StringPiece, base::StringPiece> ParsedRequestCookie;
typedef std::vector<ParsedRequestCookie> ParsedRequestCookies;
typedef std::vector<linked_ptr<net::ParsedCookie> > ParsedResponseCookies;

static const char* kResourceTypeStrings[] = {
  "main_frame",
  "sub_frame",
  "stylesheet",
  "script",
  "image",
  "object",
  "xmlhttprequest",
  "other",
  "other",
};

static ResourceType::Type kResourceTypeValues[] = {
  ResourceType::MAIN_FRAME,
  ResourceType::SUB_FRAME,
  ResourceType::STYLESHEET,
  ResourceType::SCRIPT,
  ResourceType::IMAGE,
  ResourceType::OBJECT,
  ResourceType::XHR,
  ResourceType::LAST_TYPE,  // represents "other"
  // TODO(jochen): We duplicate the last entry, so the array's size is not a
  // power of two. If it is, this triggers a bug in gcc 4.4 in Release builds
  // (http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43949). Once we use a version
  // of gcc with this bug fixed, or the array is changed so this duplicate
  // entry is no longer required, this should be removed.
  ResourceType::LAST_TYPE,
};

COMPILE_ASSERT(
    arraysize(kResourceTypeStrings) == arraysize(kResourceTypeValues),
    keep_resource_types_in_sync);

void ClearCacheOnNavigationOnUI() {
  WebCacheManager::GetInstance()->ClearCacheOnNavigation();
}

bool ParseCookieLifetime(net::ParsedCookie* cookie,
                         int64* seconds_till_expiry) {
  // 'Max-Age' is processed first because according to:
  // http://tools.ietf.org/html/rfc6265#section-5.3 'Max-Age' attribute
  // overrides 'Expires' attribute.
  if (cookie->HasMaxAge() &&
      base::StringToInt64(cookie->MaxAge(), seconds_till_expiry)) {
    return true;
  }

  Time parsed_expiry_time;
  if (cookie->HasExpires())
    parsed_expiry_time = net::cookie_util::ParseCookieTime(cookie->Expires());

  if (!parsed_expiry_time.is_null()) {
    *seconds_till_expiry =
        ceil((parsed_expiry_time - Time::Now()).InSecondsF());
    return *seconds_till_expiry >= 0;
  }
  return false;
}

}  // namespace

RequestCookie::RequestCookie() {}
RequestCookie::~RequestCookie() {}

ResponseCookie::ResponseCookie() {}
ResponseCookie::~ResponseCookie() {}

FilterResponseCookie::FilterResponseCookie() {}
FilterResponseCookie::~FilterResponseCookie() {}

RequestCookieModification::RequestCookieModification() {}
RequestCookieModification::~RequestCookieModification() {}

ResponseCookieModification::ResponseCookieModification() : type(ADD) {}
ResponseCookieModification::~ResponseCookieModification() {}

EventResponseDelta::EventResponseDelta(
    const std::string& extension_id, const base::Time& extension_install_time)
    : extension_id(extension_id),
      extension_install_time(extension_install_time),
      cancel(false) {
}

EventResponseDelta::~EventResponseDelta() {
}


// Creates a NetLog callback the returns a Value with the ID of the extension
// that caused an event.  |delta| must remain valid for the lifetime of the
// callback.
net::NetLog::ParametersCallback CreateNetLogExtensionIdCallback(
    const EventResponseDelta* delta) {
  return net::NetLog::StringCallback("extension_id", &delta->extension_id);
}

// Creates NetLog parameters to indicate that an extension modified a request.
// Caller takes ownership of returned value.
Value* NetLogModificationCallback(
    const EventResponseDelta* delta,
    net::NetLog::LogLevel log_level) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("extension_id", delta->extension_id);

  ListValue* modified_headers = new ListValue();
  net::HttpRequestHeaders::Iterator modification(
      delta->modified_request_headers);
  while (modification.GetNext()) {
    std::string line = modification.name() + ": " + modification.value();
    modified_headers->Append(Value::CreateStringValue(line));
  }
  dict->Set("modified_headers", modified_headers);

  ListValue* deleted_headers = new ListValue();
  for (std::vector<std::string>::const_iterator key =
           delta->deleted_request_headers.begin();
       key != delta->deleted_request_headers.end();
       ++key) {
    deleted_headers->Append(Value::CreateStringValue(*key));
  }
  dict->Set("deleted_headers", deleted_headers);
  return dict;
}

bool InDecreasingExtensionInstallationTimeOrder(
    const linked_ptr<EventResponseDelta>& a,
    const linked_ptr<EventResponseDelta>& b) {
  return a->extension_install_time > b->extension_install_time;
}

ListValue* StringToCharList(const std::string& s) {
  ListValue* result = new ListValue;
  for (size_t i = 0, n = s.size(); i < n; ++i) {
    result->Append(
        Value::CreateIntegerValue(
            *reinterpret_cast<const unsigned char*>(&s[i])));
  }
  return result;
}

bool CharListToString(const ListValue* list, std::string* out) {
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

  // The event listener might not have passed any new headers if he
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
    const net::HttpResponseHeaders* old_response_headers,
    ResponseHeaders* new_response_headers) {
  EventResponseDelta* result =
      new EventResponseDelta(extension_id, extension_install_time);
  result->cancel = cancel;

  if (!new_response_headers)
    return result;

  // Find deleted headers (header keys are treated case insensitively).
  {
    void* iter = NULL;
    std::string name;
    std::string value;
    while (old_response_headers->EnumerateHeaderLines(&iter, &name, &value)) {
      std::string name_lowercase(name);
      StringToLowerASCII(&name_lowercase);

      bool header_found = false;
      for (ResponseHeaders::const_iterator i = new_response_headers->begin();
           i != new_response_headers->end(); ++i) {
        if (LowerCaseEqualsASCII(i->first, name_lowercase.c_str()) &&
            value == i->second) {
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
    for (ResponseHeaders::const_iterator i = new_response_headers->begin();
         i != new_response_headers->end(); ++i) {
      void* iter = NULL;
      std::string value;
      bool header_found = false;
      while (old_response_headers->EnumerateHeader(&iter, i->first, &value) &&
             !header_found) {
        header_found = (value == i->second);
      }
      if (!header_found)
        result->added_response_headers.push_back(*i);
    }
  }

  return result;
}

EventResponseDelta* CalculateOnAuthRequiredDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    scoped_ptr<net::AuthCredentials>* auth_credentials) {
  EventResponseDelta* result =
      new EventResponseDelta(extension_id, extension_install_time);
  result->cancel = cancel;
  result->auth_credentials.swap(*auth_credentials);
  return result;
}

void MergeCancelOfResponses(
    const EventResponseDeltas& deltas,
    bool* canceled,
    const net::BoundNetLog* net_log) {
  for (EventResponseDeltas::const_iterator i = deltas.begin();
       i != deltas.end(); ++i) {
    if ((*i)->cancel) {
      *canceled = true;
      net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_ABORTED_REQUEST,
          CreateNetLogExtensionIdCallback(i->get()));
      break;
    }
  }
}

// Helper function for MergeOnBeforeRequestResponses() that allows ignoring
// all redirects but those to data:// urls and about:blank. This is important
// to treat these URLs as "cancel urls", i.e. URLs that extensions redirect
// to if they want to express that they want to cancel a request. This reduces
// the number of conflicts that we need to flag, as canceling is considered
// a higher precedence operation that redirects.
// Returns whether a redirect occurred.
static bool MergeOnBeforeRequestResponsesHelper(
    const EventResponseDeltas& deltas,
    GURL* new_url,
    extensions::ExtensionWarningSet* conflicting_extensions,
    const net::BoundNetLog* net_log,
    bool consider_only_cancel_scheme_urls) {
  bool redirected = false;

  // Extension that determines the |new_url|.
  std::string winning_extension_id;
  EventResponseDeltas::const_iterator delta;
  for (delta = deltas.begin(); delta != deltas.end(); ++delta) {
    if ((*delta)->new_url.is_empty())
      continue;
    if (consider_only_cancel_scheme_urls &&
        !(*delta)->new_url.SchemeIs(chrome::kDataScheme) &&
        (*delta)->new_url.spec() != "about:blank") {
      continue;
    }

    if (!redirected || *new_url == (*delta)->new_url) {
      *new_url = (*delta)->new_url;
      winning_extension_id = (*delta)->extension_id;
      redirected = true;
      net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_REDIRECTED_REQUEST,
          CreateNetLogExtensionIdCallback(delta->get()));
    } else {
      conflicting_extensions->insert(
          ExtensionWarning::CreateRedirectConflictWarning(
              (*delta)->extension_id,
              winning_extension_id,
              (*delta)->new_url,
              *new_url));
      net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
          CreateNetLogExtensionIdCallback(delta->get()));
    }
  }
  return redirected;
}

void MergeOnBeforeRequestResponses(
    const EventResponseDeltas& deltas,
    GURL* new_url,
    extensions::ExtensionWarningSet* conflicting_extensions,
    const net::BoundNetLog* net_log) {

  // First handle only redirects to data:// URLs and about:blank. These are a
  // special case as they represent a way of cancelling a request.
  if (MergeOnBeforeRequestResponsesHelper(
          deltas, new_url, conflicting_extensions, net_log, true)) {
    // If any extension cancelled a request by redirecting to a data:// URL or
    // about:blank, we don't consider the other redirects.
    return;
  }

  // Handle all other redirects.
  MergeOnBeforeRequestResponsesHelper(
      deltas, new_url, conflicting_extensions, net_log, false);
}

// Assumes that |header_value| is the cookie header value of a HTTP Request
// following the cookie-string schema of RFC 6265, section 4.2.1, and returns
// cookie name/value pairs. If cookie values are presented in double quotes,
// these will appear in |parsed| as well. We can assume that the cookie header
// is written by Chromium and therefore, well-formed.
static void ParseRequestCookieLine(
    const std::string& header_value,
    ParsedRequestCookies* parsed_cookies) {
  std::string::const_iterator i = header_value.begin();
  while (i != header_value.end()) {
    // Here we are at the beginning of a cookie.

    // Eat whitespace.
    while (i != header_value.end() && *i == ' ') ++i;
    if (i == header_value.end()) return;

    // Find cookie name.
    std::string::const_iterator cookie_name_beginning = i;
    while (i != header_value.end() && *i != '=') ++i;
    base::StringPiece cookie_name(cookie_name_beginning, i);

    // Find cookie value.
    base::StringPiece cookie_value;
    if (i != header_value.end()) {  // Cookies may have no value.
      ++i;  // Skip '='.
      std::string::const_iterator cookie_value_beginning = i;
      if (*i == '"') {
        while (i != header_value.end() && *i != '"') ++i;
        if (i == header_value.end()) return;
        ++i;  // Skip '"'.
        cookie_value = base::StringPiece(cookie_value_beginning, i);
        // i points to character after '"', potentially a ';'
      } else {
        while (i != header_value.end() && *i != ';') ++i;
        cookie_value = base::StringPiece(cookie_value_beginning, i);
        // i points to ';' or end of string.
      }
    }
    parsed_cookies->push_back(make_pair(cookie_name, cookie_value));
    // Eat ';'
    if (i != header_value.end()) ++i;
  }
}

// Writes all cookies of |parsed_cookies| into a HTTP Request header value
// that belongs to the "Cookie" header.
static std::string SerializeRequestCookieLine(
    const ParsedRequestCookies& parsed_cookies) {
  std::string buffer;
  for (ParsedRequestCookies::const_iterator i = parsed_cookies.begin();
       i != parsed_cookies.end(); ++i) {
    if (!buffer.empty())
      buffer += "; ";
    buffer += i->first.as_string();
    if (!i->second.empty())
      buffer += "=" + i->second.as_string();
  }
  return buffer;
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
    extensions::ExtensionWarningSet* conflicting_extensions,
    const net::BoundNetLog* net_log) {
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
  ParseRequestCookieLine(cookie_header, &cookies);

  // Modify cookies.
  bool modified = false;
  modified |= MergeAddRequestCookieModifications(deltas, &cookies);
  modified |= MergeEditRequestCookieModifications(deltas, &cookies);
  modified |= MergeRemoveRequestCookieModifications(deltas, &cookies);

  // Reassemble and store new cookie line.
  if (modified) {
    std::string new_cookie_header = SerializeRequestCookieLine(cookies);
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
      if (key == modification.name() && value == modification.value())
        return (*delta)->extension_id;
    }
  }
  return "";
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
      if (*i == key)
        return (*delta)->extension_id;
    }
  }
  return "";
}

void MergeOnBeforeSendHeadersResponses(
    const EventResponseDeltas& deltas,
    net::HttpRequestHeaders* request_headers,
    extensions::ExtensionWarningSet* conflicting_extensions,
    const net::BoundNetLog* net_log) {
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
      net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_MODIFIED_HEADERS,
          base::Bind(&NetLogModificationCallback, delta->get()));
    } else {
      conflicting_extensions->insert(
          ExtensionWarning::CreateRequestHeaderConflictWarning(
              (*delta)->extension_id, winning_extension_id,
              conflicting_header));
      net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
          CreateNetLogExtensionIdCallback(delta->get()));
    }
  }

  MergeCookiesInOnBeforeSendHeadersResponses(deltas, request_headers,
      conflicting_extensions, net_log);
}

// Retrives all cookies from |override_response_headers|.
static ParsedResponseCookies GetResponseCookies(
    scoped_refptr<net::HttpResponseHeaders> override_response_headers) {
  ParsedResponseCookies result;

  void* iter = NULL;
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
  if (filter->name.get() && cookie->Name() != *filter->name) return false;
  if (filter->value.get() && cookie->Value() != *filter->value) return false;
  if (filter->expires.get()) {
    std::string actual_value = cookie->HasExpires() ? cookie->Expires() : "";
    if (actual_value != *filter->expires)
      return false;
  }
  if (filter->max_age.get()) {
    std::string actual_value = cookie->HasMaxAge() ? cookie->MaxAge() : "";
    if (actual_value != base::IntToString(*filter->max_age))
      return false;
  }
  if (filter->domain.get()) {
    std::string actual_value = cookie->HasDomain() ? cookie->Domain() : "";
    if (actual_value != *filter->domain)
      return false;
  }
  if (filter->path.get()) {
    std::string actual_value = cookie->HasPath() ? cookie->Path() : "";
    if (actual_value != *filter->path)
      return false;
  }
  if (filter->secure.get() && cookie->IsSecure() != *filter->secure)
    return false;
  if (filter->http_only.get() && cookie->IsHttpOnly() != *filter->http_only)
    return false;
  int64 seconds_till_expiry;
  bool lifetime_parsed = false;
  if (filter->age_upper_bound.get() ||
      filter->age_lower_bound.get() ||
      (filter->session_cookie.get() && *filter->session_cookie)) {
    lifetime_parsed = ParseCookieLifetime(cookie, &seconds_till_expiry);
  }
  if (filter->age_upper_bound.get()) {
    if (seconds_till_expiry > *filter->age_upper_bound)
      return false;
  }
  if (filter->age_lower_bound.get()) {
    if (seconds_till_expiry < *filter->age_lower_bound)
      return false;
  }
  if (filter->session_cookie.get() &&
      *filter->session_cookie &&
      lifetime_parsed) {
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
      linked_ptr<net::ParsedCookie> cookie(new net::ParsedCookie(""));
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
    extensions::ExtensionWarningSet* conflicting_extensions,
    const net::BoundNetLog* net_log) {
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
  std::string lower_key(header.first);
  StringToLowerASCII(&lower_key);
  return ResponseHeader(lower_key, header.second);
}

// Returns the extension ID of the first extension in |deltas| that removes the
// request header identified by |key|.
static std::string FindRemoveResponseHeader(
    const EventResponseDeltas& deltas,
    const std::string& key) {
  std::string lower_key = StringToLowerASCII(key);
  EventResponseDeltas::const_iterator delta;
  for (delta = deltas.begin(); delta != deltas.end(); ++delta) {
    ResponseHeaders::const_iterator i;
    for (i = (*delta)->deleted_response_headers.begin();
         i != (*delta)->deleted_response_headers.end(); ++i) {
      if (StringToLowerASCII(i->first) == lower_key)
        return (*delta)->extension_id;
    }
  }
  return "";
}

void MergeOnHeadersReceivedResponses(
    const EventResponseDeltas& deltas,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    extensions::ExtensionWarningSet* conflicting_extensions,
    const net::BoundNetLog* net_log) {
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
      net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_MODIFIED_HEADERS,
          CreateNetLogExtensionIdCallback(delta->get()));
    } else {
      conflicting_extensions->insert(
          ExtensionWarning::CreateResponseHeaderConflictWarning(
              (*delta)->extension_id, winning_extension_id,
              conflicting_header));
      net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
          CreateNetLogExtensionIdCallback(delta->get()));
    }
  }

  MergeCookiesInOnHeadersReceivedResponses(deltas, original_response_headers,
      override_response_headers, conflicting_extensions, net_log);
}

bool MergeOnAuthRequiredResponses(
    const EventResponseDeltas& deltas,
    net::AuthCredentials* auth_credentials,
    extensions::ExtensionWarningSet* conflicting_extensions,
    const net::BoundNetLog* net_log) {
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
          ExtensionWarning::CreateCredentialsConflictWarning(
              (*delta)->extension_id, winning_extension_id));
      net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
          CreateNetLogExtensionIdCallback(delta->get()));
    } else {
      net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_PROVIDE_AUTH_CREDENTIALS,
          CreateNetLogExtensionIdCallback(delta->get()));
      *auth_credentials = *(*delta)->auth_credentials;
      credentials_set = true;
      winning_extension_id = (*delta)->extension_id;
    }
  }
  return credentials_set;
}


#define ARRAYEND(array) (array + arraysize(array))

bool IsRelevantResourceType(ResourceType::Type type) {
  ResourceType::Type* iter =
      std::find(kResourceTypeValues, ARRAYEND(kResourceTypeValues), type);
  return iter != ARRAYEND(kResourceTypeValues);
}

const char* ResourceTypeToString(ResourceType::Type type) {
  ResourceType::Type* iter =
      std::find(kResourceTypeValues, ARRAYEND(kResourceTypeValues), type);
  if (iter == ARRAYEND(kResourceTypeValues))
    return "other";

  return kResourceTypeStrings[iter - kResourceTypeValues];
}

bool ParseResourceType(const std::string& type_str,
                       ResourceType::Type* type) {
  const char** iter =
      std::find(kResourceTypeStrings, ARRAYEND(kResourceTypeStrings), type_str);
  if (iter == ARRAYEND(kResourceTypeStrings))
    return false;
  *type = kResourceTypeValues[iter - kResourceTypeStrings];
  return true;
}

void ClearCacheOnNavigation() {
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    ClearCacheOnNavigationOnUI();
  } else {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::Bind(&ClearCacheOnNavigationOnUI));
  }
}

}  // namespace extension_web_request_api_helpers
