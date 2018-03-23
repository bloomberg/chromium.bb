// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/toast.h"

#include "chrome/browser/vr/elements/linear_layout.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/ui_element_type.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace vr {

Toast::Toast() {
  set_bounds_contain_children(true);

  auto background = std::make_unique<Rect>();
  background->SetType(kTypeToastBackground);
  background->set_contributes_to_parent_bounds(false);
  background_ = background.get();

  auto container = std::make_unique<LinearLayout>(LinearLayout::kRight);
  container->SetType(kTypeToastContainer);
  container_ = container.get();

  AddChild(std::move(background));
  AddChild(std::move(container));
}

Toast::~Toast() = default;

void Toast::Render(UiElementRenderer* renderer,
                   const CameraModel& model) const {}

void Toast::AddIcon(const gfx::VectorIcon& icon,
                    int width_pixels,
                    float icon_size) {
  DCHECK(!icon_);
  auto vector_icon = std::make_unique<VectorIcon>(width_pixels);
  vector_icon->SetType(kTypeToastIcon);
  vector_icon->SetDrawPhase(draw_phase());
  vector_icon->SetIcon(icon);
  vector_icon->SetSize(icon_size, icon_size);
  vector_icon->SetVisible(true);
  vector_icon->set_owner_name_for_test(name());
  icon_ = vector_icon.get();
  container_->AddChild(std::move(vector_icon));
}

void Toast::AddText(const base::string16& text,
                    float font_height_dmm,
                    TextLayoutMode text_layout_mode) {
  DCHECK(!text_);
  auto text_element = std::make_unique<Text>(font_height_dmm);
  text_element->SetType(kTypeToastText);
  text_element->SetDrawPhase(draw_phase());
  text_element->SetText(text);
  text_element->SetLayoutMode(text_layout_mode);
  text_element->set_owner_name_for_test(name());
  text_element->SetVisible(true);
  text_ = text_element.get();
  container_->AddChild(std::move(text_element));
}

void Toast::SetMargin(float margin) {
  container_->set_margin(margin);
}

void Toast::OnSetDrawPhase() {
  background_->SetDrawPhase(draw_phase());
  if (icon_)
    icon_->SetDrawPhase(draw_phase());
  if (text_)
    text_->SetDrawPhase(draw_phase());
}

void Toast::OnSetName() {
  background_->set_owner_name_for_test(name());
  container_->set_owner_name_for_test(name());
  if (icon_)
    icon_->set_owner_name_for_test(name());
  if (text_)
    text_->set_owner_name_for_test(name());
}

void Toast::OnSetSize(const gfx::SizeF& size) {
  background_->SetSize(size.width(), size.height());
}

void Toast::OnSetCornerRadii(const CornerRadii& radii) {
  background_->SetCornerRadii(radii);
}

void Toast::SetForegroundColor(SkColor color) {
  DCHECK(icon_ || text_);
  if (icon_)
    icon_->SetColor(color);
  if (text_)
    text_->SetColor(color);
}

void Toast::SetBackgroundColor(SkColor color) {
  DCHECK(background_);
  background_->SetColor(color);
}

void Toast::NotifyClientSizeAnimated(const gfx::SizeF& size,
                                     int target_property_id,
                                     cc::KeyframeModel* animation) {
  if (target_property_id == BOUNDS)
    background_->SetSize(size.width(), size.height());
  UiElement::NotifyClientSizeAnimated(size, target_property_id, animation);
}

}  // namespace vr
