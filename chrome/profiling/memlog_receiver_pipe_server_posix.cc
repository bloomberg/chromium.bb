// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/memlog_receiver_pipe_server_posix.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/profiling/memlog_stream.h"

namespace profiling {

MemlogReceiverPipeServer::MemlogReceiverPipeServer(
    base::TaskRunner* io_runner,
    const std::string& pipe_id,
    NewConnectionCallback on_new_conn)
    : io_runner_(io_runner),
      pipe_id_(pipe_id),
      on_new_connection_(on_new_conn) {}

MemlogReceiverPipeServer::~MemlogReceiverPipeServer() {}

void MemlogReceiverPipeServer::Start() {}

}  // namespace profiling
