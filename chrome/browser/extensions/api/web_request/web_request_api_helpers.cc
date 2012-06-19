// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_request/web_request_api_helpers.h"

#include "base/bind.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/common/url_constants.h"
#include "net/base/net_log.h"
#include "net/http/http_util.h"

namespace extension_web_request_api_helpers {

namespace {

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

}  // namespace


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

bool CharListToString(ListValue* list, std::string* out) {
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
    net::HttpResponseHeaders* old_response_headers,
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
    std::set<std::string>* conflicting_extensions,
    const net::BoundNetLog* net_log,
    bool consider_only_cancel_scheme_urls) {
  bool redirected = false;

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
      redirected = true;
      net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_REDIRECTED_REQUEST,
          CreateNetLogExtensionIdCallback(delta->get()));
    } else {
      conflicting_extensions->insert((*delta)->extension_id);
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
    std::set<std::string>* conflicting_extensions,
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

void MergeOnBeforeSendHeadersResponses(
    const EventResponseDeltas& deltas,
    net::HttpRequestHeaders* request_headers,
    std::set<std::string>* conflicting_extensions,
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
    {
      net::HttpRequestHeaders::Iterator modification(
          (*delta)->modified_request_headers);
      while (modification.GetNext() && !extension_conflicts) {
        // This modification sets |key| to |value|.
        const std::string& key = modification.name();
        const std::string& value = modification.value();

        // We must not delete anything that has been modified before.
        if (removed_headers.find(key) != removed_headers.end())
          extension_conflicts = true;

        // We must not modify anything that has been set to a *different*
        // value before.
        if (set_headers.find(key) != set_headers.end()) {
          std::string current_value;
          if (!request_headers->GetHeader(key, &current_value) ||
              current_value != value) {
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
        if (set_headers.find(*key) != set_headers.end())
          extension_conflicts = true;
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
      conflicting_extensions->insert((*delta)->extension_id);
      net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
          CreateNetLogExtensionIdCallback(delta->get()));
    }
  }
}

// Converts the key of the (key, value) pair to lower case.
static ResponseHeader ToLowerCase(const ResponseHeader& header) {
  std::string lower_key(header.first);
  StringToLowerASCII(&lower_key);
  return ResponseHeader(lower_key, header.second);
}

void MergeOnHeadersReceivedResponses(
    const EventResponseDeltas& deltas,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    std::set<std::string>* conflicting_extensions,
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
    ResponseHeaders::const_iterator i;
    for (i = (*delta)->deleted_response_headers.begin();
         i != (*delta)->deleted_response_headers.end(); ++i) {
      if (removed_headers.find(ToLowerCase(*i)) != removed_headers.end()) {
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
          (*override_response_headers)->RemoveHeaderWithValue(i->first,
                                                              i->second);
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
      conflicting_extensions->insert((*delta)->extension_id);
      net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
          CreateNetLogExtensionIdCallback(delta->get()));
    }
  }
}

bool MergeOnAuthRequiredResponses(
    const EventResponseDeltas& deltas,
    net::AuthCredentials* auth_credentials,
    std::set<std::string>* conflicting_extensions,
    const net::BoundNetLog* net_log) {
  CHECK(auth_credentials);
  bool credentials_set = false;

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
      conflicting_extensions->insert((*delta)->extension_id);
      net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
          CreateNetLogExtensionIdCallback(delta->get()));
    } else {
      net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_PROVIDE_AUTH_CREDENTIALS,
          CreateNetLogExtensionIdCallback(delta->get()));
      *auth_credentials = *(*delta)->auth_credentials;
      credentials_set = true;
    }
  }
  return credentials_set;
}

namespace {

// Returns true if the URL is sensitive and requests to this URL must not be
// modified/canceled by extensions, e.g. because it is targeted to the webstore
// to check for updates, extension blacklisting, etc.
bool IsSensitiveURL(const GURL& url) {
  bool is_webstore_gallery_url =
      StartsWithASCII(url.spec(), extension_urls::kGalleryBrowsePrefix, true);
  bool sensitive_chrome_url = false;
  if (EndsWith(url.host(), "google.com", true)) {
    sensitive_chrome_url |= (url.host() == "www.google.com") &&
                            StartsWithASCII(url.path(), "/chrome", true);
    sensitive_chrome_url |= (url.host() == "chrome.google.com");
    if (StartsWithASCII(url.host(), "client", true)) {
      for (int i = 0; i < 10; ++i) {
        sensitive_chrome_url |=
            (StringPrintf("client%d.google.com", i) == url.host());
      }
    }
  }
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  GURL url_without_query = url.ReplaceComponents(replacements);
  return is_webstore_gallery_url || sensitive_chrome_url ||
      extension_urls::IsWebstoreUpdateUrl(url_without_query) ||
      extension_urls::IsBlacklistUpdateUrl(url);
}

// Returns true if the scheme is one we want to allow extensions to have access
// to. Extensions still need specific permissions for a given URL, which is
// covered by CanExtensionAccessURL.
bool HasWebRequestScheme(const GURL& url) {
  return (url.SchemeIs(chrome::kAboutScheme) ||
          url.SchemeIs(chrome::kFileScheme) ||
          url.SchemeIs(chrome::kFileSystemScheme) ||
          url.SchemeIs(chrome::kFtpScheme) ||
          url.SchemeIs(chrome::kHttpScheme) ||
          url.SchemeIs(chrome::kHttpsScheme) ||
          url.SchemeIs(chrome::kExtensionScheme));
}

}  // namespace

bool HideRequestForURL(const GURL& url) {
  return IsSensitiveURL(url) || !HasWebRequestScheme(url);
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

}  // namespace extension_web_request_api_helpers
