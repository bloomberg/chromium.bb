// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Broker RPC Client implementation.

#include "ceee/ie/broker/broker_rpc_client.h"

#include "base/lock.h"
#include "base/logging.h"
#include "broker_rpc_lib.h"  // NOLINT
#include "ceee/common/com_utils.h"
#include "ceee/ie/broker/broker_rpc_utils.h"

BrokerRpcClient::BrokerRpcClient() : context_(0), binding_handle_(NULL) {
}

BrokerRpcClient::~BrokerRpcClient() {
  Disconnect();
}

int HandleRpcException(unsigned int rpc_exception_code) {
  switch (rpc_exception_code) {
    case STATUS_ACCESS_VIOLATION:
    case STATUS_DATATYPE_MISALIGNMENT:
    case STATUS_PRIVILEGED_INSTRUCTION:
    case STATUS_BREAKPOINT:
    case STATUS_STACK_OVERFLOW:
    case STATUS_IN_PAGE_ERROR:
    case STATUS_GUARD_PAGE_VIOLATION:
      return EXCEPTION_CONTINUE_SEARCH;
    default:
      break;
  }
  return EXCEPTION_EXECUTE_HANDLER;
}

void BrokerRpcClient::LockContext() {
  RpcTryExcept {
    context_ = BrokerRpcClient_Connect(binding_handle_);
  } RpcExcept(HandleRpcException(RpcExceptionCode())) {
    LOG(ERROR) << "RPC error in LockContext: " << RpcExceptionCode();
  } RpcEndExcept
}

void BrokerRpcClient::ReleaseContext() {
  RpcTryExcept {
    BrokerRpcClient_Disconnect(binding_handle_, &context_);
  } RpcExcept(HandleRpcException(RpcExceptionCode())) {
    LOG(ERROR) << "RPC error in ReleaseContext: " << RpcExceptionCode();
  } RpcEndExcept
}

bool BrokerRpcClient::Connect() {
  if (is_connected())
    return true;

  std::wstring end_point = GetRpcEndPointAddress();
  std::wstring protocol = kRpcProtocol;
  DCHECK(!protocol.empty());
  DCHECK(!end_point.empty());
  if (protocol.empty() || end_point.empty())
    return false;

  // TODO(vitalybuka@google.com): There's no guarantee (aside from name
  // uniqueness) that it will connect to an endpoint created by the same user.
  // Hint: The missing invocation is RpcBindingSetAuthInfoEx.
  LOG(INFO) << "Connecting to RPC server. Endpoint: " << end_point;
  RPC_WSTR string_binding = NULL;
  // Create binding string with given protocol and end point.
  RPC_STATUS status = ::RpcStringBindingCompose(
      NULL,
      reinterpret_cast<RPC_WSTR>(&protocol[0]),
      NULL,
      reinterpret_cast<RPC_WSTR>(&end_point[0]),
      NULL,
      &string_binding);
  LOG_IF(ERROR, RPC_S_OK != status) <<
      "Failed to compose binding string. RPC_STATUS=0x" << com::LogWe(status);

  if (RPC_S_OK == status) {
    // Create binding from just generated binding string. Binding handle should
    // used for PRC calls.
    status = ::RpcBindingFromStringBinding(string_binding, &binding_handle_);
    LOG_IF(ERROR, RPC_S_OK != status) <<
        "Failed to bind. RPC_STATUS=0x" << com::LogWe(status);
    ::RpcStringFree(&string_binding);
    if (RPC_S_OK == status) {
      LOG(INFO) << "RPC client is connected. Endpoint: " << end_point;
      LockContext();
    }
  }
  if (!is_connected())
    Disconnect();
  return is_connected();
}

void BrokerRpcClient::Disconnect() {
  if (context_ != NULL)
    ReleaseContext();
  if (binding_handle_ != NULL) {
    RPC_STATUS status = ::RpcBindingFree(&binding_handle_);
    LOG_IF(WARNING, RPC_S_OK != status) <<
        "Failed to unbind. RPC_STATUS=0x" << com::LogWe(status);
  }
}

bool BrokerRpcClient::FireEvent(BSTR event_name, BSTR event_args) {
  RpcTryExcept {
    BrokerRpcClient_FireEvent(binding_handle_, event_name, event_args);
    return true;
  } RpcExcept(HandleRpcException(RpcExceptionCode())) {
    LOG(ERROR) << "RPC error in FireEvent: " << RpcExceptionCode();
  } RpcEndExcept
  return false;
}
