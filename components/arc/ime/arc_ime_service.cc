// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/ime/arc_ime_service.h"

#include "base/logging.h"
#include "components/arc/ime/arc_ime_bridge_impl.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"

namespace arc {

namespace {

bool IsArcWindow(const aura::Window* window) {
  return exo::Surface::AsSurface(window);
}

bool IsArcTopLevelWindow(const aura::Window* window) {
  return exo::ShellSurface::GetMainSurface(window);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ArcImeService main implementation:

ArcImeService::ArcImeService(ArcBridgeService* bridge_service)
    : ArcService(bridge_service),
      ime_bridge_(new ArcImeBridgeImpl(this, bridge_service)),
      ime_type_(ui::TEXT_INPUT_TYPE_NONE),
      has_composition_text_(false),
      test_input_method_(nullptr) {
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->AddObserver(this);
}

ArcImeService::~ArcImeService() {
  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->DetachTextInputClient(this);

  for (aura::Window* window : arc_windows_.windows())
    window->RemoveObserver(this);
  for (aura::Window* root : observing_root_windows_.windows())
    aura::client::GetFocusClient(root)->RemoveObserver(this);
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->RemoveObserver(this);
}

void ArcImeService::SetImeBridgeForTesting(
    scoped_ptr<ArcImeBridge> test_ime_bridge) {
  ime_bridge_ = std::move(test_ime_bridge);
}

void ArcImeService::SetInputMethodForTesting(
    ui::InputMethod* test_input_method) {
  test_input_method_ = test_input_method;
}

ui::InputMethod* ArcImeService::GetInputMethod() {
  if (test_input_method_)
    return test_input_method_;
  if (focused_arc_window_.windows().empty())
    return nullptr;
  return focused_arc_window_.windows().front()->GetHost()->GetInputMethod();
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from aura::EnvObserver:

void ArcImeService::OnWindowInitialized(aura::Window* new_window) {
  if (IsArcWindow(new_window)) {
    arc_windows_.Add(new_window);
    new_window->AddObserver(this);
  }
}

void ArcImeService::OnWindowAddedToRootWindow(aura::Window* window) {
  aura::Window* root = window->GetRootWindow();
  aura::client::FocusClient* focus_client = aura::client::GetFocusClient(root);
  if (focus_client && !observing_root_windows_.Contains(root)) {
    focus_client->AddObserver(this);
    observing_root_windows_.Add(root);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from aura::client::FocusChangeObserver:

void ArcImeService::OnWindowFocused(aura::Window* gained_focus,
                                   aura::Window* lost_focus) {
  // The Aura focus may or may not be on sub-window of the toplevel ARC++ frame.
  // To handle all cases, judge the state by always climbing up to the toplevel.
  gained_focus = gained_focus ? gained_focus->GetToplevelWindow() : nullptr;
  lost_focus = lost_focus ? lost_focus->GetToplevelWindow() : nullptr;
  if (lost_focus == gained_focus)
    return;

  if (lost_focus && focused_arc_window_.Contains(lost_focus)) {
    ui::InputMethod* const input_method = GetInputMethod();
    if (input_method)
      input_method->DetachTextInputClient(this);
    focused_arc_window_.Remove(lost_focus);
  }

  if (gained_focus && IsArcTopLevelWindow(gained_focus)) {
    focused_arc_window_.Add(gained_focus);
    ui::InputMethod* const input_method = GetInputMethod();
    if (input_method)
      input_method->SetFocusedTextInputClient(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from arc::ArcImeBridge::Delegate

void ArcImeService::OnTextInputTypeChanged(ui::TextInputType type) {
  if (ime_type_ == type)
    return;
  ime_type_ = type;

  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method) {
    input_method->OnTextInputTypeChanged(this);
    if (input_method->GetTextInputClient() == this &&
        ime_type_ != ui::TEXT_INPUT_TYPE_NONE) {
      // TODO(kinaba): crbug.com/581282. This is tentative short-term solution.
      //
      // For fully correct implementation, rather than to piggyback the "show"
      // request with the input type change, we need dedicated IPCs to share the
      // virtual keyboard show/hide states between Chromium and ARC.
      input_method->ShowImeIfNeeded();
    }
  }
}

void ArcImeService::OnCursorRectChanged(const gfx::Rect& rect) {
  if (cursor_rect_ == rect)
    return;
  cursor_rect_ = rect;

  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->OnCaretBoundsChanged(this);
}

void ArcImeService::OnCancelComposition() {
  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->CancelComposition(this);
}

////////////////////////////////////////////////////////////////////////////////
// Oberridden from ui::TextInputClient:

void ArcImeService::SetCompositionText(
    const ui::CompositionText& composition) {
  has_composition_text_ = !composition.text.empty();
  ime_bridge_->SendSetCompositionText(composition);
}

void ArcImeService::ConfirmCompositionText() {
  has_composition_text_ = false;
  ime_bridge_->SendConfirmCompositionText();
}

void ArcImeService::ClearCompositionText() {
  if (has_composition_text_) {
    has_composition_text_ = false;
    ime_bridge_->SendInsertText(base::string16());
  }
}

void ArcImeService::InsertText(const base::string16& text) {
  has_composition_text_ = false;
  ime_bridge_->SendInsertText(text);
}

void ArcImeService::InsertChar(const ui::KeyEvent& event) {
  // Drop 0x00-0x1f (C0 controls), 0x7f (DEL), and 0x80-0x9f (C1 controls).
  // See: https://en.wikipedia.org/wiki/Unicode_control_characters
  // They are control characters and not treated as a text insertion.
  const base::char16 ch = event.GetCharacter();
  const bool is_control_char = (0x00 <= ch && ch <= 0x1f) ||
                               (0x7f <= ch && ch <= 0x9f);

  if (!is_control_char && !ui::IsSystemKeyModifier(event.flags())) {
    has_composition_text_ = false;
    ime_bridge_->SendInsertText(base::string16(1, event.GetText()));
  }
}

ui::TextInputType ArcImeService::GetTextInputType() const {
  return ime_type_;
}

gfx::Rect ArcImeService::GetCaretBounds() const {
  if (focused_arc_window_.windows().empty())
    return gfx::Rect();
  aura::Window* window = focused_arc_window_.windows().front();

  // |cursor_rect_| holds the rectangle reported from ARC apps, in the "screen
  // coordinates" in ARC, counted by physical pixels.
  // Chrome OS input methods expect the coordinates in Chrome OS screen, within
  // device independent pixels. Two factors are involved for the conversion.

  // Divide by the scale factor. To convert from physical pixels to DIP.
  gfx::Rect converted = gfx::ScaleToEnclosingRect(
      cursor_rect_, 1 / window->layer()->device_scale_factor());

  // Add the offset of the window showing the ARC app.
  converted.Offset(window->GetBoundsInScreen().OffsetFromOrigin());
  return converted;
}

ui::TextInputMode ArcImeService::GetTextInputMode() const {
  return ui::TEXT_INPUT_MODE_DEFAULT;
}

int ArcImeService::GetTextInputFlags() const {
  return ui::TEXT_INPUT_FLAG_NONE;
}

bool ArcImeService::CanComposeInline() const {
  return true;
}

bool ArcImeService::GetCompositionCharacterBounds(
    uint32_t index, gfx::Rect* rect) const {
  return false;
}

bool ArcImeService::HasCompositionText() const {
  return has_composition_text_;
}

bool ArcImeService::GetTextRange(gfx::Range* range) const {
  return false;
}

bool ArcImeService::GetCompositionTextRange(gfx::Range* range) const {
  return false;
}

bool ArcImeService::GetSelectionRange(gfx::Range* range) const {
  return false;
}

bool ArcImeService::SetSelectionRange(const gfx::Range& range) {
  return false;
}

bool ArcImeService::DeleteRange(const gfx::Range& range) {
  return false;
}

bool ArcImeService::GetTextFromRange(
    const gfx::Range& range, base::string16* text) const {
  return false;
}

bool ArcImeService::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  return false;
}

bool ArcImeService::IsEditCommandEnabled(int command_id) {
  return false;
}

}  // namespace arc
