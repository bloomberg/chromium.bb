// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/test_browser_plugin_guest_delegate.h"

namespace content {

TestBrowserPluginGuestDelegate::TestBrowserPluginGuestDelegate()
    : load_aborted_(false) {
}

TestBrowserPluginGuestDelegate::~TestBrowserPluginGuestDelegate() {
}

void TestBrowserPluginGuestDelegate::ResetStates() {
  load_aborted_ = false;
  load_aborted_url_ = GURL();
}

void TestBrowserPluginGuestDelegate::AddMessageToConsole(
    int32 level,
    const base::string16& message,
    int32 line_no,
    const base::string16& source_id) {
}

void TestBrowserPluginGuestDelegate::Close() {
}

void TestBrowserPluginGuestDelegate::GuestProcessGone(
    base::TerminationStatus status) {
}

bool TestBrowserPluginGuestDelegate::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  return BrowserPluginGuestDelegate::HandleKeyboardEvent(event);
}

void TestBrowserPluginGuestDelegate::LoadAbort(bool is_top_level,
                                               const GURL& url,
                                               const std::string& error_type) {
  load_aborted_ = true;
  load_aborted_url_ = url;
}

void TestBrowserPluginGuestDelegate::RendererResponsive() {
}

void TestBrowserPluginGuestDelegate::RendererUnresponsive() {
}

void TestBrowserPluginGuestDelegate::RequestPermission(
    BrowserPluginPermissionType permission_type,
    const base::DictionaryValue& request_info,
    const PermissionResponseCallback& callback,
    bool allowed_by_default) {
}

void TestBrowserPluginGuestDelegate::SizeChanged(const gfx::Size& old_size,
                                                 const gfx::Size& new_size) {
}

}  // namespace content
