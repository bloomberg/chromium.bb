// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Broker RPC Client implementation.

#include "ceee/ie/broker/broker_rpc_client.h"

#include <atlbase.h>

#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/tuple.h"
#include "base/win/scoped_comptr.h"
#include "ceee/common/com_utils.h"
#include "ceee/ie/broker/broker_rpc_utils.h"
#include "ceee/ie/common/ceee_module_util.h"

#include "broker_lib.h"  // NOLINT
#include "broker_rpc_lib.h"  // NOLINT

namespace {

// Avoid using objects requiring unwind in functions that use __try.
void LogRpcException(const char* str, unsigned int exception_code) {
  LOG(ERROR) << str << com::LogWe(exception_code);
}

// Avoid using objects requiring unwind in functions that use __try.
void RpcDcheck(const char* message) {
  NOTREACHED() << message;
}

HRESULT BindRpc(std::wstring endpoint, RPC_BINDING_HANDLE* binding_handle) {
  DCHECK(binding_handle != NULL);
  std::wstring protocol = kRpcProtocol;
  DCHECK(!protocol.empty());
  DCHECK(!endpoint.empty());
  if (protocol.empty() || endpoint.empty() || binding_handle == NULL)
    return E_INVALIDARG;

  RPC_BINDING_HANDLE tmp_binding_handle = NULL;

  // TODO(vitalybuka@google.com): There's no guarantee (aside from name
  // uniqueness) that it will connect to an endpoint created by the same user.
  // Hint: The missing invocation is RpcBindingSetAuthInfoEx.
  VLOG(1) << "Connecting to RPC server. Endpoint: " << endpoint;
  RPC_WSTR string_binding = NULL;
  // Create binding string for given end point.
  RPC_STATUS status = ::RpcStringBindingCompose(
      NULL,
      reinterpret_cast<RPC_WSTR>(&protocol[0]),
      NULL,
      reinterpret_cast<RPC_WSTR>(&endpoint[0]),
      NULL,
      &string_binding);

  if (RPC_S_OK == status) {
    // Create binding from just generated binding string. Binding handle should
    // be used for PRC calls.
    status = ::RpcBindingFromStringBinding(string_binding, &tmp_binding_handle);
    ::RpcStringFree(&string_binding);
    if (RPC_S_OK == status) {
      VLOG(1) << "RPC client is connected. Endpoint: " << endpoint;
      *binding_handle = tmp_binding_handle;
      return S_OK;
    } else {
      LogRpcException("Failed to bind. RPC_STATUS:", status);
    }
  } else {
    LogRpcException("Failed to compose binding string. RPC_STATUS:", status);
  }
  return RPC_E_FAULT;
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

}  // namespace

BrokerRpcClient::BrokerRpcClient(bool allow_restarts)
    : context_(0),
      binding_handle_(NULL),
      allow_restarts_(allow_restarts) {
}

BrokerRpcClient::~BrokerRpcClient() {
  Disconnect();
}

void BrokerRpcClient::LockContext() {
  RpcTryExcept {
    context_ = BrokerRpcClient_Connect(binding_handle_);
  } RpcExcept(HandleRpcException(RpcExceptionCode())) {
    LogRpcException("RPC error in LockContext", RpcExceptionCode());
  } RpcEndExcept
}

void BrokerRpcClient::ReleaseContext() {
  RpcTryExcept {
    BrokerRpcClient_Disconnect(binding_handle_, &context_);
  } RpcExcept(HandleRpcException(RpcExceptionCode())) {
    LogRpcException("RPC error in ReleaseContext", RpcExceptionCode());
  } RpcEndExcept
}

HRESULT BrokerRpcClient::StartServer(ICeeeBrokerRegistrar** server) {
  return StartCeeeBroker(server);
}

HRESULT BrokerRpcClient::Connect(bool start_server) {
  if (is_connected())
    return S_OK;

  // Keep alive until RPC is connected.
  base::win::ScopedComPtr<ICeeeBrokerRegistrar> broker;
  if (start_server) {
    HRESULT hr = StartServer(broker.Receive());
    if (FAILED(hr))
      return hr;
  }

  if (SUCCEEDED(BindRpc(GetRpcEndpointAddress(), &binding_handle_)))
    LockContext();

  if (!is_connected()) {
    Disconnect();
    return RPC_E_FAULT;
  }
  return S_OK;
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

template<class Function, class Params>
HRESULT BrokerRpcClient::RunRpc(bool allow_restart,
                                Function rpc_function,
                                const Params& params) {
  if (!rpc_function) {
    RpcDcheck("rpc_function is NULL");
  }
  if (!is_connected()) {
    RpcDcheck("BrokerRpcClient is not connected");
    return RPC_E_FAULT;
  }
  RpcTryExcept {
    DispatchToFunction(rpc_function, params);
    return S_OK;
  } RpcExcept(HandleRpcException(RpcExceptionCode())) {
    LogRpcException("RPC error in RunRpc", RpcExceptionCode());

    if (allow_restart &&
        RPC_S_OK != ::RpcMgmtIsServerListening(binding_handle_)) {
      Disconnect();
      if (SUCCEEDED(Connect(true))) {
        return RunRpc(false, rpc_function, params);
      }
    }
    return RPC_E_FAULT;
  } RpcEndExcept
}

HRESULT BrokerRpcClient::FireEvent(const char* event_name,
                                   const char* event_args) {
  return RunRpc(allow_restarts_,
                &BrokerRpcClient_FireEvent,
                MakeRefTuple(binding_handle_, context_, event_name,
                             event_args));
}

HRESULT BrokerRpcClient::SendUmaHistogramTimes(const char* name, int sample) {
  return RunRpc(allow_restarts_,
                &BrokerRpcClient_SendUmaHistogramTimes,
                MakeRefTuple(binding_handle_, name, sample));
}

HRESULT BrokerRpcClient::SendUmaHistogramData(const char* name,
                                              int sample,
                                              int min,
                                              int max,
                                              int bucket_count) {
  return RunRpc(allow_restarts_,
                &BrokerRpcClient_SendUmaHistogramData,
                MakeRefTuple(binding_handle_, name, sample, min, max,
                          bucket_count));
}

HRESULT StartCeeeBroker(ICeeeBrokerRegistrar** broker) {
  ceee_module_util::RefreshElevationPolicyIfNeeded();
  base::win::ScopedComPtr<ICeeeBrokerRegistrar> broker_tmp;
  // TODO(vitalybuka@google.com): Start broker without COM after the last
  // COM interface is removed.
  HRESULT hr = broker_tmp.CreateInstance(CLSID_CeeeBroker);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create broker. " << com::LogHr(hr);
    return hr;
  }
  *broker = broker_tmp.Detach();
  return S_OK;
}
