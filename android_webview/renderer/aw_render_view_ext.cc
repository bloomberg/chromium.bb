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
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHitTestResult.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

namespace android_webview {

namespace {

bool RemovePrefixAndAssignIfMatches(const base::StringPiece& prefix,
                                    const GURL& url,
                                    std::string* dest) {
  const base::StringPiece spec(url.spec());

  if (spec.starts_with(prefix)) {
    dest->assign(spec.begin() + prefix.length(), spec.end());
    return true;
  }
  return false;
}

}

AwRenderViewExt::AwRenderViewExt(content::RenderView* render_view)
    : content::RenderViewObserver(render_view) {
  render_view->GetWebView()->setPermissionClient(this);
  // TODO(leandrogracia): enable once the feature is available in RenderView.
  //render_view->SetCapturePictureCallback(
  //    base::Bind(&AwRenderViewExt::OnPictureUpdate, AsWeakPtr()));
}

AwRenderViewExt::~AwRenderViewExt() {
  // TODO(leandrogracia): enable once the feature is available in RenderView.
  //render_view()->SetCapturePictureCallback(
  //    content::RenderView::CapturePictureCallback());
  RendererPictureMap::GetInstance()->ClearRendererPicture(routing_id());
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
    IPC_MESSAGE_HANDLER(AwViewMsg_EnableCapturePictureCallback,
                        OnEnableCapturePictureCallback)
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
  if (!node.isNull()) {
    if (node.isTextNode() && node.isContentEditable()) {
      AwHitTestData data;
      data.type = AwHitTestData::EDIT_TEXT_TYPE;
      Send(new AwViewHostMsg_UpdateHitTestData(
          routing_id(), data));
    } else {
      // TODO(boliu): Implement this path.
      NOTIMPLEMENTED() << "Tab focused links not implemented";
    }
  }
}

void AwRenderViewExt::OnDoHitTest(int view_x, int view_y) {
  if (!render_view() || !render_view()->GetWebView())
    return;

  const WebKit::WebHitTestResult result =
      render_view()->GetWebView()->hitTestResultAt(
          WebKit::WebPoint(view_x, view_y));
  AwHitTestData data;

  // Populate fixed AwHitTestData fields.
  if (result.absoluteImageURL().isValid())
    data.img_src = result.absoluteImageURL();
  if (!result.urlElement().isNull()) {
    data.anchor_text = result.urlElement().innerText();

    // href is the actual 'href' attribute, which might relative if valid or can
    // possibly contain garbage otherwise, so not using absoluteLinkURL here.
    data.href = result.urlElement().getAttribute("href");
  }

  GURL url(result.absoluteLinkURL());
  bool is_javascript_scheme = url.SchemeIs(chrome::kJavaScriptScheme);

  // Set AwHitTestData type and extra_data_for_type.
  if (result.absoluteLinkURL().isValid() &&
      !result.absoluteImageURL().isValid() &&
      !is_javascript_scheme) {
    if (RemovePrefixAndAssignIfMatches(
        content::kAddressPrefix,
        url,
        &data.extra_data_for_type)) {
      data.type = AwHitTestData::GEO_TYPE;
    } else if (RemovePrefixAndAssignIfMatches(
        content::kPhoneNumberPrefix,
        url,
        &data.extra_data_for_type)) {
      data.type = AwHitTestData::PHONE_TYPE;
    } else if (RemovePrefixAndAssignIfMatches(
        content::kEmailPrefix,
        url,
        &data.extra_data_for_type)) {
      data.type = AwHitTestData::EMAIL_TYPE;
    } else {
      data.type = AwHitTestData::SRC_LINK_TYPE;
      data.extra_data_for_type = url.spec();
    }
  } else if (result.absoluteLinkURL().isValid() &&
             result.absoluteImageURL().isValid() &&
             !is_javascript_scheme) {
    data.type = AwHitTestData::SRC_IMAGE_LINK_TYPE;
    data.extra_data_for_type = data.img_src.spec();
  } else if (!result.absoluteLinkURL().isValid() &&
             result.absoluteImageURL().isValid()) {
    data.type = AwHitTestData::IMAGE_TYPE;
    data.extra_data_for_type = data.img_src.spec();
  } else if (result.isContentEditable()) {
    data.type = AwHitTestData::EDIT_TEXT_TYPE;
    DCHECK(data.extra_data_for_type.length() == 0);
  }

  Send(new AwViewHostMsg_UpdateHitTestData(routing_id(), data));
}

void AwRenderViewExt::OnEnableCapturePictureCallback(bool enable) {
  // TODO(leandrogracia): enable once the feature is available in RenderView.
  //render_view()->SetCapturePictureCallback(enable ?
  //    base::Bind(&AwRenderViewExt::OnPictureUpdate, AsWeakPtr()) :
  //    content::RenderView::CapturePictureCallback());
}

void AwRenderViewExt::OnPictureUpdate(
    scoped_refptr<cc::PicturePileImpl> picture) {
  RendererPictureMap::GetInstance()->SetRendererPicture(routing_id(), picture);
  Send(new AwViewHostMsg_PictureUpdated(routing_id()));
}

}  // namespace android_webview
