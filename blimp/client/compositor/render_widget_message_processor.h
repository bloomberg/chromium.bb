// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_COMPOSITOR_RENDER_WIDGET_MESSAGE_PROCESSOR_H_
#define BLIMP_CLIENT_COMPOSITOR_RENDER_WIDGET_MESSAGE_PROCESSOR_H_

#include "base/containers/small_map.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/client/blimp_client_export.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/input_message_generator.h"

namespace blink {
class WebInputEvent;
}

namespace cc {
namespace proto {
class CompositorMessage;
}
}

namespace blimp {

// Handles all incoming and outgoing protobuf message types tied to a specific
// RenderWidget. This includes BlimpMessage::INPUT, BlimpMessage::COMPOSITOR,
// and BlimpMessage::RENDER_WIDGET messages.  Delegates can be added to be
// notified of incoming messages. This class automatically attaches a specific
// id so that the engine can drop stale RenderWidget related messages after it
// sends a RenderWidgetMessage::INITIALIZE message.
class BLIMP_CLIENT_EXPORT RenderWidgetMessageProcessor
    : public BlimpMessageProcessor {
 public:
  // A delegate to be notified of specific RenderWidget related incoming events.
  class RenderWidgetMessageDelegate {
   public:
    // Called when the engine's RenderWidget has changed for a particular
    // WebContents.  When this is received all state related to the existing
    // client RenderWidget should be reset.
    virtual void OnRenderWidgetInitialized() = 0;

    // Called when the engine sent a CompositorMessage.  These messages should
    // be sent to the client's RemoteChannel of the compositor.
    virtual void OnCompositorMessageReceived(
        scoped_ptr<cc::proto::CompositorMessage> message) = 0;
  };

  RenderWidgetMessageProcessor(
      BlimpMessageProcessor* input_message_processor,
      BlimpMessageProcessor* compositor_message_processor);
  ~RenderWidgetMessageProcessor() override;

  // Sends a WebInputEvent for |tab_id| to the engine.
  void SendInputEvent(const int tab_id, const blink::WebInputEvent& event);

  // Sends a CompositorMessage for |tab_id| to the engine.
  void SendCompositorMessage(const int tab_id,
                             const cc::proto::CompositorMessage& message);

  // Sets a RenderWidgetMessageDelegate to be notified of all incoming
  // RenderWidget related messages for |tab_id| from the engine.  There can only
  // be one RenderWidgetMessageDelegate per tab.
  void SetDelegate(const int tab_id, RenderWidgetMessageDelegate* delegate);
  void RemoveDelegate(const int tab_id);

  // BlimpMessageProcessor implementation.
  void ProcessMessage(scoped_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

 private:
  // Returns nullptr if no delegate is found.
  RenderWidgetMessageDelegate* FindDelegate(const int tab_id);

  // Returns 0 if no id is found.
  uint32_t GetRenderWidgetId(const int tab_id);

  typedef base::SmallMap<std::map<int, RenderWidgetMessageDelegate*> >
      DelegateMap;
  typedef base::SmallMap<std::map<int, uint32_t> > RenderWidgetIdMap;

  DelegateMap delegates_;
  RenderWidgetIdMap render_widget_ids_;

  InputMessageGenerator input_message_generator_;

  BlimpMessageProcessor* input_message_processor_;
  BlimpMessageProcessor* compositor_message_processor_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetMessageProcessor);
};

}  // namespace blimp

#endif  // BLIMP_CLIENT_COMPOSITOR_RENDER_WIDGET_MESSAGE_PROCESSOR_H_
