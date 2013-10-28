// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/translate_bubble_factory.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"

namespace {

void ShowDefault(BrowserWindow* window,
                 content::WebContents* web_contents,
                 TranslateBubbleModel::ViewState view_state) {
  // |window| might be null when testing.
  if (!window)
    return;
  window->ShowTranslateBubble(web_contents, view_state);
}

}  // namespace

TranslateBubbleFactory::~TranslateBubbleFactory() {
}

// static
void TranslateBubbleFactory::Show(BrowserWindow* window,
                                  content::WebContents* web_contents,
                                  TranslateBubbleModel::ViewState view_state) {
  if (current_factory_) {
    current_factory_->ShowImplementation(window, web_contents, view_state);
    return;
  }

  ShowDefault(window, web_contents, view_state);
}

// static
void TranslateBubbleFactory::SetFactory(TranslateBubbleFactory* factory) {
  current_factory_ = factory;
}

// static
TranslateBubbleFactory* TranslateBubbleFactory::current_factory_ = NULL;
