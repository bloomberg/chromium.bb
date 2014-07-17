// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_render_view_ext.h"

#include <string>

#include "android_webview/common/aw_hit_test_data.h"
#include "android_webview/common/render_view_messages.h"
#include "base/bind.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/public/renderer/android_content_detection_prefixes.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_view.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebElementCollection.h"
#include "third_party/WebKit/public/web/WebHitTestResult.h"
#include "third_party/WebKit/public/web/WebImageCache.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
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

blink::WebElement GetImgChild(const blink::WebElement& element) {
  // This implementation is incomplete (for example if is an area tag) but
  // matches the original WebViewClassic implementation.

  blink::WebElementCollection collection =
      element.getElementsByHTMLTagName("img");
  DCHECK(!collection.isNull());
  return collection.firstItem();
}

bool RemovePrefixAndAssignIfMatches(const base::StringPiece& prefix,
                                    const GURL& url,
                                    std::string* dest) {
  const base::StringPiece spec(url.possibly_invalid_spec());

  if (spec.starts_with(prefix)) {
    url::RawCanonOutputW<1024> output;
    url::DecodeURLEscapeSequences(spec.data() + prefix.length(),
                                  spec.length() - prefix.length(),
                                  &output);
    std::string decoded_url = base::UTF16ToUTF8(
        base::string16(output.data(), output.length()));
    dest->assign(decoded_url.begin(), decoded_url.end());
    return true;
  }
  return false;
}

void DistinguishAndAssignSrcLinkType(const GURL& url, AwHitTestData* data) {
  if (RemovePrefixAndAssignIfMatches(
      content::kAddressPrefix,
      url,
      &data->extra_data_for_type)) {
    data->type = AwHitTestData::GEO_TYPE;
  } else if (RemovePrefixAndAssignIfMatches(
      content::kPhoneNumberPrefix,
      url,
      &data->extra_data_for_type)) {
    data->type = AwHitTestData::PHONE_TYPE;
  } else if (RemovePrefixAndAssignIfMatches(
      content::kEmailPrefix,
      url,
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
    DCHECK(data->extra_data_for_type.length() == 0);
  }
}

}  // namespace

AwRenderViewExt::AwRenderViewExt(content::RenderView* render_view)
    : content::RenderViewObserver(render_view), page_scale_factor_(0.0f) {
}

AwRenderViewExt::~AwRenderViewExt() {
}

// static
void AwRenderViewExt::RenderViewCreated(content::RenderView* render_view) {
  new AwRenderViewExt(render_view);  // |render_view| takes ownership.
}

bool AwRenderViewExt::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AwRenderViewExt, message)
    IPC_MESSAGE_HANDLER(AwViewMsg_DocumentHasImages, OnDocumentHasImagesRequest)
    IPC_MESSAGE_HANDLER(AwViewMsg_DoHitTest, OnDoHitTest)
    IPC_MESSAGE_HANDLER(AwViewMsg_SetTextZoomFactor, OnSetTextZoomFactor)
    IPC_MESSAGE_HANDLER(AwViewMsg_ResetScrollAndScaleState,
                        OnResetScrollAndScaleState)
    IPC_MESSAGE_HANDLER(AwViewMsg_SetInitialPageScale, OnSetInitialPageScale)
    IPC_MESSAGE_HANDLER(AwViewMsg_SetFixedLayoutSize, OnSetFixedLayoutSize)
    IPC_MESSAGE_HANDLER(AwViewMsg_SetBackgroundColor, OnSetBackgroundColor)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AwRenderViewExt::OnDocumentHasImagesRequest(int id) {
  bool hasImages = false;
  if (render_view()) {
    blink::WebView* webview = render_view()->GetWebView();
    if (webview) {
      blink::WebVector<blink::WebElement> images;
      webview->mainFrame()->document().images(images);
      hasImages = !images.isEmpty();
    }
  }
  Send(new AwViewHostMsg_DocumentHasImagesResponse(routing_id(), id,
                                                   hasImages));
}

void AwRenderViewExt::DidCommitCompositorFrame() {
  UpdatePageScaleFactor();
}

void AwRenderViewExt::DidUpdateLayout() {
  if (check_contents_size_timer_.IsRunning())
    return;

  check_contents_size_timer_.Start(FROM_HERE,
                                   base::TimeDelta::FromMilliseconds(0), this,
                                   &AwRenderViewExt::CheckContentsSize);
}

void AwRenderViewExt::UpdatePageScaleFactor() {
  if (page_scale_factor_ != render_view()->GetWebView()->pageScaleFactor()) {
    page_scale_factor_ = render_view()->GetWebView()->pageScaleFactor();
    Send(new AwViewHostMsg_PageScaleFactorChanged(routing_id(),
                                                  page_scale_factor_));
  }
}

void AwRenderViewExt::CheckContentsSize() {
  if (!render_view()->GetWebView())
    return;

  gfx::Size contents_size;

  blink::WebFrame* main_frame = render_view()->GetWebView()->mainFrame();
  if (main_frame)
    contents_size = main_frame->contentsSize();

  // Fall back to contentsPreferredMinimumSize if the mainFrame is reporting a
  // 0x0 size (this happens during initial load).
  if (contents_size.IsEmpty()) {
    contents_size = render_view()->GetWebView()->contentsPreferredMinimumSize();
  }

  if (contents_size == last_sent_contents_size_)
    return;

  last_sent_contents_size_ = contents_size;
  Send(new AwViewHostMsg_OnContentsSizeChanged(routing_id(), contents_size));
}

void AwRenderViewExt::Navigate(const GURL& url) {
  // Navigate is called only on NEW navigations, so WebImageCache won't be freed
  // when the user just clicks on links, but only when a navigation is started,
  // for instance via loadUrl. A better approach would be clearing the cache on
  // cross-site boundaries, however this would require too many changes both on
  // the browser side (in RenderViewHostManger), to the IPCmessages and to the
  // RenderViewObserver. Thus, clearing decoding image cache on Navigate, seems
  // a more acceptable compromise.
  blink::WebImageCache::clear();
}

void AwRenderViewExt::FocusedNodeChanged(const blink::WebNode& node) {
  if (node.isNull() || !node.isElementNode() || !render_view())
    return;

  // Note: element is not const due to innerText() is not const.
  blink::WebElement element = node.toConst<blink::WebElement>();
  AwHitTestData data;

  data.href = GetHref(element);
  data.anchor_text = element.innerText();

  GURL absolute_link_url;
  if (node.isLink())
    absolute_link_url = GetAbsoluteUrl(node, data.href);

  GURL absolute_image_url;
  const blink::WebElement child_img = GetImgChild(element);
  if (!child_img.isNull()) {
    absolute_image_url =
        GetAbsoluteSrcUrl(child_img);
  }

  PopulateHitTestData(absolute_link_url,
                      absolute_image_url,
                      render_view()->IsEditableNode(node),
                      &data);
  Send(new AwViewHostMsg_UpdateHitTestData(routing_id(), data));
}

void AwRenderViewExt::OnDoHitTest(int view_x, int view_y) {
  if (!render_view() || !render_view()->GetWebView())
    return;

  const blink::WebHitTestResult result =
      render_view()->GetWebView()->hitTestResultAt(
          blink::WebPoint(view_x, view_y));
  AwHitTestData data;

  if (!result.urlElement().isNull()) {
    data.anchor_text = result.urlElement().innerText();
    data.href = GetHref(result.urlElement());
  }

  PopulateHitTestData(result.absoluteLinkURL(),
                      result.absoluteImageURL(),
                      result.isContentEditable(),
                      &data);
  Send(new AwViewHostMsg_UpdateHitTestData(routing_id(), data));
}

void AwRenderViewExt::OnSetTextZoomFactor(float zoom_factor) {
  if (!render_view() || !render_view()->GetWebView())
    return;
  // Hide selection and autofill popups.
  render_view()->GetWebView()->hidePopups();
  render_view()->GetWebView()->setTextZoomFactor(zoom_factor);
}

void AwRenderViewExt::OnResetScrollAndScaleState() {
  if (!render_view() || !render_view()->GetWebView())
    return;
  render_view()->GetWebView()->resetScrollAndScaleState();
}

void AwRenderViewExt::OnSetInitialPageScale(double page_scale_factor) {
  if (!render_view() || !render_view()->GetWebView())
    return;
  render_view()->GetWebView()->setInitialPageScaleOverride(
      page_scale_factor);
}

void AwRenderViewExt::OnSetFixedLayoutSize(const gfx::Size& size) {
  if (!render_view() || !render_view()->GetWebView())
    return;
  render_view()->GetWebView()->setFixedLayoutSize(size);
}

void AwRenderViewExt::OnSetBackgroundColor(SkColor c) {
  if (!render_view() || !render_view()->GetWebView())
    return;
  render_view()->GetWebView()->setBaseBackgroundColor(c);
}

}  // namespace android_webview
