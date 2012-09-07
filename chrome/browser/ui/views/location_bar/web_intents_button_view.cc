// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/web_intents_button_view.h"

#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "grit/generated_resources.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/animation/tween.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

// Animation time to open the button.
const int kMoveTimeMs = 150;

WebIntentsButtonView::WebIntentsButtonView(LocationBarView* parent,
                                           const int background_images[])
    : LocationBarDecorationView(parent, background_images) {
}

void WebIntentsButtonView::Update(TabContents* tab_contents) {
  if (!tab_contents ||
      !tab_contents->web_intent_picker_controller() ||
      !tab_contents->web_intent_picker_controller()->
          ShowLocationBarPickerTool()) {
    SetVisible(false);
  } else {
    SetVisible(true);
  }

  int animated_string_id = IDS_INTENT_PICKER_USE_ANOTHER_SERVICE;
  string16 animated_text = l10n_util::GetStringUTF16(animated_string_id);
  SetTooltipText(animated_text);

  StartLabelAnimation(animated_text, kMoveTimeMs);
  AlwaysDrawText();
}

void WebIntentsButtonView::OnClick(LocationBarView* parent) {
  TabContents* tab_contents = parent->GetTabContents();
  if (!tab_contents)
    return;

  tab_contents->web_intent_picker_controller()->LocationBarPickerToolClicked();
}

int WebIntentsButtonView::GetTextAnimationSize(double state, int text_size) {
  if (state < 1.0) {
    return static_cast<int>(text_size * state);
  }

  return text_size;
}
