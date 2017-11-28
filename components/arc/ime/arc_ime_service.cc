// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/ime/arc_ime_service.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/ime/arc_ime_bridge_impl.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/range/range.h"

namespace arc {

namespace {

class ArcWindowDelegateImpl : public ArcImeService::ArcWindowDelegate {
 public:
  explicit ArcWindowDelegateImpl(ArcImeService* ime_service)
    : ime_service_(ime_service) {}

  ~ArcWindowDelegateImpl() override = default;

  bool IsArcWindow(
      const aura::Window* window) const override {
    return exo::Surface::AsSurface(window) ||
           exo::ShellSurface::GetMainSurface(window);
  }

  void RegisterFocusObserver() override {
    DCHECK(exo::WMHelper::HasInstance());
    exo::WMHelper::GetInstance()->AddFocusObserver(ime_service_);
  }

  void UnregisterFocusObserver() override {
    // If WMHelper is already destroyed, do nothing.
    // TODO(crbug.com/748380): Fix shutdown order.
    if (!exo::WMHelper::HasInstance())
      return;
    exo::WMHelper::GetInstance()->RemoveFocusObserver(ime_service_);
  }

  ui::InputMethod* GetInputMethodForWindow(
      aura::Window* window) const override {
    if (!window || !window->GetHost())
      return nullptr;
    return window->GetHost()->GetInputMethod();
  }

 private:
  ArcImeService* const ime_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcWindowDelegateImpl);
};

// Singleton factory for ArcImeService.
class ArcImeServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcImeService,
          ArcImeServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcImeServiceFactory";

  static ArcImeServiceFactory* GetInstance() {
    return base::Singleton<ArcImeServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcImeServiceFactory>;
  ArcImeServiceFactory() = default;
  ~ArcImeServiceFactory() override = default;
};

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// ArcImeService main implementation:

// static
ArcImeService* ArcImeService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcImeServiceFactory::GetForBrowserContext(context);
}

ArcImeService::ArcImeService(content::BrowserContext* context,
                             ArcBridgeService* bridge_service)
    : ime_bridge_(new ArcImeBridgeImpl(this, bridge_service)),
      arc_window_delegate_(new ArcWindowDelegateImpl(this)),
      ime_type_(ui::TEXT_INPUT_TYPE_NONE),
      has_composition_text_(false),
      keyboard_controller_(nullptr),
      is_focus_observer_installed_(false) {
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->AddObserver(this);
}

ArcImeService::~ArcImeService() {
  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->DetachTextInputClient(this);

  if (focused_arc_window_)
    focused_arc_window_->RemoveObserver(this);
  if (is_focus_observer_installed_)
    arc_window_delegate_->UnregisterFocusObserver();
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->RemoveObserver(this);
  // Removing |this| from KeyboardController.
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller && keyboard_controller_ == keyboard_controller) {
    keyboard_controller_->RemoveObserver(this);
  }
}

void ArcImeService::SetImeBridgeForTesting(
    std::unique_ptr<ArcImeBridge> test_ime_bridge) {
  ime_bridge_ = std::move(test_ime_bridge);
}

void ArcImeService::SetArcWindowDelegateForTesting(
    std::unique_ptr<ArcWindowDelegate> delegate) {
  arc_window_delegate_ = std::move(delegate);
}

ui::InputMethod* ArcImeService::GetInputMethod() {
  return arc_window_delegate_->GetInputMethodForWindow(focused_arc_window_);
}

void ArcImeService::ReattachInputMethod(aura::Window* old_window,
                                        aura::Window* new_window) {
  ui::InputMethod* const old_ime =
      arc_window_delegate_->GetInputMethodForWindow(old_window);
  ui::InputMethod* const new_ime =
      arc_window_delegate_->GetInputMethodForWindow(new_window);

  if (old_ime != new_ime) {
    if (old_ime)
      old_ime->DetachTextInputClient(this);
    if (new_ime)
      new_ime->SetFocusedTextInputClient(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from aura::EnvObserver:

void ArcImeService::OnWindowInitialized(aura::Window* new_window) {
  if (arc_window_delegate_->IsArcWindow(new_window)) {
    if (!is_focus_observer_installed_) {
      arc_window_delegate_->RegisterFocusObserver();
      is_focus_observer_installed_ = true;
    }
  }
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller && keyboard_controller_ != keyboard_controller) {
    // Registering |this| as an observer only once per KeyboardController.
    keyboard_controller_ = keyboard_controller;
    keyboard_controller_->AddObserver(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from aura::WindowObserver:

void ArcImeService::OnWindowDestroying(aura::Window* window) {
  // This shouldn't be reached on production, since the window lost the focus
  // and called OnWindowFocused() before destroying.
  // But we handle this case for testing.
  DCHECK_EQ(window, focused_arc_window_);
  OnWindowFocused(nullptr, focused_arc_window_);
}

void ArcImeService::OnWindowRemovingFromRootWindow(aura::Window* window,
                                                   aura::Window* new_root) {
  DCHECK_EQ(window, focused_arc_window_);
  // IMEs are associated with root windows, hence we may need to detach/attach.
  ReattachInputMethod(focused_arc_window_, new_root);
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from exo::WMHelper::FocusChangeObserver:

void ArcImeService::OnWindowFocused(aura::Window* gained_focus,
                                    aura::Window* lost_focus) {
  if (lost_focus == gained_focus)
    return;

  const bool detach = (lost_focus && focused_arc_window_ == lost_focus);
  const bool attach =
      (gained_focus && arc_window_delegate_->IsArcWindow(gained_focus));

  if (detach) {
    focused_arc_window_->RemoveObserver(this);
    focused_arc_window_ = nullptr;
  }
  if (attach) {
    DCHECK_EQ(nullptr, focused_arc_window_);
    focused_arc_window_ = gained_focus;
    focused_arc_window_->AddObserver(this);
  }

  ReattachInputMethod(detach ? lost_focus : nullptr, focused_arc_window_);
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from arc::ArcImeBridge::Delegate

void ArcImeService::OnTextInputTypeChanged(ui::TextInputType type) {
  if (ime_type_ == type)
    return;
  ime_type_ = type;

  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->OnTextInputTypeChanged(this);
}

void ArcImeService::OnCursorRectChanged(const gfx::Rect& rect) {
  InvalidateSurroundingTextAndSelectionRange();
  if (cursor_rect_ == rect)
    return;
  cursor_rect_ = rect;

  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->OnCaretBoundsChanged(this);
}

void ArcImeService::OnCancelComposition() {
  InvalidateSurroundingTextAndSelectionRange();
  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->CancelComposition(this);
}

void ArcImeService::ShowImeIfNeeded() {
  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method && input_method->GetTextInputClient() == this) {
    input_method->ShowImeIfNeeded();
  }
}

void ArcImeService::OnCursorRectChangedWithSurroundingText(
    const gfx::Rect& rect,
    const gfx::Range& text_range,
    const base::string16& text_in_range,
    const gfx::Range& selection_range) {
  text_range_ = text_range;
  text_in_range_ = text_in_range;
  selection_range_ = selection_range;

  if (cursor_rect_ == rect)
    return;
  cursor_rect_ = rect;

  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->OnCaretBoundsChanged(this);
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from keyboard::KeyboardControllerObserver
void ArcImeService::OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) {
  if (!focused_arc_window_)
    return;
  aura::Window* window = focused_arc_window_;
  // Multiply by the scale factor. To convert from DPI to physical pixels.
  gfx::Rect bounds_in_px = gfx::ScaleToEnclosingRect(
      new_bounds, window->layer()->device_scale_factor());
  ime_bridge_->SendOnKeyboardBoundsChanging(bounds_in_px);
}

void ArcImeService::OnKeyboardClosed() {}

////////////////////////////////////////////////////////////////////////////////
// Overridden from ui::TextInputClient:

void ArcImeService::SetCompositionText(
    const ui::CompositionText& composition) {
  InvalidateSurroundingTextAndSelectionRange();
  has_composition_text_ = !composition.text.empty();
  ime_bridge_->SendSetCompositionText(composition);
}

void ArcImeService::ConfirmCompositionText() {
  InvalidateSurroundingTextAndSelectionRange();
  has_composition_text_ = false;
  ime_bridge_->SendConfirmCompositionText();
}

void ArcImeService::ClearCompositionText() {
  InvalidateSurroundingTextAndSelectionRange();
  if (has_composition_text_) {
    has_composition_text_ = false;
    ime_bridge_->SendInsertText(base::string16());
  }
}

void ArcImeService::InsertText(const base::string16& text) {
  InvalidateSurroundingTextAndSelectionRange();
  has_composition_text_ = false;
  ime_bridge_->SendInsertText(text);
}

void ArcImeService::InsertChar(const ui::KeyEvent& event) {
  // According to the document in text_input_client.h, InsertChar() is called
  // even when the text input type is NONE. We ignore such events, since for
  // ARC we are only interested in the event as a method of text input.
  if (ime_type_ == ui::TEXT_INPUT_TYPE_NONE)
    return;

  InvalidateSurroundingTextAndSelectionRange();

  // For apps that doesn't handle hardware keyboard events well, keys that are
  // typically on software keyboard and lack of them are fatal, namely,
  // unmodified enter and backspace keys are sent through IME.
  constexpr int kModifierMask = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                                ui::EF_ALTGR_DOWN | ui::EF_MOD3_DOWN;
  if ((event.flags() & kModifierMask) == 0) {
    if (event.key_code() ==  ui::VKEY_RETURN) {
      has_composition_text_ = false;
      ime_bridge_->SendInsertText(base::ASCIIToUTF16("\n"));
      return;
    }
    if (event.key_code() ==  ui::VKEY_BACK) {
      has_composition_text_ = false;
      ime_bridge_->SendInsertText(base::ASCIIToUTF16("\b"));
      return;
    }
  }

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
  if (!focused_arc_window_)
    return gfx::Rect();
  aura::Window* window = focused_arc_window_;

  // |cursor_rect_| holds the rectangle reported from ARC apps, in the "screen
  // coordinates" in ARC, counted by physical pixels.
  // Chrome OS input methods expect the coordinates in Chrome OS screen, within
  // device independent pixels. Two factors are involved for the conversion.

  // Divide by the scale factor. To convert from physical pixels to DIP.
  gfx::Rect converted = gfx::ScaleToEnclosingRect(
      cursor_rect_, 1 / window->layer()->device_scale_factor());

  // Add the offset of the window showing the ARC app.
  // TODO(yoshiki): Support for non-arc toplevel window. The following code do
  // not work correctly with arc windows inside non-arc toplevel window (eg.
  // notification).
  converted.Offset(
      window->GetToplevelWindow()->GetBoundsInScreen().OffsetFromOrigin());
  return converted;
}

bool ArcImeService::GetTextRange(gfx::Range* range) const {
  if (!text_range_.IsValid())
    return false;
  *range = text_range_;
  return true;
}

bool ArcImeService::GetSelectionRange(gfx::Range* range) const {
  if (!selection_range_.IsValid())
    return false;
  *range = selection_range_;
  return true;
}

bool ArcImeService::GetTextFromRange(const gfx::Range& range,
                                     base::string16* text) const {
  // It's supposed that this method is called only from
  // InputMethod::OnCaretBoundsChanged(). In that method, the range obtained
  // from GetTextRange() is used as the argument of this method. To prevent an
  // unexpected usage, the check, |range != text_range_|, is added.
  if (!text_range_.IsValid() || range != text_range_)
    return false;
  *text = text_in_range_;
  return true;
}

ui::TextInputMode ArcImeService::GetTextInputMode() const {
  return ui::TEXT_INPUT_MODE_DEFAULT;
}

base::i18n::TextDirection ArcImeService::GetTextDirection() const {
  return base::i18n::UNKNOWN_DIRECTION;
}

void ArcImeService::ExtendSelectionAndDelete(size_t before, size_t after) {
  InvalidateSurroundingTextAndSelectionRange();
  ime_bridge_->SendExtendSelectionAndDelete(before, after);
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

bool ArcImeService::GetCompositionTextRange(gfx::Range* range) const {
  return false;
}

bool ArcImeService::SetSelectionRange(const gfx::Range& range) {
  return false;
}

bool ArcImeService::DeleteRange(const gfx::Range& range) {
  return false;
}

bool ArcImeService::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  return false;
}

bool ArcImeService::IsTextEditCommandEnabled(
    ui::TextEditCommand command) const {
  return false;
}

const std::string& ArcImeService::GetClientSourceInfo() const {
  // TODO(yhanada): Implement this method. crbug.com/752657
  NOTIMPLEMENTED_LOG_ONCE();
  return base::EmptyString();
}

void ArcImeService::InvalidateSurroundingTextAndSelectionRange() {
  text_range_ = gfx::Range::InvalidRange();
  text_in_range_ = base::string16();
  selection_range_ = gfx::Range::InvalidRange();
}

}  // namespace arc
