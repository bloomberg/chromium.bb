// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_BINDER_COMMAND_BROKER_H_
#define CHROMEOS_BINDER_COMMAND_BROKER_H_

#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromeos/binder/command_stream.h"
#include "chromeos/chromeos_export.h"

namespace binder {

class Driver;
class TransactionData;

// Issues appropriate outgoing commands to perform required tasks, and
// dispatches incoming commands to appropriate objects.
// Usually this class lives as long as the corresponding thread.
// TODO(hashimoto): Add code to handle incoming commands (e.g. transactions).
class CHROMEOS_EXPORT CommandBroker
    : public CommandStream::IncomingCommandHandler {
 public:
  explicit CommandBroker(Driver* driver);
  ~CommandBroker() override;

  // Performs transaction with the remote object specified by the handle.
  // Returns true on success. If not one-way transaction, this method blocks
  // until the target object sends a reply.
  bool Transact(int32_t handle,
                const TransactionData& request,
                scoped_ptr<TransactionData>* reply);

  // CommandStream::IncomingCommandHandler override:
  void OnReply(scoped_ptr<TransactionData> data) override;
  void OnDeadReply() override;
  void OnTransactionComplete() override;
  void OnFailedReply() override;

 private:
  enum ResponseType {
    RESPONSE_TYPE_NONE,
    RESPONSE_TYPE_TRANSACTION_COMPLETE,
    RESPONSE_TYPE_TRANSACTION_REPLY,
    RESPONSE_TYPE_FAILED,
    RESPONSE_TYPE_DEAD,
  };

  // Waits for a response to the previous transaction.
  ResponseType WaitForResponse(scoped_ptr<TransactionData>* data);

  base::ThreadChecker thread_checker_;
  CommandStream command_stream_;

  ResponseType response_type_ = RESPONSE_TYPE_NONE;
  scoped_ptr<TransactionData> response_data_;

  DISALLOW_COPY_AND_ASSIGN(CommandBroker);
};

}  // namespace binder

#endif  // CHROMEOS_BINDER_COMMAND_BROKER_H_
