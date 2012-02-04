// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

#include "ui/gfx/size.h"

namespace {
// The minimum width of the inline disposition tab contents.
const int kMinInlineDispositionWidth = 300;

// The minimum height of the inline disposition tab contents.
const int kMinInlineDispositionHeight = 300;
}

// static
gfx::Size WebIntentPicker::GetDefaultInlineDispositionSize(
    content::WebContents* web_contents) {
  // TODO(gbillock): This size calculation needs more thought.
  gfx::Size tab_size = web_contents->GetView()->GetContainerSize();
  int width = std::max(tab_size.width()/2, kMinInlineDispositionWidth);
  int height = std::max(tab_size.height()/2, kMinInlineDispositionHeight);

  return gfx::Size(width, height);
}
