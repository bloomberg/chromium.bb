// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/db_message_filter.h"

#include "chrome/common/child_process.h"
#include "chrome/common/render_messages.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDatabase.h"

DBMessageFilter* DBMessageFilter::instance_ = NULL;

DBMessageFilter::DBMessageFilter()
    : io_thread_message_loop_(ChildProcess::current()->io_message_loop()),
      channel_(NULL),
      channel_lock_(new Lock()),
      shutdown_event_(ChildProcess::current()->GetShutDownEvent()),
      messages_awaiting_replies_(new IDMap<DBMessageState>()),
      unique_id_generator_(new base::AtomicSequenceNumber()) {
  DCHECK(!instance_);
  instance_ = this;
}

int DBMessageFilter::GetUniqueID() {
  return unique_id_generator_->GetNext();
}

static void SendMessageOnIOThread(IPC::Message* message,
                                  IPC::Channel* channel,
                                  Lock* channel_lock) {
  AutoLock channel_auto_lock(*channel_lock);
  if (channel)
    channel->Send(message);
  else
    delete message;
}

void DBMessageFilter::Send(IPC::Message* message) {
  io_thread_message_loop_->PostTask(FROM_HERE,
      NewRunnableFunction(SendMessageOnIOThread, message, channel_,
                          channel_lock_.get()));
}

void DBMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  AutoLock channel_auto_lock(*channel_lock_);
  channel_ = channel;
}

void DBMessageFilter::OnChannelError() {
  AutoLock channel_auto_lock(*channel_lock_);
  channel_ = NULL;
}

void DBMessageFilter::OnChannelClosing() {
  AutoLock channel_auto_lock(*channel_lock_);
  channel_ = NULL;
}

bool DBMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DBMessageFilter, message)
    IPC_MESSAGE_HANDLER(ViewMsg_DatabaseOpenFileResponse,
                        OnResponse<ViewMsg_DatabaseOpenFileResponse_Params>)
    IPC_MESSAGE_HANDLER(ViewMsg_DatabaseDeleteFileResponse, OnResponse<int>)
    IPC_MESSAGE_HANDLER(ViewMsg_DatabaseGetFileAttributesResponse,
                        OnResponse<uint32>)
    IPC_MESSAGE_HANDLER(ViewMsg_DatabaseGetFileSizeResponse, OnResponse<int64>)
    IPC_MESSAGE_HANDLER(ViewMsg_DatabaseUpdateSize, OnDatabaseUpdateSize)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DBMessageFilter::OnDatabaseUpdateSize(const string16& origin_identifier,
                                           const string16& database_name,
                                           int64 database_size,
                                           int64 space_available) {
  WebKit::WebDatabase::updateDatabaseSize(
      origin_identifier, database_name, database_size, space_available);
}
