// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ipc/ipc_message_attachment.h"
#include "ipc/ipc_message_attachment_set.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#include "ipc/ipc_platform_file_attachment_posix.h"
#endif

namespace {

base::StaticAtomicSequenceNumber g_ref_num;

// Create a reference number for identifying IPC messages in traces. The return
// values has the reference number stored in the upper 24 bits, leaving the low
// 8 bits set to 0 for use as flags.
inline uint32 GetRefNumUpper24() {
  base::trace_event::TraceLog* trace_log =
      base::trace_event::TraceLog::GetInstance();
  uint32 pid = trace_log ? trace_log->process_id() : 0;
  uint32 count = g_ref_num.GetNext();
  // The 24 bit hash is composed of 14 bits of the count and 10 bits of the
  // Process ID. With the current trace event buffer cap, the 14-bit count did
  // not appear to wrap during a trace. Note that it is not a big deal if
  // collisions occur, as this is only used for debugging and trace analysis.
  return ((pid << 14) | (count & 0x3fff)) << 8;
}

}  // namespace

namespace IPC {

//------------------------------------------------------------------------------

Message::~Message() {
}

Message::Message()
    : Pickle(sizeof(Header)) {
  header()->routing = header()->type = 0;
  header()->flags = GetRefNumUpper24();
#if defined(OS_POSIX)
  header()->num_fds = 0;
  header()->pad = 0;
#endif
  Init();
}

Message::Message(int32 routing_id, uint32 type, PriorityValue priority)
    : Pickle(sizeof(Header)) {
  header()->routing = routing_id;
  header()->type = type;
  DCHECK((priority & 0xffffff00) == 0);
  header()->flags = priority | GetRefNumUpper24();
#if defined(OS_POSIX)
  header()->num_fds = 0;
  header()->pad = 0;
#endif
  Init();
}

Message::Message(const char* data, int data_len) : Pickle(data, data_len) {
  Init();
}

Message::Message(const Message& other) : Pickle(other) {
  Init();
#if defined(OS_POSIX)
  attachment_set_ = other.attachment_set_;
#endif
}

void Message::Init() {
  dispatch_error_ = false;
#ifdef IPC_MESSAGE_LOG_ENABLED
  received_time_ = 0;
  dont_log_ = false;
  log_data_ = NULL;
#endif
}

Message& Message::operator=(const Message& other) {
  *static_cast<Pickle*>(this) = other;
#if defined(OS_POSIX)
  attachment_set_ = other.attachment_set_;
#endif
  return *this;
}

void Message::SetHeaderValues(int32 routing, uint32 type, uint32 flags) {
  // This should only be called when the message is already empty.
  DCHECK(payload_size() == 0);

  header()->routing = routing;
  header()->type = type;
  header()->flags = flags;
}

void Message::EnsureMessageAttachmentSet() {
  if (attachment_set_.get() == NULL)
    attachment_set_ = new MessageAttachmentSet;
}

#ifdef IPC_MESSAGE_LOG_ENABLED
void Message::set_sent_time(int64 time) {
  DCHECK((header()->flags & HAS_SENT_TIME_BIT) == 0);
  header()->flags |= HAS_SENT_TIME_BIT;
  WriteInt64(time);
}

int64 Message::sent_time() const {
  if ((header()->flags & HAS_SENT_TIME_BIT) == 0)
    return 0;

  const char* data = end_of_payload();
  data -= sizeof(int64);
  return *(reinterpret_cast<const int64*>(data));
}

void Message::set_received_time(int64 time) const {
  received_time_ = time;
}
#endif

bool Message::WriteAttachment(scoped_refptr<MessageAttachment> attachment) {
  // We write the index of the descriptor so that we don't have to
  // keep the current descriptor as extra decoding state when deserialising.
  WriteInt(attachment_set()->size());
  return attachment_set()->AddAttachment(attachment);
}

bool Message::ReadAttachment(
    PickleIterator* iter,
    scoped_refptr<MessageAttachment>* attachment) const {
  int descriptor_index;
  if (!iter->ReadInt(&descriptor_index))
    return false;

  MessageAttachmentSet* attachment_set = attachment_set_.get();
  if (!attachment_set)
    return false;

  *attachment = attachment_set->GetAttachmentAt(descriptor_index);
  return nullptr != attachment->get();
}

bool Message::HasAttachments() const {
  return attachment_set_.get() && !attachment_set_->empty();
}

bool Message::HasMojoHandles() const {
  return attachment_set_.get() && 0 < attachment_set_->num_mojo_handles();
}

}  // namespace IPC
