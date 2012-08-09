// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/searchbox_extension.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

SearchBox::SearchBox(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      content::RenderViewObserverTracker<SearchBox>(render_view),
      verbatim_(false),
      selection_start_(0),
      selection_end_(0) {
}

SearchBox::~SearchBox() {
}

void SearchBox::SetSuggestions(const std::vector<string16>& suggestions,
                               InstantCompleteBehavior behavior) {
  // Explicitly allow empty vector to be sent to the browser.
  render_view()->Send(new ChromeViewHostMsg_SetSuggestions(
      render_view()->GetRoutingID(), render_view()->GetPageId(), suggestions,
      behavior));
}

gfx::Rect SearchBox::GetRect() {
  // Need to adjust for scale.
  if (rect_.IsEmpty())
    return rect_;
  WebKit::WebView* web_view = render_view()->GetWebView();
  if (!web_view)
    return rect_;
  double zoom = WebKit::WebView::zoomLevelToZoomFactor(web_view->zoomLevel());
  if (zoom == 0)
    return rect_;
  return gfx::Rect(static_cast<int>(static_cast<float>(rect_.x()) / zoom),
                   static_cast<int>(static_cast<float>(rect_.y()) / zoom),
                   static_cast<int>(static_cast<float>(rect_.width()) / zoom),
                   static_cast<int>(static_cast<float>(rect_.height()) / zoom));
}

bool SearchBox::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SearchBox, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxChange, OnChange)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxSubmit, OnSubmit)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxCancel, OnCancel)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxResize, OnResize)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_DetermineIfPageSupportsInstant,
                        OnDetermineIfPageSupportsInstant)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SearchBox::OnChange(const string16& value,
                         bool verbatim,
                         size_t selection_start,
                         size_t selection_end) {
  value_ = value;
  verbatim_ = verbatim;
  selection_start_ = selection_start;
  selection_end_ = selection_end;
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchChange(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnSubmit(const string16& value) {
  value_ = value;
  verbatim_ = true;
  selection_start_ = selection_end_ = value_.size();
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchSubmit(
        render_view()->GetWebView()->mainFrame());
  }
  Reset();
}

void SearchBox::OnCancel(const string16& value) {
  value_ = value;
  verbatim_ = true;
  selection_start_ = selection_end_ = value_.size();
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchCancel(
        render_view()->GetWebView()->mainFrame());
  }
  Reset();
}

void SearchBox::OnResize(const gfx::Rect& bounds) {
  rect_ = bounds;
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchResize(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnDetermineIfPageSupportsInstant() {
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    bool result = extensions_v8::SearchBoxExtension::PageSupportsInstant(
        render_view()->GetWebView()->mainFrame());
    render_view()->Send(new ChromeViewHostMsg_InstantSupportDetermined(
        render_view()->GetRoutingID(), render_view()->GetPageId(), result));
  }
}

void SearchBox::Reset() {
  value_.clear();
  verbatim_ = false;
  selection_start_ = selection_end_ = 0;
  rect_ = gfx::Rect();
}
