// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/previews_state_helper.h"

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

namespace {

// Chrome Proxy Previews header and directives.
const char kChromeProxyHeader[] = "chrome-proxy";
const char kChromeProxyContentTransformHeader[] =
    "chrome-proxy-content-transform";
const char kChromeProxyPagePoliciesDirective[] = "page-policies";
const char kChromeProxyEmptyImageDirective[] = "empty-image";
const char kChromeProxyLitePageDirective[] = "lite-page";

bool HasEmptyImageDirective(const blink::WebURLResponse& web_url_response) {
  std::string chrome_proxy_value =
      web_url_response
          .HttpHeaderField(blink::WebString::FromUTF8(kChromeProxyHeader))
          .Utf8();
  for (const auto& directive :
       base::SplitStringPiece(chrome_proxy_value, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    if (!base::StartsWith(directive, kChromeProxyPagePoliciesDirective,
                          base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }

    // Check policy directive for empty-image entry.
    base::StringPiece page_policies_value = base::StringPiece(directive).substr(
        arraysize(kChromeProxyPagePoliciesDirective));
    for (const auto& policy :
         base::SplitStringPiece(page_policies_value, "|", base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY)) {
      if (base::LowerCaseEqualsASCII(policy, kChromeProxyEmptyImageDirective)) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

PreviewsState GetPreviewsStateFromMainFrameResponse(
    PreviewsState original_state,
    const blink::WebURLResponse& web_url_response) {
  if (original_state == PREVIEWS_UNSPECIFIED) {
    return PREVIEWS_OFF;
  }

  // Don't update the state if server previews were not enabled.
  if (!(original_state & SERVER_LITE_PAGE_ON)) {
    return original_state;
  }

  // At this point, this is a proxy main frame response for which the
  // PreviewsState needs to be updated from what was enabled/accepted by the
  // client to what the client should actually do based on the server response.

  PreviewsState updated_state = original_state;

  // Clear the Lite Page bit if Lite Page transformation did not occur.
  if (web_url_response
          .HttpHeaderField(
              blink::WebString::FromUTF8(kChromeProxyContentTransformHeader))
          .Utf8() != kChromeProxyLitePageDirective) {
    updated_state &= ~(SERVER_LITE_PAGE_ON);
  }

  // Determine whether to keep or clear Lo-Fi bits. We need to receive the
  // empty-image policy directive and have SERVER_LOFI_ON in order to retain
  // Lo-Fi bits.
  if (!(original_state & SERVER_LOFI_ON)) {
    // Server Lo-Fi not enabled so ensure Client Lo-Fi off for this request.
    updated_state &= ~(CLIENT_LOFI_ON);
  } else if (!HasEmptyImageDirective(web_url_response)) {
    updated_state &= ~(SERVER_LOFI_ON | CLIENT_LOFI_ON);
  }

  // If we are left with no previews bits set, return the off state.
  if (updated_state == PREVIEWS_UNSPECIFIED) {
    return PREVIEWS_OFF;
  }

  return updated_state;
}

}  // namespace content
