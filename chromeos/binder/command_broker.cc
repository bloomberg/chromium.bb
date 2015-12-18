// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/binder/command_broker.h"

#include <linux/android/binder.h>

#include "base/logging.h"
#include "chromeos/binder/driver.h"
#include "chromeos/binder/transaction_data.h"

namespace binder {

namespace {

// Converts TransactionData to binder_transaction_data struct.
binder_transaction_data ConvertTransactionDataToStruct(
    const TransactionData& data) {
  binder_transaction_data result = {};
  result.code = data.GetCode();
  result.flags = TF_ACCEPT_FDS;
  if (data.IsOneWay()) {
    result.flags |= TF_ONE_WAY;
  }
  if (data.HasStatus()) {
    result.flags |= TF_STATUS_CODE;
  }
  result.data_size = data.GetDataSize();
  result.data.ptr.buffer = reinterpret_cast<binder_uintptr_t>(data.GetData());
  result.offsets_size = data.GetNumObjectOffsets() * sizeof(size_t);
  result.data.ptr.offsets =
      reinterpret_cast<binder_uintptr_t>(data.GetObjectOffsets());
  return result;
}

}  // namespace

CommandBroker::CommandBroker(Driver* driver) : command_stream_(driver, this) {}

CommandBroker::~CommandBroker() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool CommandBroker::Transact(int32 handle,
                             const TransactionData& request,
                             scoped_ptr<TransactionData>* reply) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Send transaction.
  binder_transaction_data tr = ConvertTransactionDataToStruct(request);
  tr.target.handle = handle;
  command_stream_.AppendOutgoingCommand(BC_TRANSACTION, &tr, sizeof(tr));
  if (!command_stream_.Flush()) {
    LOG(ERROR) << "Failed to write";
    return false;
  }
  // Wait for response.
  scoped_ptr<TransactionData> response_data;
  ResponseType response_type = WaitForResponse(&response_data);
  if (response_type != RESPONSE_TYPE_TRANSACTION_COMPLETE) {
    LOG(ERROR) << "Failed to wait for response: response_type = "
               << response_type;
    return false;
  }
  if (!request.IsOneWay()) {
    // Wait for reply.
    scoped_ptr<TransactionData> response_data;
    ResponseType response_type = WaitForResponse(&response_data);
    if (response_type != RESPONSE_TYPE_TRANSACTION_REPLY) {
      LOG(ERROR) << "Failed to wait for response: response_type = "
                 << response_type;
      return false;
    }
    *reply = response_data.Pass();
  }
  return true;
}

void CommandBroker::OnReply(scoped_ptr<TransactionData> data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(response_type_, RESPONSE_TYPE_NONE);
  DCHECK(!response_data_);
  response_type_ = RESPONSE_TYPE_TRANSACTION_REPLY;
  response_data_ = data.Pass();
}

void CommandBroker::OnDeadReply() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(response_type_, RESPONSE_TYPE_NONE);
  response_type_ = RESPONSE_TYPE_DEAD;
}

void CommandBroker::OnTransactionComplete() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(response_type_, RESPONSE_TYPE_NONE);
  response_type_ = RESPONSE_TYPE_TRANSACTION_COMPLETE;
}

void CommandBroker::OnFailedReply() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(response_type_, RESPONSE_TYPE_NONE);
  response_type_ = RESPONSE_TYPE_FAILED;
}

CommandBroker::ResponseType CommandBroker::WaitForResponse(
    scoped_ptr<TransactionData>* data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(response_type_, RESPONSE_TYPE_NONE);
  DCHECK(!response_data_);
  while (response_type_ == RESPONSE_TYPE_NONE) {
    if (command_stream_.CanProcessIncomingCommand()) {
      if (!command_stream_.ProcessIncomingCommand()) {
        LOG(ERROR) << "Failed to process command.";
        return RESPONSE_TYPE_NONE;
      }
    } else {
      if (!command_stream_.Fetch()) {
        LOG(ERROR) << "Failed to fetch.";
        return RESPONSE_TYPE_NONE;
      }
    }
  }
  ResponseType response_type = response_type_;
  response_type_ = RESPONSE_TYPE_NONE;
  *data = response_data_.Pass();
  return response_type;
}

}  // namespace binder
