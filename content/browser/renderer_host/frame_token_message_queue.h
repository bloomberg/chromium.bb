// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_FRAME_TOKEN_MESSAGE_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_FRAME_TOKEN_MESSAGE_QUEUE_H_

#include <vector>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "content/common/content_export.h"

namespace IPC {
class Message;
}  // namespace IPC

namespace content {

// The Renderer sends various IPC::Messages which are not to be processed until
// after their associated frame has been processed. These messages are provided
// with a FrameToken to be used for synchronizing.
//
// Viz processes the frames, after which it notifies the Browser of which
// FrameToken has completed processing.
//
// This enqueues all IPC::Messages associated with a FrameToken.
//
// Upon receipt of DidProcessFrame all IPC::Messages associated with the
// provided FrameToken are then dispatched.
class CONTENT_EXPORT FrameTokenMessageQueue {
 public:
  // Notified of errors in processing messages, as well as of the actual
  // enqueued IPC::Messages which need processing.
  class Client {
   public:
    ~Client() {}

    // Notified when an invalid frame token was received.
    virtual void OnInvalidFrameToken(uint32_t frame_token) = 0;

    // Called when there are dispatching errors in OnMessageReceived().
    virtual void OnMessageDispatchError(const IPC::Message& message) = 0;

    // Process the enqueued message.
    virtual void OnProcessSwapMessage(const IPC::Message& message) = 0;
  };
  explicit FrameTokenMessageQueue(Client* client);
  virtual ~FrameTokenMessageQueue();

  // Signals that a frame with token |frame_token| was finished processing. If
  // there are any queued messages belonging to it, they will be processed.
  void DidProcessFrame(uint32_t frame_token);

  // Enqueues the swap messages.
  void OnFrameSwapMessagesReceived(uint32_t frame_token,
                                   std::vector<IPC::Message> messages);

  // Called when the renderer process is gone. This will reset our state to be
  // consistent incase a new renderer is created.
  void Reset();

  uint32_t size() const { return queued_messages_.size(); }

 protected:
  // Once both the frame and its swap messages arrive, we call this method to
  // process the messages. Virtual for tests.
  virtual void ProcessSwapMessages(std::vector<IPC::Message> messages);

 private:
  // Not owned.
  Client* const client_;

  // Last non-zero frame token received from the renderer. Any swap messsages
  // having a token less than or equal to this value will be processed.
  uint32_t last_received_frame_token_ = 0;

  // List of all swap messages that their corresponding frames have not arrived.
  // Sorted by frame token.
  base::queue<std::pair<uint32_t, std::vector<IPC::Message>>> queued_messages_;

  DISALLOW_COPY_AND_ASSIGN(FrameTokenMessageQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_FRAME_TOKEN_MESSAGE_QUEUE_H_
