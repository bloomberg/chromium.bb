// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/searchbox_extension.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebView;

SearchBox::SearchBox(RenderView* render_view)
    : RenderViewObserver(render_view),
      verbatim_(false),
      selection_start_(0),
      selection_end_(0) {
}

SearchBox::~SearchBox() {
}

void SearchBox::SetSuggestions(const std::vector<std::string>& suggestions,
                               InstantCompleteBehavior behavior) {
  // Explicitly allow empty vector to be sent to the browser.
  render_view()->Send(new ViewHostMsg_SetSuggestions(
      render_view()->routing_id(), render_view()->page_id(), suggestions,
      behavior));
}

bool SearchBox::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SearchBox, message)
    IPC_MESSAGE_HANDLER(ViewMsg_SearchBoxChange, OnChange)
    IPC_MESSAGE_HANDLER(ViewMsg_SearchBoxSubmit, OnSubmit)
    IPC_MESSAGE_HANDLER(ViewMsg_SearchBoxCancel, OnCancel)
    IPC_MESSAGE_HANDLER(ViewMsg_SearchBoxResize, OnResize)
    IPC_MESSAGE_HANDLER(ViewMsg_DetermineIfPageSupportsInstant,
                        OnDetermineIfPageSupportsInstant)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SearchBox::OnChange(const string16& value,
                         bool verbatim,
                         int selection_start,
                         int selection_end) {
  value_ = value;
  verbatim_ = verbatim;
  selection_start_ = selection_start;
  selection_end_ = selection_end;
  if (!render_view()->webview() || !render_view()->webview()->mainFrame())
    return;
  extensions_v8::SearchBoxExtension::DispatchChange(
      render_view()->webview()->mainFrame());
}

void SearchBox::OnSubmit(const string16& value, bool verbatim) {
  value_ = value;
  verbatim_ = verbatim;
  if (!render_view()->webview() || !render_view()->webview()->mainFrame())
    return;
  extensions_v8::SearchBoxExtension::DispatchSubmit(
      render_view()->webview()->mainFrame());
}

void SearchBox::OnCancel() {
  verbatim_ = false;
  if (!render_view()->webview() || !render_view()->webview()->mainFrame())
    return;
  extensions_v8::SearchBoxExtension::DispatchCancel(
      render_view()->webview()->mainFrame());
}

void SearchBox::OnResize(const gfx::Rect& bounds) {
  rect_ = bounds;
  if (!render_view()->webview() || !render_view()->webview()->mainFrame())
    return;
  extensions_v8::SearchBoxExtension::DispatchResize(
      render_view()->webview()->mainFrame());
}

void SearchBox::OnDetermineIfPageSupportsInstant(const string16& value,
                                                 bool verbatim,
                                                 int selection_start,
                                                 int selection_end) {
  value_ = value;
  verbatim_ = verbatim;
  selection_start_ = selection_start;
  selection_end_ = selection_end;
  bool result = extensions_v8::SearchBoxExtension::PageSupportsInstant(
      render_view()->webview()->mainFrame());
  render_view()->Send(new ViewHostMsg_InstantSupportDetermined(
      render_view()->routing_id(), render_view()->page_id(), result));
}
