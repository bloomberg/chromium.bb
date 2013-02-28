// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_render_view_ext.h"

#include <string>

#include "android_webview/common/aw_hit_test_data.h"
#include "android_webview/common/render_view_messages.h"
#include "android_webview/common/renderer_picture_map.h"
#include "base/bind.h"
#include "base/string_piece.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/android_content_detection_prefixes.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_view.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHitTestResult.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNodeList.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace android_webview {

namespace {

GURL GetAbsoluteUrl(const WebKit::WebNode& node, const string16& url_fragment) {
  return GURL(node.document().completeURL(url_fragment));
}

string16 GetHref(const WebKit::WebElement& element) {
  // Get the actual 'href' attribute, which might relative if valid or can
  // possibly contain garbage otherwise, so not using absoluteLinkURL here.
  return element.getAttribute("href");
}

GURL GetAbsoluteSrcUrl(const WebKit::WebElement& element) {
  if (element.isNull())
    return GURL();
  return GetAbsoluteUrl(element, element.getAttribute("src"));
}

WebKit::WebNode GetImgChild(const WebKit::WebNode& node) {
  // This implementation is incomplete (for example if is an area tag) but
  // matches the original WebViewClassic implementation.

  WebKit::WebNodeList list = node.getElementsByTagName("img");
  if (list.length() > 0)
    return list.item(0);
  return WebKit::WebNode();
}

bool RemovePrefixAndAssignIfMatches(const base::StringPiece& prefix,
                                    const GURL& url,
                                    std::string* dest) {
  const base::StringPiece spec(url.possibly_invalid_spec());

  if (spec.starts_with(prefix)) {
    dest->assign(spec.begin() + prefix.length(), spec.end());
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
      absolute_link_url.SchemeIs(chrome::kJavaScriptScheme);
  const bool has_link_url = !absolute_link_url.is_empty();
  const bool has_image_url = !absolute_image_url.is_empty();

  if (has_link_url && !has_image_url && !is_javascript_scheme) {
    DistinguishAndAssignSrcLinkType(absolute_link_url, data);
  } else if (has_link_url && has_image_url && !is_javascript_scheme) {
    data->type = AwHitTestData::SRC_IMAGE_LINK_TYPE;
    data->extra_data_for_type = data->img_src.possibly_invalid_spec();
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
    : content::RenderViewObserver(render_view) {
  render_view->GetWebView()->setPermissionClient(this);
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
    IPC_MESSAGE_HANDLER(AwViewMsg_SetEnableFixedLayoutMode,
                        OnSetEnableFixedLayoutMode)
    IPC_MESSAGE_HANDLER(AwViewMsg_SetTextZoomLevel, OnSetTextZoomLevel)
    IPC_MESSAGE_HANDLER(AwViewMsg_ResetScrollAndScaleState,
                        OnResetScrollAndScaleState)
    IPC_MESSAGE_HANDLER(AwViewMsg_SetInitialPageScale, OnSetInitialPageScale)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AwRenderViewExt::OnDocumentHasImagesRequest(int id) {
  bool hasImages = false;
  if (render_view()) {
    WebKit::WebView* webview = render_view()->GetWebView();
    if (webview) {
      WebKit::WebVector<WebKit::WebElement> images;
      webview->mainFrame()->document().images(images);
      hasImages = !images.isEmpty();
    }
  }
  Send(new AwViewHostMsg_DocumentHasImagesResponse(routing_id(), id,
                                                   hasImages));
}

bool AwRenderViewExt::allowImage(WebKit::WebFrame* frame,
                                 bool enabled_per_settings,
                                 const WebKit::WebURL& image_url) {
  // Implementing setBlockNetworkImages, so allow local scheme images to be
  // loaded.
  if (enabled_per_settings)
    return true;

  // For compatibility, only blacklist network schemes instead of whitelisting.
  const GURL url(image_url);
  return !(url.SchemeIs(chrome::kHttpScheme) ||
           url.SchemeIs(chrome::kHttpsScheme) ||
           url.SchemeIs(chrome::kFtpScheme));
}

void AwRenderViewExt::DidCommitProvisionalLoad(WebKit::WebFrame* frame,
                                               bool is_new_navigation) {
  content::DocumentState* document_state =
      content::DocumentState::FromDataSource(frame->dataSource());
  if (document_state->can_load_local_resources()) {
    WebKit::WebSecurityOrigin origin = frame->document().securityOrigin();
    origin.grantLoadLocalResources();
  }
}

void AwRenderViewExt::FocusedNodeChanged(const WebKit::WebNode& node) {
  if (node.isNull() || !node.isElementNode() || !render_view())
    return;

  // Note: element is not const due to innerText() is not const.
  WebKit::WebElement element = node.toConst<WebKit::WebElement>();
  AwHitTestData data;

  data.href = GetHref(element);
  data.anchor_text = element.innerText();

  GURL absolute_link_url;
  if (node.isLink())
    absolute_link_url = GetAbsoluteUrl(node, data.href);

  GURL absolute_image_url;
  const WebKit::WebNode child_img = GetImgChild(node);
  if (!child_img.isNull() && child_img.isElementNode()) {
    absolute_image_url =
        GetAbsoluteSrcUrl(child_img.toConst<WebKit::WebElement>());
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

  const WebKit::WebHitTestResult result =
      render_view()->GetWebView()->hitTestResultAt(
          WebKit::WebPoint(view_x, view_y));
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

void AwRenderViewExt::OnSetEnableFixedLayoutMode(bool enabled) {
  if (!render_view() || !render_view()->GetWebView())
    return;
  render_view()->GetWebView()->enableFixedLayoutMode(enabled);
}

void AwRenderViewExt::OnSetTextZoomLevel(double zoom_level) {
  if (!render_view() || !render_view()->GetWebView())
    return;
  // Hide selection and autofill popups.
  render_view()->GetWebView()->hidePopups();
  render_view()->GetWebView()->setZoomLevel(true, zoom_level);
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

}  // namespace android_webview
