// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper classes and functions used for the WebRequest API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_HELPERS_H_
#pragma once

#include <list>
#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/base/auth.h"
#include "net/base/net_log.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"

namespace base {
class ListValue;
class Value;
}

namespace extension_webrequest_api_helpers {

typedef std::pair<std::string, std::string> ResponseHeader;
typedef std::vector<ResponseHeader> ResponseHeaders;

// Contains the modification an extension wants to perform on an event.
struct EventResponseDelta {
  // ID of the extension that sent this response.
  std::string extension_id;

  // The time that the extension was installed. Used for deciding order of
  // precedence in case multiple extensions respond with conflicting
  // decisions.
  base::Time extension_install_time;

  // Response values. These are mutually exclusive.
  bool cancel;
  GURL new_url;

  // Newly introduced or overridden request headers.
  net::HttpRequestHeaders modified_request_headers;

  // Keys of request headers to be deleted.
  std::vector<std::string> deleted_request_headers;

  // Headers that were added to the response. A modification of a header
  // corresponds to a deletion and subsequent addition of the new header.
  ResponseHeaders added_response_headers;

  // Headers that were deleted from the response.
  ResponseHeaders deleted_response_headers;

  // Authentication Credentials to use.
  scoped_ptr<net::AuthCredentials> auth_credentials;

  EventResponseDelta(const std::string& extension_id,
                     const base::Time& extension_install_time);
  ~EventResponseDelta();

  DISALLOW_COPY_AND_ASSIGN(EventResponseDelta);
};

typedef std::list<linked_ptr<EventResponseDelta> > EventResponseDeltas;

// Container for NetLog events that shall be reported.
struct EventLogEntry {
  net::NetLog::EventType event_type;
  scoped_refptr<net::NetLog::EventParameters> params;

  EventLogEntry(net::NetLog::EventType event_type,
                const scoped_refptr<net::NetLog::EventParameters>& params);
  ~EventLogEntry();

  // Allow implicit copy and assignment.
};

typedef std::list<EventLogEntry> EventLogEntries;

// Comparison operator that returns true if the extension that caused
// |a| was installed after the extension that caused |b|.
bool InDecreasingExtensionInstallationTimeOrder(
    const linked_ptr<EventResponseDelta>& a,
    const linked_ptr<EventResponseDelta>& b);

// Converts a string to a list of integers, each in 0..255. Ownership
// of the created list is passed to the caller.
base::ListValue* StringToCharList(const std::string& s);

// Converts a list of integer values between 0 and 255 into a string |*out|.
// Returns true if the conversion was successful.
bool CharListToString(base::ListValue* list, std::string* out);

// The following functions calculate and return the modifications to requests
// commanded by extension handlers. All functions take the id of the extension
// that commanded a modification, the installation time of this extension (used
// for defining a precedence in conflicting modifications) and whether the
// extension requested to |cancel| the request. Other parameters depend on a
// the signal handler. Ownership of the returned object is passed to the caller.

EventResponseDelta* CalculateOnBeforeRequestDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    const GURL& new_url);
EventResponseDelta* CalculateOnBeforeSendHeadersDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    net::HttpRequestHeaders* old_headers,
    net::HttpRequestHeaders* new_headers);
EventResponseDelta* CalculateOnHeadersReceivedDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    net::HttpResponseHeaders* old_response_headers,
    ResponseHeaders* new_response_headers);
// Destructively moves the auth credentials from |auth_credentials| to the
// returned EventResponseDelta.
EventResponseDelta* CalculateOnAuthRequiredDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    scoped_ptr<net::AuthCredentials>* auth_credentials);

// These functions merge the responses (the |deltas|) of request handlers.
// The |deltas| need to be sorted in decreasing order of precedence of
// extensions. In case extensions had |deltas| that could not be honored, their
// IDs are reported in |conflicting_extensions|. NetLog events that shall be
// reported will be stored in |event_log_entries|.

// Stores in |canceled| whether any extension wanted to cancel the request.
void MergeCancelOfResponses(
    const EventResponseDeltas& deltas,
    bool* canceled,
    EventLogEntries* event_log_entries);
// Stores in |*new_url| the redirect request of the extension with highest
// precedence. Extensions that did not command to redirect the request are
// ignored in this logic.
void MergeOnBeforeRequestResponses(
    const EventResponseDeltas& deltas,
    GURL* new_url,
    std::set<std::string>* conflicting_extensions,
    EventLogEntries* event_log_entries);
// Modifies the headers in |request_headers| according to |deltas|. Conflicts
// are tried to be resolved.
void MergeOnBeforeSendHeadersResponses(
    const EventResponseDeltas& deltas,
    net::HttpRequestHeaders* request_headers,
    std::set<std::string>* conflicting_extensions,
    EventLogEntries* event_log_entries);
// Stores a copy of |original_response_header| into |override_response_headers|
// that is modified according to |deltas|. If |deltas| does not instruct to
// modify the response headers, |override_response_headers| remains empty.
void MergeOnHeadersReceivedResponses(
    const EventResponseDeltas& deltas,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    std::set<std::string>* conflicting_extensions,
    EventLogEntries* event_log_entries);
// Merge the responses of blocked onAuthRequired handlers. The first
// registered listener that supplies authentication credentials in a response,
// if any, will have its authentication credentials used. |request| must be
// non-NULL, and contain |deltas| that are sorted in decreasing order of
// precedence.
// Returns whether authentication credentials are set.
bool MergeOnAuthRequiredResponses(
    const EventResponseDeltas& deltas,
    net::AuthCredentials* auth_credentials,
    std::set<std::string>* conflicting_extensions,
    EventLogEntries* event_log_entries);

}  // namespace extension_webrequest_api_helpers

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_HELPERS_H_
