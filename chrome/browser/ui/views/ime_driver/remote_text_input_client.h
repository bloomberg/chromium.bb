// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IME_DRIVER_REMOTE_TEXT_INPUT_CLIENT_H_
#define CHROME_BROWSER_UI_VIEWS_IME_DRIVER_REMOTE_TEXT_INPUT_CLIENT_H_

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "services/ws/public/mojom/ime/ime.mojom.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/text_input_client.h"

// This implementation of ui::TextInputClient sends all updates via mojo IPC to
// a remote client. This is intended to be passed to the overrides of
// ui::InputMethod::SetFocusedTextInputClient().
// NOTE: Under SingleProcessMash this is used by ash code, for example by the
// virtual keyboard controller in //ui/keyboard.
class RemoteTextInputClient : public ui::TextInputClient,
                              public ui::internal::InputMethodDelegate {
 public:
  RemoteTextInputClient(ws::mojom::TextInputClientPtr client,
                        ws::mojom::SessionDetailsPtr details);
  ~RemoteTextInputClient() override;

  void SetTextInputState(ws::mojom::TextInputStatePtr text_input_state);
  void SetCaretBounds(const gfx::Rect& caret_bounds);
  void SetTextInputClientData(ws::mojom::TextInputClientDataPtr data);

 private:
  struct QueuedEvent;

  // See |pending_callbacks_| for details.
  void OnDispatchKeyEventPostIMECompleted(bool handled,
                                          bool stopped_propagation);

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
  FocusReason GetFocusReason() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool GetEditableSelectionRange(gfx::Range* range) const override;
  bool SetEditableSelectionRange(const gfx::Range& range) override;
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
  ukm::SourceId GetClientSourceForMetrics() const override;
  bool ShouldDoLearning() override;
  bool SetCompositionFromExistingText(
      const gfx::Range& range,
      const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) override;

  // ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event,
      DispatchKeyEventPostIMECallback callback) override;

  // Dispatches the first queued event.
  void DispatchQueuedEvent();

  // Removes the queue event at the front of |queued_events_| and runs its
  // callback with |handled| and |stopped_propagation| as arguments.
  void RunNextPendingCallback(bool handled, bool stopped_propagation);

  ws::mojom::TextInputClientPtr remote_client_;
  ws::mojom::SessionDetailsPtr details_;

  // Events to be dispatched with DispatchKeyEventPostIME(). Only one event is
  // dispatched at a time and others are queued here until the current one
  // finished processing, i.e. the response from the remote side is received
  // (OnDispatchKeyEventPostIMECompleted()), the dispatched event is removed
  // from the queue and its callback is invoked. This is done to avoid
  // overlapping of key events processing (https://crbug.com/938808).
  // Note that when we are destroyed all the callbacks needs to run. This is
  // necessary as the callbacks may have originated from a remote client.
  base::queue<QueuedEvent> queued_events_;

  base::WeakPtrFactory<RemoteTextInputClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RemoteTextInputClient);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IME_DRIVER_REMOTE_TEXT_INPUT_CLIENT_H_
