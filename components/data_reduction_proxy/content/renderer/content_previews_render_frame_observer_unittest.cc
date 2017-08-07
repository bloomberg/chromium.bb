// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/renderer/content_previews_render_frame_observer.h"

#include "content/public/common/previews_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"

namespace data_reduction_proxy {

TEST(ContentPreviewsRenderFrameObserverTest,
     GetPreviewsStateNoServerTransformRequested) {
  blink::WebURLResponse response_no_headers;

  // No transforms specified.
  EXPECT_EQ(content::PREVIEWS_OFF,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::PREVIEWS_UNSPECIFIED, response_no_headers));

  // Client Lo-Fi preserved if no Server Lo-Fi nor Lite Page.
  EXPECT_EQ(content::CLIENT_LOFI_ON,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::CLIENT_LOFI_ON, response_no_headers));
}

TEST(ContentPreviewsRenderFrameObserverTest,
     GetPreviewsStateNoTransformResponseHeaders) {
  blink::WebURLResponse response_no_headers;
  // Lite Page enabled but no CPCT nor page-polices => all cleared.
  EXPECT_EQ(content::PREVIEWS_OFF,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                    content::CLIENT_LOFI_ON,
                response_no_headers));
  EXPECT_EQ(content::PREVIEWS_OFF,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON, response_no_headers));
  EXPECT_EQ(content::PREVIEWS_OFF,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON | content::CLIENT_LOFI_ON,
                response_no_headers));
  EXPECT_EQ(content::PREVIEWS_OFF,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON,
                response_no_headers));

  // Legacy Lo-Fi path (Lite Pages not enabled but Server Lo-Fi is).
  EXPECT_EQ(content::SERVER_LOFI_ON,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LOFI_ON, response_no_headers));
  EXPECT_EQ(content::SERVER_LOFI_ON | content::CLIENT_LOFI_ON,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LOFI_ON | content::CLIENT_LOFI_ON,
                response_no_headers));
}

TEST(ContentPreviewsRenderFrameObserverTest,
     GetPreviewsStateLitePageTransform) {
  blink::WebURLResponse response_with_lite_page;
  response_with_lite_page.AddHTTPHeaderField("chrome-proxy-content-transform",
                                             "lite-page");
  EXPECT_EQ(content::SERVER_LITE_PAGE_ON,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON, response_with_lite_page));
  EXPECT_EQ(content::SERVER_LITE_PAGE_ON,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                    content::CLIENT_LOFI_ON,
                response_with_lite_page));
  EXPECT_EQ(content::SERVER_LITE_PAGE_ON,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON | content::CLIENT_LOFI_ON,
                response_with_lite_page));
  EXPECT_EQ(content::SERVER_LITE_PAGE_ON,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON,
                response_with_lite_page));
}

TEST(ContentPreviewsRenderFrameObserverTest,
     GetPreviewsStateLitePageAndLoFiTransforms) {
  blink::WebURLResponse response_with_lite_page_and_page_policy;
  response_with_lite_page_and_page_policy.AddHTTPHeaderField(
      "chrome-proxy-content-transform", "lite-page");
  response_with_lite_page_and_page_policy.AddHTTPHeaderField(
      "Chrome-Proxy", "Page-Policies=Empty-Image");

  EXPECT_EQ(content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                content::CLIENT_LOFI_ON,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                    content::CLIENT_LOFI_ON,
                response_with_lite_page_and_page_policy));
  EXPECT_EQ(content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON,
                response_with_lite_page_and_page_policy));
}

TEST(ContentPreviewsRenderFrameObserverTest,
     GetPreviewsStateLoFiFallbackPagePolicy) {
  blink::WebURLResponse response_with_page_policy;
  response_with_page_policy.AddHTTPHeaderField("Chrome-Proxy", "SomeNoise");
  response_with_page_policy.AddHTTPHeaderField("Chrome-Proxy",
                                               "Page-Policies=Empty-Image");

  // No fallbacks if Server Lo-Fi not enabled.
  EXPECT_EQ(content::PREVIEWS_OFF,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON, response_with_page_policy));
  EXPECT_EQ(content::PREVIEWS_OFF,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON | content::CLIENT_LOFI_ON,
                response_with_page_policy));

  // Lo-Fi fallbacks if Server Lo-Fi enabled and get empty-image directive.
  EXPECT_EQ(content::SERVER_LOFI_ON | content::CLIENT_LOFI_ON,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                    content::CLIENT_LOFI_ON,
                response_with_page_policy));
  EXPECT_EQ(content::SERVER_LOFI_ON,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON,
                response_with_page_policy));
}

TEST(ContentPreviewsRenderFrameObserverTest,
     GetPreviewsStateLoFiFallbackAndOtherPolicies) {
  blink::WebURLResponse response_with_page_policy;
  response_with_page_policy.AddHTTPHeaderField("chrome-proxy", "Prefix=Noise");
  response_with_page_policy.AddHTTPHeaderField(
      "chrome-proxy", "page-policies=new-hotness|empty-image|newer-hotness");
  response_with_page_policy.AddHTTPHeaderField("chrome-proxy",
                                               "Suffix=More|Noise");

  // Lo-Fi fallbacks if Server Lo-Fi enabled and get empty-image directive.
  EXPECT_EQ(content::SERVER_LOFI_ON | content::CLIENT_LOFI_ON,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                    content::CLIENT_LOFI_ON,
                response_with_page_policy));
  EXPECT_EQ(content::SERVER_LOFI_ON,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON,
                response_with_page_policy));
}

TEST(ContentPreviewsRenderFrameObserverTest, GetPreviewsStateNoKnownPolicies) {
  blink::WebURLResponse response_with_page_policy;
  response_with_page_policy.AddHTTPHeaderField("chrome-proxy", "Prefix=Noise");
  response_with_page_policy.AddHTTPHeaderField(
      "chrome-proxy",
      "page-policies=new-hotness|new-empty-image|newer-hotness");
  response_with_page_policy.AddHTTPHeaderField("chrome-proxy",
                                               "Suffix=More|Noise");

  // Lo-Fi fallbacks if Server Lo-Fi enabled and get empty-image directive.
  EXPECT_EQ(content::PREVIEWS_OFF,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                    content::CLIENT_LOFI_ON,
                response_with_page_policy));
  EXPECT_EQ(content::PREVIEWS_OFF,
            ContentPreviewsRenderFrameObserver::GetPreviewsStateFromResponse(
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON,
                response_with_page_policy));
}

}  // namespace data_reduction_proxy
