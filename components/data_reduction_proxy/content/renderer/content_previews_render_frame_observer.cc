// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/renderer/content_previews_render_frame_observer.h"

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocumentLoader.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace data_reduction_proxy {

namespace {

bool HasEmptyImageDirective(const blink::WebURLResponse& web_url_response) {
  std::string chrome_proxy_value =
      web_url_response
          .HttpHeaderField(blink::WebString::FromUTF8(chrome_proxy_header()))
          .Utf8();
  for (const auto& directive :
       base::SplitStringPiece(chrome_proxy_value, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    if (!base::StartsWith(directive, page_policies_directive(),
                          base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }

    // Check policy directive for empty-image entry.
    base::StringPiece page_policies_value = base::StringPiece(directive).substr(
        strlen(page_policies_directive()) + 1);
    for (const auto& policy :
         base::SplitStringPiece(page_policies_value, "|", base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY)) {
      if (base::LowerCaseEqualsASCII(policy, empty_image_directive())) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

ContentPreviewsRenderFrameObserver::ContentPreviewsRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

ContentPreviewsRenderFrameObserver::~ContentPreviewsRenderFrameObserver() =
    default;

content::PreviewsState
ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
    content::PreviewsState original_state,
    const blink::WebURLResponse& web_url_response) {
  if (original_state == content::PREVIEWS_UNSPECIFIED) {
    return content::PREVIEWS_OFF;
  }

  // Don't update the state if server previews were not enabled.
  if (!(original_state & content::SERVER_LITE_PAGE_ON)) {
    return original_state;
  }

  // At this point, this is a proxy main frame response for which the
  // PreviewsState needs to be updated from what was enabled/accepted by the
  // client to what the client should actually do based on the server response.

  content::PreviewsState updated_state = original_state;

  // Clear the Lite Page bit if Lite Page transformation did not occur.
  // TODO(megjablon): Leverage common code in drp_headers.
  if (web_url_response
          .HttpHeaderField(blink::WebString::FromUTF8(
              chrome_proxy_content_transform_header()))
          .Utf8() != lite_page_directive()) {
    updated_state &= ~(content::SERVER_LITE_PAGE_ON);
  }

  // Determine whether to keep or clear Lo-Fi bits. We need to receive the
  // empty-image policy directive and have SERVER_LOFI_ON in order to retain
  // Lo-Fi bits.
  if (!(original_state & content::SERVER_LOFI_ON)) {
    // Server Lo-Fi not enabled so ensure Client Lo-Fi off for this request.
    updated_state &= ~(content::CLIENT_LOFI_ON);
  } else if (!HasEmptyImageDirective(web_url_response)) {
    updated_state &= ~(content::SERVER_LOFI_ON | content::CLIENT_LOFI_ON);
  }

  // If we are left with no previews bits set, return the off state.
  if (updated_state == content::PREVIEWS_UNSPECIFIED) {
    return content::PREVIEWS_OFF;
  }

  return updated_state;
}

void ContentPreviewsRenderFrameObserver::OnDestruct() {
  delete this;
}

void ContentPreviewsRenderFrameObserver::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_document_navigation) {
  if (is_same_document_navigation)
    return;

  content::PreviewsState original_state = render_frame()->GetPreviewsState();
  const blink::WebURLResponse& web_url_response =
      render_frame()->GetWebFrame()->GetDocumentLoader()->GetResponse();

  render_frame()->SetPreviewsState(
      GetPreviewsStateFromResponse(original_state, web_url_response));
}

}  // namespace data_reduction_proxy
