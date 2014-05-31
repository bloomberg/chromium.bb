// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/web_activity.h"

#include "athena/activity/public/activity_manager.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/web_contents.h"

WebActivity::WebActivity(content::WebContents* contents)
    : content::WebContentsObserver(contents) {
}

WebActivity::~WebActivity() {
  athena::ActivityManager::Get()->RemoveActivity(this);
}

athena::ActivityViewModel* WebActivity::GetActivityViewModel() {
  return this;
}

SkColor WebActivity::GetRepresentativeColor() {
  // TODO(sad): Compute the color from the favicon.
  return SK_ColorGRAY;
}

std::string WebActivity::GetTitle() {
  return base::UTF16ToUTF8(web_contents()->GetTitle());
}

aura::Window* WebActivity::GetNativeWindow() {
  return web_contents()->GetNativeView();
}

void WebActivity::TitleWasSet(content::NavigationEntry* entry,
                              bool explicit_set) {
  athena::ActivityManager::Get()->UpdateActivity(this);
}

void WebActivity::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  athena::ActivityManager::Get()->UpdateActivity(this);
}
