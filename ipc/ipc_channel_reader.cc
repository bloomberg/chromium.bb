// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_reader.h"

#include <algorithm>

#include "ipc/ipc_listener.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_attachment_set.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"

namespace IPC {
namespace internal {

ChannelReader::ChannelReader(Listener* listener) : listener_(listener) {
  memset(input_buf_, 0, sizeof(input_buf_));
}

ChannelReader::~ChannelReader() {
  DCHECK(blocked_ids_.empty());
}

ChannelReader::DispatchState ChannelReader::ProcessIncomingMessages() {
  while (true) {
    int bytes_read = 0;
    ReadState read_state = ReadData(input_buf_, Channel::kReadBufferSize,
                                    &bytes_read);
    if (read_state == READ_FAILED)
      return DISPATCH_ERROR;
    if (read_state == READ_PENDING)
      return DISPATCH_FINISHED;

    DCHECK(bytes_read > 0);
    if (!TranslateInputData(input_buf_, bytes_read))
      return DISPATCH_ERROR;

    DispatchState state = DispatchMessages();
    if (state != DISPATCH_FINISHED)
      return state;
  }
}

ChannelReader::DispatchState ChannelReader::AsyncReadComplete(int bytes_read) {
  if (!TranslateInputData(input_buf_, bytes_read))
    return DISPATCH_ERROR;

  return DispatchMessages();
}

bool ChannelReader::IsInternalMessage(const Message& m) {
  return m.routing_id() == MSG_ROUTING_NONE &&
      m.type() >= Channel::CLOSE_FD_MESSAGE_TYPE &&
      m.type() <= Channel::HELLO_MESSAGE_TYPE;
}

bool ChannelReader::IsHelloMessage(const Message& m) {
  return m.routing_id() == MSG_ROUTING_NONE &&
      m.type() == Channel::HELLO_MESSAGE_TYPE;
}

void ChannelReader::CleanUp() {
  if (!blocked_ids_.empty()) {
    StopObservingAttachmentBroker();
    blocked_ids_.clear();
  }
}

bool ChannelReader::TranslateInputData(const char* input_data,
                                       int input_data_len) {
  const char* p;
  const char* end;

  // Possibly combine with the overflow buffer to make a larger buffer.
  if (input_overflow_buf_.empty()) {
    p = input_data;
    end = input_data + input_data_len;
  } else {
    if (input_overflow_buf_.size() + input_data_len >
        Channel::kMaximumMessageSize) {
      input_overflow_buf_.clear();
      LOG(ERROR) << "IPC message is too big";
      return false;
    }
    input_overflow_buf_.append(input_data, input_data_len);
    p = input_overflow_buf_.data();
    end = p + input_overflow_buf_.size();
  }

  // Dispatch all complete messages in the data buffer.
  while (p < end) {
    Message::NextMessageInfo info;
    Message::FindNext(p, end, &info);
    if (info.message_found) {
      int pickle_len = static_cast<int>(info.pickle_end - p);
      Message translated_message(p, pickle_len);

      for (const auto& id : info.attachment_ids)
        translated_message.AddPlaceholderBrokerableAttachmentWithId(id);

      if (!GetNonBrokeredAttachments(&translated_message))
        return false;

      // If there are no queued messages, attempt to immediately dispatch the
      // newly translated message.
      if (queued_messages_.empty()) {
        DCHECK(blocked_ids_.empty());
        AttachmentIdSet blocked_ids =
            GetBrokeredAttachments(&translated_message);

        if (blocked_ids.empty()) {
          // Dispatch the message and continue the loop.
          DispatchMessage(&translated_message);
          p = info.message_end;
          continue;
        }

        blocked_ids_.swap(blocked_ids);
        StartObservingAttachmentBroker();
      }

      // Make a deep copy of |translated_message| to add to the queue.
      scoped_ptr<Message> m(new Message(translated_message));
      queued_messages_.push_back(m.release());
      p = info.message_end;
    } else {
      // Last message is partial.
      break;
    }
  }

  // Save any partial data in the overflow buffer.
  input_overflow_buf_.assign(p, end - p);

  if (input_overflow_buf_.empty() && !DidEmptyInputBuffers())
    return false;
  return true;
}

ChannelReader::DispatchState ChannelReader::DispatchMessages() {
  while (!queued_messages_.empty()) {
    if (!blocked_ids_.empty())
      return DISPATCH_WAITING_ON_BROKER;

    Message* m = queued_messages_.front();

    AttachmentIdSet blocked_ids = GetBrokeredAttachments(m);
    if (!blocked_ids.empty()) {
      blocked_ids_.swap(blocked_ids);
      StartObservingAttachmentBroker();
      return DISPATCH_WAITING_ON_BROKER;
    }

    DispatchMessage(m);
    queued_messages_.erase(queued_messages_.begin());
  }
  return DISPATCH_FINISHED;
}

void ChannelReader::DispatchMessage(Message* m) {
  m->set_sender_pid(GetSenderPID());

#ifdef IPC_MESSAGE_LOG_ENABLED
  std::string name;
  Logging::GetInstance()->GetMessageText(m->type(), &name, m, NULL);
  TRACE_EVENT_WITH_FLOW1("ipc,toplevel",
                         "ChannelReader::DispatchInputData",
                         m->flags(),
                         TRACE_EVENT_FLAG_FLOW_IN,
                         "name", name);
#else
  TRACE_EVENT_WITH_FLOW2("ipc,toplevel",
                         "ChannelReader::DispatchInputData",
                         m->flags(),
                         TRACE_EVENT_FLAG_FLOW_IN,
                         "class", IPC_MESSAGE_ID_CLASS(m->type()),
                         "line", IPC_MESSAGE_ID_LINE(m->type()));
#endif

  bool handled = false;
  if (IsInternalMessage(*m)) {
    HandleInternalMessage(*m);
    handled = true;
  }
#if USE_ATTACHMENT_BROKER
  if (!handled && IsAttachmentBrokerEndpoint() && GetAttachmentBroker()) {
    handled = GetAttachmentBroker()->OnMessageReceived(*m);
  }
#endif  // USE_ATTACHMENT_BROKER

  // TODO(erikchen): Temporary code to help track http://crbug.com/527588.
  Channel::MessageVerifier verifier = Channel::GetMessageVerifier();
  if (verifier)
    verifier(m);

  if (!handled)
    listener_->OnMessageReceived(*m);
  if (m->dispatch_error())
    listener_->OnBadMessageReceived(*m);
}

ChannelReader::AttachmentIdSet ChannelReader::GetBrokeredAttachments(
    Message* msg) {
  std::set<BrokerableAttachment::AttachmentId> blocked_ids;

#if USE_ATTACHMENT_BROKER
  MessageAttachmentSet* set = msg->attachment_set();
  std::vector<const BrokerableAttachment*> brokerable_attachments_copy =
      set->PeekBrokerableAttachments();
  for (const BrokerableAttachment* attachment : brokerable_attachments_copy) {
    if (attachment->NeedsBrokering()) {
      AttachmentBroker* broker = GetAttachmentBroker();
      scoped_refptr<BrokerableAttachment> brokered_attachment;
      bool result = broker->GetAttachmentWithId(attachment->GetIdentifier(),
                                                &brokered_attachment);
      if (!result) {
        blocked_ids.insert(attachment->GetIdentifier());
        continue;
      }

      set->ReplacePlaceholderWithAttachment(brokered_attachment);
    }
  }
#endif  // USE_ATTACHMENT_BROKER

  return blocked_ids;
}

void ChannelReader::ReceivedBrokerableAttachmentWithId(
    const BrokerableAttachment::AttachmentId& id) {
  if (blocked_ids_.empty())
    return;

  auto it = find(blocked_ids_.begin(), blocked_ids_.end(), id);
  if (it != blocked_ids_.end())
    blocked_ids_.erase(it);

  if (blocked_ids_.empty()) {
    StopObservingAttachmentBroker();
    DispatchMessages();
  }
}

void ChannelReader::StartObservingAttachmentBroker() {
#if USE_ATTACHMENT_BROKER
  GetAttachmentBroker()->AddObserver(this);
#endif  // USE_ATTACHMENT_BROKER
}

void ChannelReader::StopObservingAttachmentBroker() {
#if USE_ATTACHMENT_BROKER
  GetAttachmentBroker()->RemoveObserver(this);
#endif  // USE_ATTACHMENT_BROKER
}

}  // namespace internal
}  // namespace IPC
