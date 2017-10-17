// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_WIDGET_INPUT_HANDLER_H_
#define CONTENT_TEST_MOCK_WIDGET_INPUT_HANDLER_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "content/common/input/input_handler.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {

class MockWidgetInputHandler : public mojom::WidgetInputHandler {
 public:
  MockWidgetInputHandler();
  MockWidgetInputHandler(mojom::WidgetInputHandlerRequest request,
                         mojom::WidgetInputHandlerHostPtr host);

  ~MockWidgetInputHandler() override;

  class DispatchedEditCommandMessage;
  class DispatchedEventMessage;
  class DispatchedIMEMessage;

  // Abstract storage of a received call on the MockWidgetInputHandler
  // interface.
  class DispatchedMessage {
   public:
    explicit DispatchedMessage(const std::string& name);
    virtual ~DispatchedMessage();

    // Cast this to an DispatchedEditCommandMessage if it is one, null
    // otherwise.
    virtual DispatchedEditCommandMessage* ToEditCommand();

    // Cast this to an DispatchedEventMessage if it is one, null otherwise.
    virtual DispatchedEventMessage* ToEvent();

    // Cast this to an DispatchedIMEMessage if it is one, null otherwise.
    virtual DispatchedIMEMessage* ToIME();

    // Return the name associated with this message. It will either match
    // the message call name (eg. MouseCaptureLost) or the name of an
    // input event (eg. GestureScrollBegin).
    const std::string& name() const { return name_; }

   private:
    std::string name_;

    DISALLOW_COPY_AND_ASSIGN(DispatchedMessage);
  };

  // A DispatchedMessage that stores the IME compositing parameters
  // that were invoked with.
  class DispatchedIMEMessage : public DispatchedMessage {
   public:
    DispatchedIMEMessage(const std::string& name,
                         const base::string16& text,
                         const std::vector<ui::ImeTextSpan>& ime_text_spans,
                         const gfx::Range& range,
                         int32_t start,
                         int32_t end);
    ~DispatchedIMEMessage() override;

    // Override and return |this|.
    DispatchedIMEMessage* ToIME() override;

    // Returns if this message matches the parameters passed in.
    bool Matches(const base::string16& text,
                 const std::vector<ui::ImeTextSpan>& ime_text_spans,
                 const gfx::Range& range,
                 int32_t start,
                 int32_t end) const;

   private:
    base::string16 text_;
    std::vector<ui::ImeTextSpan> text_spans_;
    gfx::Range range_;
    int32_t start_;
    int32_t end_;

    DISALLOW_COPY_AND_ASSIGN(DispatchedIMEMessage);
  };

  // A DispatchedMessage that stores the IME compositing parameters
  // that were invoked with.
  class DispatchedEditCommandMessage : public DispatchedMessage {
   public:
    explicit DispatchedEditCommandMessage(
        const std::vector<content::EditCommand>& commands);
    ~DispatchedEditCommandMessage() override;

    // Override and return |this|.
    DispatchedEditCommandMessage* ToEditCommand() override;

    const std::vector<content::EditCommand>& Commands() const;

   private:
    std::vector<content::EditCommand> commands_;

    DISALLOW_COPY_AND_ASSIGN(DispatchedEditCommandMessage);
  };

  // A DispatchedMessage that stores the InputEvent and callback
  // that was passed to the MockWidgetInputHandler interface.
  class DispatchedEventMessage : public DispatchedMessage {
   public:
    DispatchedEventMessage(std::unique_ptr<content::InputEvent> event,
                           DispatchEventCallback callback);
    ~DispatchedEventMessage() override;

    // Override and return |this|.
    DispatchedEventMessage* ToEvent() override;

    // Invoke the callback on this object with the passed in |state|.
    // The callback is called with default values for the other fields.
    void CallCallback(InputEventAckState state);

    // Invoke a callback with all the arguments provided.
    void CallCallback(InputEventAckSource source,
                      const ui::LatencyInfo& latency_info,
                      InputEventAckState state,
                      const base::Optional<ui::DidOverscrollParams>& overscroll,
                      const base::Optional<cc::TouchAction>& touch_action);

    // Return if the callback is set.
    bool HasCallback() const;

    // Return the associated event.
    const content::InputEvent* Event() const;

   private:
    std::unique_ptr<content::InputEvent> event_;
    DispatchEventCallback callback_;

    DISALLOW_COPY_AND_ASSIGN(DispatchedEventMessage);
  };

  // mojom::WidgetInputHandler override.
  void SetFocus(bool focused) override;
  void MouseCaptureLost() override;
  void SetEditCommandsForNextKeyEvent(
      const std::vector<content::EditCommand>& commands) override;
  void CursorVisibilityChanged(bool visible) override;
  void ImeSetComposition(const base::string16& text,
                         const std::vector<ui::ImeTextSpan>& ime_text_spans,
                         const gfx::Range& range,
                         int32_t start,
                         int32_t end) override;
  void ImeCommitText(const base::string16& text,
                     const std::vector<ui::ImeTextSpan>& ime_text_spans,
                     const gfx::Range& range,
                     int32_t relative_cursor_position) override;
  void ImeFinishComposingText(bool keep_selection) override;
  void RequestTextInputStateUpdate() override;
  void RequestCompositionUpdates(bool immediate_request,
                                 bool monitor_request) override;

  void DispatchEvent(std::unique_ptr<content::InputEvent> event,
                     DispatchEventCallback callback) override;
  void DispatchNonBlockingEvent(
      std::unique_ptr<content::InputEvent> event) override;

  using MessageVector = std::vector<std::unique_ptr<DispatchedMessage>>;
  MessageVector GetAndResetDispatchedMessages();

 private:
  mojo::Binding<mojom::WidgetInputHandler> binding_;
  mojom::WidgetInputHandlerHostPtr host_ = nullptr;
  MessageVector dispatched_messages_;

  DISALLOW_COPY_AND_ASSIGN(MockWidgetInputHandler);
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_INPUT_ACK_HANDLER_H_
