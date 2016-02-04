// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_hit_test_data.h"
#include "android_webview/common/render_view_messages.h"
#include "android_webview/renderer/aw_render_frame_ext.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/android_content_detection_prefixes.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebElementCollection.h"
#include "third_party/WebKit/public/web/WebHitTestResult.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebMeaningfulLayout.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/url_canon.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace android_webview {

namespace {

GURL GetAbsoluteUrl(const blink::WebNode& node,
                    const base::string16& url_fragment) {
  return GURL(node.document().completeURL(url_fragment));
}

base::string16 GetHref(const blink::WebElement& element) {
  // Get the actual 'href' attribute, which might relative if valid or can
  // possibly contain garbage otherwise, so not using absoluteLinkURL here.
  return element.getAttribute("href");
}

GURL GetAbsoluteSrcUrl(const blink::WebElement& element) {
  if (element.isNull())
    return GURL();
  return GetAbsoluteUrl(element, element.getAttribute("src"));
}

blink::WebElement GetImgChild(const blink::WebNode& node) {
  // This implementation is incomplete (for example if is an area tag) but
  // matches the original WebViewClassic implementation.

  blink::WebElementCollection collection = node.getElementsByHTMLTagName("img");
  DCHECK(!collection.isNull());
  return collection.firstItem();
}

GURL GetChildImageUrlFromElement(const blink::WebElement& element) {
  const blink::WebElement child_img = GetImgChild(element);
  if (child_img.isNull())
    return GURL();
  return GetAbsoluteSrcUrl(child_img);
}

bool RemovePrefixAndAssignIfMatches(const base::StringPiece& prefix,
                                    const GURL& url,
                                    std::string* dest) {
  const base::StringPiece spec(url.possibly_invalid_spec());

  if (spec.starts_with(prefix)) {
    url::RawCanonOutputW<1024> output;
    url::DecodeURLEscapeSequences(spec.data() + prefix.length(),
                                  spec.length() - prefix.length(), &output);
    *dest =
        base::UTF16ToUTF8(base::StringPiece16(output.data(), output.length()));
    return true;
  }
  return false;
}

void DistinguishAndAssignSrcLinkType(const GURL& url, AwHitTestData* data) {
  if (RemovePrefixAndAssignIfMatches(content::kAddressPrefix, url,
                                     &data->extra_data_for_type)) {
    data->type = AwHitTestData::GEO_TYPE;
  } else if (RemovePrefixAndAssignIfMatches(content::kPhoneNumberPrefix, url,
                                            &data->extra_data_for_type)) {
    data->type = AwHitTestData::PHONE_TYPE;
  } else if (RemovePrefixAndAssignIfMatches(content::kEmailPrefix, url,
                                            &data->extra_data_for_type)) {
    data->type = AwHitTestData::EMAIL_TYPE;
  } else {
    data->type = AwHitTestData::SRC_LINK_TYPE;
    data->extra_data_for_type = url.possibly_invalid_spec();
    if (!data->extra_data_for_type.empty())
      data->href = base::UTF8ToUTF16(data->extra_data_for_type);
  }
}

void PopulateHitTestData(const GURL& absolute_link_url,
                         const GURL& absolute_image_url,
                         bool is_editable,
                         AwHitTestData* data) {
  // Note: Using GURL::is_empty instead of GURL:is_valid due to the
  // WebViewClassic allowing any kind of protocol which GURL::is_valid
  // disallows. Similar reasons for using GURL::possibly_invalid_spec instead of
  // GURL::spec.
  if (!absolute_image_url.is_empty())
    data->img_src = absolute_image_url;

  const bool is_javascript_scheme =
      absolute_link_url.SchemeIs(url::kJavaScriptScheme);
  const bool has_link_url = !absolute_link_url.is_empty();
  const bool has_image_url = !absolute_image_url.is_empty();

  if (has_link_url && !has_image_url && !is_javascript_scheme) {
    DistinguishAndAssignSrcLinkType(absolute_link_url, data);
  } else if (has_link_url && has_image_url && !is_javascript_scheme) {
    data->type = AwHitTestData::SRC_IMAGE_LINK_TYPE;
    data->extra_data_for_type = data->img_src.possibly_invalid_spec();
    if (absolute_link_url.is_valid())
      data->href = base::UTF8ToUTF16(absolute_link_url.possibly_invalid_spec());
  } else if (!has_link_url && has_image_url) {
    data->type = AwHitTestData::IMAGE_TYPE;
    data->extra_data_for_type = data->img_src.possibly_invalid_spec();
  } else if (is_editable) {
    data->type = AwHitTestData::EDIT_TEXT_TYPE;
    DCHECK_EQ(0u, data->extra_data_for_type.length());
  }
}

}  // namespace

AwRenderFrameExt::AwRenderFrameExt(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
}

AwRenderFrameExt::~AwRenderFrameExt() {
}

void AwRenderFrameExt::DidCommitProvisionalLoad(bool is_new_navigation,
                                                bool is_same_page_navigation) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  content::DocumentState* document_state =
      content::DocumentState::FromDataSource(frame->dataSource());
  if (document_state->can_load_local_resources()) {
    blink::WebSecurityOrigin origin = frame->document().securityOrigin();
    origin.grantLoadLocalResources();
  }
}

bool AwRenderFrameExt::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AwRenderFrameExt, message)
    IPC_MESSAGE_HANDLER(AwViewMsg_DocumentHasImages, OnDocumentHasImagesRequest)
    IPC_MESSAGE_HANDLER(AwViewMsg_DoHitTest, OnDoHitTest)
    IPC_MESSAGE_HANDLER(AwViewMsg_SetTextZoomFactor, OnSetTextZoomFactor)
    IPC_MESSAGE_HANDLER(AwViewMsg_ResetScrollAndScaleState,
                        OnResetScrollAndScaleState)
    IPC_MESSAGE_HANDLER(AwViewMsg_SetInitialPageScale, OnSetInitialPageScale)
    IPC_MESSAGE_HANDLER(AwViewMsg_SetBackgroundColor, OnSetBackgroundColor)
    IPC_MESSAGE_HANDLER(AwViewMsg_SmoothScroll, OnSmoothScroll)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AwRenderFrameExt::OnDocumentHasImagesRequest(int id) {
  bool hasImages = false;
  blink::WebView* webview = GetWebView();
  if (webview) {
    blink::WebDocument document = webview->mainFrame()->document();
    const blink::WebElement child_img = GetImgChild(document);
    hasImages = !child_img.isNull();
  }
  Send(
      new AwViewHostMsg_DocumentHasImagesResponse(routing_id(), id, hasImages));
}

void AwRenderFrameExt::FocusedNodeChanged(const blink::WebNode& node) {
  if (node.isNull() || !node.isElementNode() || !render_frame() ||
      !render_frame()->GetRenderView())
    return;

  const blink::WebElement element = node.toConst<blink::WebElement>();
  AwHitTestData data;

  data.href = GetHref(element);
  data.anchor_text = element.textContent();

  GURL absolute_link_url;
  if (node.isLink())
    absolute_link_url = GetAbsoluteUrl(node, data.href);

  GURL absolute_image_url = GetChildImageUrlFromElement(element);

  PopulateHitTestData(absolute_link_url, absolute_image_url,
                      element.isEditable(), &data);
  Send(new AwViewHostMsg_UpdateHitTestData(routing_id(), data));
}

// Only main frame needs to *receive* the hit test request, because all we need
// is to get the blink::webView object and invoke a the hitTestResultForTap API
// from it.
void AwRenderFrameExt::OnDoHitTest(const gfx::PointF& touch_center,
                                   const gfx::SizeF& touch_area) {
  blink::WebView* webview = GetWebView();
  if (!webview)
    return;

  const blink::WebHitTestResult result = webview->hitTestResultForTap(
      blink::WebPoint(touch_center.x(), touch_center.y()),
      blink::WebSize(touch_area.width(), touch_area.height()));
  AwHitTestData data;

  GURL absolute_image_url = result.absoluteImageURL();
  if (!result.urlElement().isNull()) {
    data.anchor_text = result.urlElement().textContent();
    data.href = GetHref(result.urlElement());
    // If we hit an image that failed to load, Blink won't give us its URL.
    // Fall back to walking the DOM in this case.
    if (absolute_image_url.is_empty())
      absolute_image_url = GetChildImageUrlFromElement(result.urlElement());
  }

  PopulateHitTestData(result.absoluteLinkURL(), absolute_image_url,
                      result.isContentEditable(), &data);
  Send(new AwViewHostMsg_UpdateHitTestData(routing_id(), data));
}

void AwRenderFrameExt::OnSetTextZoomFactor(float zoom_factor) {
  blink::WebView* webview = GetWebView();
  if (!webview)
    return;

  // Hide selection and autofill popups.
  webview->hidePopups();
  webview->setTextZoomFactor(zoom_factor);
}

void AwRenderFrameExt::OnResetScrollAndScaleState() {
  blink::WebView* webview = GetWebView();
  if (!webview)
    return;

  webview->resetScrollAndScaleState();
}

void AwRenderFrameExt::OnSetInitialPageScale(double page_scale_factor) {
  blink::WebView* webview = GetWebView();
  if (!webview)
    return;

  webview->setInitialPageScaleOverride(page_scale_factor);
}

void AwRenderFrameExt::OnSetBackgroundColor(SkColor c) {
  blink::WebView* webview = GetWebView();
  if (!webview)
    return;

  webview->setBaseBackgroundColor(c);
}

void AwRenderFrameExt::OnSmoothScroll(int target_x,
                                      int target_y,
                                      long duration_ms) {
  blink::WebView* webview = GetWebView();
  if (!webview)
    return;

  webview->smoothScroll(target_x, target_y, duration_ms);
}

blink::WebView* AwRenderFrameExt::GetWebView() {
  if (!render_frame() || !render_frame()->GetRenderView() ||
      !render_frame()->GetRenderView()->GetWebView())
    return nullptr;

  return render_frame()->GetRenderView()->GetWebView();
}

}  // namespace android_webview
