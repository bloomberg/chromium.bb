// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/ancestor_throttle.h"

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

// static
std::unique_ptr<NavigationThrottle> AncestorThrottle::MaybeCreateThrottleFor(
    NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (handle->IsInMainFrame())
    return nullptr;

  return std::unique_ptr<NavigationThrottle>(new AncestorThrottle(handle));
}

AncestorThrottle::AncestorThrottle(NavigationHandle* handle)
    : NavigationThrottle(handle) {}

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
      return NavigationThrottle::BLOCK_RESPONSE;

    case HeaderDisposition::INVALID:
      ParseError(header_value, disposition);
      // TODO(mkwst): Consider failing here.
      return NavigationThrottle::PROCEED;

    case HeaderDisposition::DENY:
      ConsoleError(disposition);
      return NavigationThrottle::BLOCK_RESPONSE;

    case HeaderDisposition::SAMEORIGIN: {
      url::Origin current_origin(navigation_handle()->GetURL());
      url::Origin top_origin =
          handle->frame_tree_node()->frame_tree()->root()->current_origin();
      if (top_origin.IsSameOriginWith(current_origin))
        return NavigationThrottle::PROCEED;
      ConsoleError(disposition);
      return NavigationThrottle::BLOCK_RESPONSE;
    }

    case HeaderDisposition::NONE:
    case HeaderDisposition::BYPASS:
    case HeaderDisposition::ALLOWALL:
      return NavigationThrottle::PROCEED;
  }
  NOTREACHED();
  return NavigationThrottle::BLOCK_RESPONSE;
}

void AncestorThrottle::ParseError(const std::string& value,
                                  HeaderDisposition disposition) {
  DCHECK(disposition == HeaderDisposition::CONFLICT ||
         disposition == HeaderDisposition::INVALID);

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
      result != HeaderDisposition::ALLOWALL) {
    iter = 0;
    value = std::string();
    while (headers->EnumerateHeader(&iter, "content-security-policy", &value)) {
      // TODO(mkwst): 'frame-ancestors' is currently handled in Blink. We should
      // handle it here instead. Until then, don't block the request, and let
      // Blink handle it. https://crbug.com/555418
      std::vector<std::string> tokens = base::SplitString(
          value, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (std::count_if(tokens.begin(), tokens.end(), [](std::string token) {
            // The trailing " " is intentional; we'd otherwise match
            // "frame-ancestors-is-not-this-directive".
            return base::StartsWith(token, "frame-ancestors ",
                                    base::CompareCase::INSENSITIVE_ASCII);
          })) {
        return HeaderDisposition::BYPASS;
      }
    }
  }
  return result;
}

}  // namespace content
