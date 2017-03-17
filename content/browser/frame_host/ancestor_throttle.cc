// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/ancestor_throttle.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/common/console_message_level.h"
#include "net/http/http_response_headers.h"
#include "url/origin.h"

namespace content {

namespace {
const char kXFrameOptionsSameOriginHistogram[] = "Security.XFrameOptions";

// This enum is used for UMA metrics. Keep these enums up to date with
// tools/metrics/histograms/histograms.xml.
enum XFrameOptionsHistogram {
  // A frame is loaded without any X-Frame-Options header.
  NONE = 0,

  // X-Frame-Options: DENY.
  DENY = 1,

  // X-Frame-Options: SAMEORIGIN. The navigation proceeds and every ancestor
  // has the same origin.
  SAMEORIGIN = 2,

  // X-Frame-Options: SAMEORIGIN. The navigation is blocked because the
  // top-frame doesn't have the same origin.
  SAMEORIGIN_BLOCKED = 3,

  // X-Frame-Options: SAMEORIGIN. The navigation proceeds despite the fact that
  // there is an ancestor that doesn't have the same origin.
  SAMEORIGIN_WITH_BAD_ANCESTOR_CHAIN = 4,

  // X-Frame-Options: ALLOWALL.
  ALLOWALL = 5,

  // Invalid 'X-Frame-Options' directive encountered.
  INVALID = 6,

  // The frame sets multiple 'X-Frame-Options' header with conflicting values.
  CONFLICT = 7,

  // The 'frame-ancestors' CSP directive should take effect instead.
  BYPASS = 8,

  XFRAMEOPTIONS_HISTOGRAM_MAX = BYPASS
};

void RecordXFrameOptionsUsage(XFrameOptionsHistogram usage) {
  UMA_HISTOGRAM_ENUMERATION(kXFrameOptionsSameOriginHistogram, usage,
                            XFRAMEOPTIONS_HISTOGRAM_MAX);
}

bool HeadersContainFrameAncestorsCSP(const net::HttpResponseHeaders* headers) {
  size_t iter = 0;
  std::string value;
  while (headers->EnumerateHeader(&iter, "content-security-policy", &value)) {
    // A content-security-policy is a semicolon-separated list of directives.
    for (const auto& directive : base::SplitStringPiece(
             value, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
      // The trailing " " is intentional; we'd otherwise match
      // "frame-ancestors-is-not-this-directive".
      if (base::StartsWith(directive, "frame-ancestors ",
                           base::CompareCase::INSENSITIVE_ASCII))
        return true;
    }
  }
  return false;
}

}  // namespace

// static
std::unique_ptr<NavigationThrottle> AncestorThrottle::MaybeCreateThrottleFor(
    NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (handle->IsInMainFrame())
    return nullptr;

  return std::unique_ptr<NavigationThrottle>(new AncestorThrottle(handle));
}

AncestorThrottle::~AncestorThrottle() {}

NavigationThrottle::ThrottleCheckResult
AncestorThrottle::WillProcessResponse() {
  DCHECK(!navigation_handle()->IsInMainFrame());

  NavigationHandleImpl* handle =
      static_cast<NavigationHandleImpl*>(navigation_handle());

  std::string header_value;
  HeaderDisposition disposition =
      ParseHeader(handle->GetResponseHeaders(), &header_value);

  switch (disposition) {
    case HeaderDisposition::CONFLICT:
      ParseError(header_value, disposition);
      RecordXFrameOptionsUsage(CONFLICT);
      return NavigationThrottle::BLOCK_RESPONSE;

    case HeaderDisposition::INVALID:
      ParseError(header_value, disposition);
      RecordXFrameOptionsUsage(INVALID);
      // TODO(mkwst): Consider failing here.
      return NavigationThrottle::PROCEED;

    case HeaderDisposition::DENY:
      ConsoleError(disposition);
      RecordXFrameOptionsUsage(DENY);
      return NavigationThrottle::BLOCK_RESPONSE;

    case HeaderDisposition::SAMEORIGIN: {
      url::Origin current_origin(navigation_handle()->GetURL());
      url::Origin top_origin =
          handle->frame_tree_node()->frame_tree()->root()->current_origin();

      // Block the request when the top-frame has not the same origin.
      if (!top_origin.IsSameOriginWith(current_origin)) {
        RecordXFrameOptionsUsage(SAMEORIGIN_BLOCKED);
        ConsoleError(disposition);
        RecordXFrameOptionsUsage(SAMEORIGIN_BLOCKED);
        return NavigationThrottle::BLOCK_RESPONSE;
      }

      // Do not block the request when one of the ancestor has not the same
      // origin, but count how many times it happens for statistics.
      FrameTreeNode* parent = handle->frame_tree_node()->parent();
      while (parent) {
        if (!parent->current_origin().IsSameOriginWith(current_origin)) {
          RecordXFrameOptionsUsage(SAMEORIGIN_WITH_BAD_ANCESTOR_CHAIN);
          return NavigationThrottle::PROCEED;
        }
        parent = parent->parent();
      }
      RecordXFrameOptionsUsage(SAMEORIGIN);
      return NavigationThrottle::PROCEED;
    }

    case HeaderDisposition::NONE:
      RecordXFrameOptionsUsage(NONE);
      return NavigationThrottle::PROCEED;
    case HeaderDisposition::BYPASS:
      RecordXFrameOptionsUsage(BYPASS);
      return NavigationThrottle::PROCEED;
    case HeaderDisposition::ALLOWALL:
      RecordXFrameOptionsUsage(ALLOWALL);
      return NavigationThrottle::PROCEED;
  }
  NOTREACHED();
  return NavigationThrottle::BLOCK_RESPONSE;
}

AncestorThrottle::AncestorThrottle(NavigationHandle* handle)
    : NavigationThrottle(handle) {}

void AncestorThrottle::ParseError(const std::string& value,
                                  HeaderDisposition disposition) {
  DCHECK(disposition == HeaderDisposition::CONFLICT ||
         disposition == HeaderDisposition::INVALID);
  if (!navigation_handle()->GetRenderFrameHost())
    return;  // Some responses won't have a RFH (i.e. 204/205s or downloads).

  std::string message;
  if (disposition == HeaderDisposition::CONFLICT) {
    message = base::StringPrintf(
        "Refused to display '%s' in a frame because it set multiple "
        "'X-Frame-Options' headers with conflicting values "
        "('%s'). Falling back to 'deny'.",
        navigation_handle()->GetURL().spec().c_str(), value.c_str());
  } else {
    message = base::StringPrintf(
        "Invalid 'X-Frame-Options' header encountered when loading '%s': "
        "'%s' is not a recognized directive. The header will be ignored.",
        navigation_handle()->GetURL().spec().c_str(), value.c_str());
  }

  // Log a console error in the parent of the current RenderFrameHost (as
  // the current RenderFrameHost itself doesn't yet have a document).
  navigation_handle()->GetRenderFrameHost()->GetParent()->AddMessageToConsole(
      CONSOLE_MESSAGE_LEVEL_ERROR, message);
}

void AncestorThrottle::ConsoleError(HeaderDisposition disposition) {
  DCHECK(disposition == HeaderDisposition::DENY ||
         disposition == HeaderDisposition::SAMEORIGIN);
  if (!navigation_handle()->GetRenderFrameHost())
    return;  // Some responses won't have a RFH (i.e. 204/205s or downloads).

  std::string message = base::StringPrintf(
      "Refused to display '%s' in a frame because it set 'X-Frame-Options' "
      "to '%s'.",
      navigation_handle()->GetURL().spec().c_str(),
      disposition == HeaderDisposition::DENY ? "deny" : "sameorigin");

  // Log a console error in the parent of the current RenderFrameHost (as
  // the current RenderFrameHost itself doesn't yet have a document).
  navigation_handle()->GetRenderFrameHost()->GetParent()->AddMessageToConsole(
      CONSOLE_MESSAGE_LEVEL_ERROR, message);
}

AncestorThrottle::HeaderDisposition AncestorThrottle::ParseHeader(
    const net::HttpResponseHeaders* headers,
    std::string* header_value) {
  DCHECK(header_value);
  if (!headers)
    return HeaderDisposition::NONE;

  // Process the 'X-Frame-Options header as per Section 2 of RFC7034:
  // https://tools.ietf.org/html/rfc7034#section-2
  //
  // Note that we do not support the 'ALLOW-FROM' value, and we special-case
  // the invalid "ALLOWALL" value due to its prevalance in the wild.
  HeaderDisposition result = HeaderDisposition::NONE;
  size_t iter = 0;
  std::string value;
  while (headers->EnumerateHeader(&iter, "x-frame-options", &value)) {
    HeaderDisposition current = HeaderDisposition::INVALID;

    base::StringPiece trimmed =
        base::TrimWhitespaceASCII(value, base::TRIM_ALL);
    if (!header_value->empty())
      header_value->append(", ");
    header_value->append(trimmed.as_string());

    if (base::LowerCaseEqualsASCII(trimmed, "deny"))
      current = HeaderDisposition::DENY;
    else if (base::LowerCaseEqualsASCII(trimmed, "allowall"))
      current = HeaderDisposition::ALLOWALL;
    else if (base::LowerCaseEqualsASCII(trimmed, "sameorigin"))
      current = HeaderDisposition::SAMEORIGIN;
    else
      current = HeaderDisposition::INVALID;

    if (result == HeaderDisposition::NONE)
      result = current;
    else if (result != current)
      result = HeaderDisposition::CONFLICT;
  }

  // If 'X-Frame-Options' would potentially block the response, check whether
  // the 'frame-ancestors' CSP directive should take effect instead. See
  // https://www.w3.org/TR/CSP/#frame-ancestors-and-frame-options
  if (result != HeaderDisposition::NONE &&
      result != HeaderDisposition::ALLOWALL &&
      HeadersContainFrameAncestorsCSP(headers)) {
    // TODO(mkwst): 'frame-ancestors' is currently handled in Blink. We should
    // handle it here instead. Until then, don't block the request, and let
    // Blink handle it. https://crbug.com/555418
    return HeaderDisposition::BYPASS;
  }
  return result;
}

}  // namespace content
