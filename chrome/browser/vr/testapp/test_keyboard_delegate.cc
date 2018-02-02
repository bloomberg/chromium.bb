// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/testapp/test_keyboard_delegate.h"

#include <memory>

#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace vr {

namespace {

constexpr gfx::SizeF kKeyboardSize = {1.2f, 0.37f};
constexpr gfx::Vector2dF kKeyboardTranslate = {0, -0.1};

}  // namespace

TestKeyboardDelegate::TestKeyboardDelegate()
    : renderer_(std::make_unique<TestKeyboardRenderer>()) {}

TestKeyboardDelegate::~TestKeyboardDelegate() {}

void TestKeyboardDelegate::ShowKeyboard() {
  editing_ = true;
}

void TestKeyboardDelegate::HideKeyboard() {
  editing_ = false;
}

void TestKeyboardDelegate::SetTransform(const gfx::Transform& transform) {
  world_space_transform_ = transform;
}

bool TestKeyboardDelegate::HitTest(const gfx::Point3F& ray_origin,
                                   const gfx::Point3F& ray_target,
                                   gfx::Point3F* hit_position) {
  // TODO(ymalik): Add hittesting logic for the keyboard.
  return false;
}

void TestKeyboardDelegate::Draw(const CameraModel& model) {
  if (!editing_)
    return;

  // We try to simulate what the gvr keyboard does here by scaling and
  // translating the keyboard on top of the provided transform.
  gfx::Transform world_space_transform = world_space_transform_;
  world_space_transform.Scale(kKeyboardSize.width(), kKeyboardSize.height());
  world_space_transform.Translate(kKeyboardTranslate);
  renderer_->Draw(model, world_space_transform);
}

void TestKeyboardDelegate::Initialize(vr::SkiaSurfaceProvider* provider,
                                      UiElementRenderer* renderer) {
  renderer_->Initialize(provider, renderer);
}

void TestKeyboardDelegate::UpdateInput(const vr::TextInputInfo& info) {
  if (input_info_ == info)
    return;

  input_info_ = info;
  ui_interface_->OnInputEdited(input_info_);
}

bool TestKeyboardDelegate::HandleInput(ui::Event* e) {
  DCHECK(ui_interface_);
  DCHECK(e->IsKeyEvent());
  if (!editing_)
    return false;

  auto* event = e->AsKeyEvent();
  switch (event->key_code()) {
    case ui::VKEY_RETURN:
      input_info_.text.clear();
      input_info_.selection_start = input_info_.selection_end = 0;
      ui_interface_->OnInputCommitted(input_info_);
      break;
    case ui::VKEY_BACK:
      if (input_info_.selection_start != input_info_.selection_end) {
        input_info_.text.erase(
            input_info_.selection_start,
            input_info_.selection_end - input_info_.selection_start);
        input_info_.selection_end = input_info_.selection_start;
      } else if (!input_info_.text.empty() && input_info_.selection_start > 0) {
        input_info_.text.erase(input_info_.selection_start - 1, 1);
        input_info_.selection_start--;
        input_info_.selection_end--;
      }
      ui_interface_->OnInputEdited(input_info_);
      break;
    default:
      if (input_info_.selection_start != input_info_.selection_end) {
        input_info_.text.erase(
            input_info_.selection_start,
            input_info_.selection_end - input_info_.selection_start);
        input_info_.selection_end = input_info_.selection_start;
      }

      std::string character;
      base::WriteUnicodeCharacter(event->GetText(), &character);
      input_info_.text = input_info_.text.insert(input_info_.selection_start,
                                                 base::UTF8ToUTF16(character));
      input_info_.selection_start++;
      input_info_.selection_end++;
      ui_interface_->OnInputEdited(input_info_);
      break;
  }
  // We want to continue handling this keypress if the Ctrl key is down so
  // that we can do things like duming the tree in editing mode.
  return !event->IsControlDown();
}

}  // namespace vr
