// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IME_DRIVER_REMOTE_TEXT_INPUT_CLIENT_H_
#define CHROME_BROWSER_UI_VIEWS_IME_DRIVER_REMOTE_TEXT_INPUT_CLIENT_H_

#include "services/ui/public/interfaces/ime/ime.mojom.h"
#include "ui/base/ime/chromeos/ime_candidate_window_handler_interface.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/text_input_client.h"

// This implementation of ui::TextInputClient sends all updates via mojo IPC to
// a remote client. This is intended to be passed to the overrides of
// ui::InputMethod::SetFocusedTextInputClient().
class RemoteTextInputClient : public ui::TextInputClient,
                              public ui::internal::InputMethodDelegate,
                              chromeos::IMECandidateWindowHandlerInterface {
 public:
  RemoteTextInputClient(ui::mojom::TextInputClientPtr remote_client,
                        ui::TextInputType text_input_type,
                        ui::TextInputMode text_input_mode,
                        base::i18n::TextDirection text_direction,
                        int text_input_flags,
                        gfx::Rect caret_bounds);
  ~RemoteTextInputClient() override;

  void SetTextInputType(ui::TextInputType text_input_type);
  void SetCaretBounds(const gfx::Rect& caret_bounds);

 private:
  // ui::TextInputClient:
  void SetCompositionText(const ui::CompositionText& composition) override;
  void ConfirmCompositionText() override;
  void ClearCompositionText() override;
  void InsertText(const base::string16& text) override;
  void InsertChar(const ui::KeyEvent& event) override;
  ui::TextInputType GetTextInputType() const override;
  ui::TextInputMode GetTextInputMode() const override;
  base::i18n::TextDirection GetTextDirection() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  gfx::Rect GetCaretBounds() const override;
  bool GetCompositionCharacterBounds(uint32_t index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool GetSelectionRange(gfx::Range* range) const override;
  bool SetSelectionRange(const gfx::Range& range) override;
  bool DeleteRange(const gfx::Range& range) override;
  bool GetTextFromRange(const gfx::Range& range,
                        base::string16* text) const override;
  void OnInputMethodChanged() override;
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override;
  void EnsureCaretNotInRect(const gfx::Rect& rect) override;
  bool IsTextEditCommandEnabled(ui::TextEditCommand command) const override;
  void SetTextEditCommandForNextKeyEvent(ui::TextEditCommand command) override;

  // ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) override;

  // chromeos::IMECandidateWindowHandlerInterface:
  void UpdateLookupTable(const ui::CandidateWindow& candidate_window,
                         bool visible) override;
  void UpdatePreeditText(const base::string16& text,
                         uint32_t cursor_pos,
                         bool visible) override;
  void SetCursorBounds(const gfx::Rect& cursor_bounds,
                       const gfx::Rect& composition_head) override;
  void OnCandidateWindowVisibilityChanged(bool visible) override;

  ui::mojom::TextInputClientPtr remote_client_;
  ui::TextInputType text_input_type_;
  ui::TextInputMode text_input_mode_;
  base::i18n::TextDirection text_direction_;
  int text_input_flags_;
  gfx::Rect caret_bounds_;
  std::deque<std::unique_ptr<base::OnceCallback<void(bool)>>>
      pending_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(RemoteTextInputClient);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IME_DRIVER_REMOTE_TEXT_INPUT_CLIENT_H_
