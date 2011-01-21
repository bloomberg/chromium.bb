// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Broker RPC Server implementation.

#include "ceee/ie/broker/broker_rpc_server.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/process_util.h"
#include "broker_rpc_lib.h"  // NOLINT
#include "ceee/common/com_utils.h"
#include "ceee/ie/broker/broker_module_util.h"
#include "ceee/ie/broker/broker_rpc_utils.h"
#include "ceee/ie/broker/chrome_postman.h"


namespace {
// This lock ensures that histograms created by the broker are thread safe.
// The histograms created here can be initialized on multiple threads.
Lock g_metrics_lock;

RPC_STATUS PrepareEndpoint(std::wstring endpoint) {
  std::wstring protocol = kRpcProtocol;
  DCHECK(!protocol.empty());
  DCHECK(!endpoint.empty());
  if (protocol.empty() || endpoint.empty())
    return false;
  VLOG(1) << "RPC server is starting. Endpoint: " << endpoint;
  // Tell RPC runtime to use local interprocess communication for given
  // end point.
  RPC_STATUS status = ::RpcServerUseProtseqEp(
      reinterpret_cast<RPC_WSTR>(&protocol[0]),
      RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
      reinterpret_cast<RPC_WSTR>(&endpoint[0]),
      NULL);
  LOG_IF(ERROR, RPC_S_OK != status && RPC_S_DUPLICATE_ENDPOINT != status) <<
      "Failed to set protocol for RPC end point. RPC_STATUS=0x" <<
      com::LogWe(status);
  // This is not an error because unittest may start several servers. For
  // ceee_broker this is an error because we should not have several instances
  // of broker for the same endpoint. However BrokerRpcServer will fail anyway
  // while starting to listen.
  if (RPC_S_DUPLICATE_ENDPOINT == status)
    status = RPC_S_OK;
  return status;
}

}  // namespace

BrokerRpcServer::BrokerRpcServer()
    : is_started_(false),
      current_thread_(::GetCurrentThreadId()) {
}

BrokerRpcServer::~BrokerRpcServer() {
  DCHECK(current_thread_ == ::GetCurrentThreadId());
  Stop();
}

bool BrokerRpcServer::Start() {
  DCHECK(current_thread_ == ::GetCurrentThreadId());

  if (is_started())
    return true;

  std::wstring endpoint = GetRpcEndpointAddress();
  RPC_STATUS status = PrepareEndpoint(endpoint);

  if (RPC_S_OK == status) {
    // Register RPC interface with the RPC runtime.
    status = ::RpcServerRegisterIfEx(BrokerRpcServer_CeeeBroker_v1_1_s_ifspec,
        NULL, NULL, 0, RPC_C_LISTEN_MAX_CALLS_DEFAULT, NULL);
    LOG_IF(ERROR, RPC_S_OK != status) <<
      "Failed to register RPC interface. RPC_STATUS=0x" << com::LogWe(status);
    if (RPC_S_OK == status) {
      // Start listen for RPC calls.
      status = ::RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, TRUE);
      LOG_IF(ERROR, RPC_S_OK != status) <<
          "Failed to start listening. RPC_STATUS=0x" << com::LogWe(status);
      if (RPC_S_OK == status) {
        VLOG(1) << "RPC server is started. Endpoint: " << endpoint;
        is_started_ = true;
      }
    }
  }
  if (!is_started())
    Stop();

  return is_started();
}

bool BrokerRpcServer::Stop() {
  DCHECK(current_thread_ == ::GetCurrentThreadId());
  is_started_ = false;
  // Stop server listening for RPC.
  RPC_STATUS status = ::RpcMgmtStopServerListening(NULL);
  LOG_IF(WARNING, RPC_S_OK != status && RPC_S_NOT_LISTENING != status) <<
      "Failed to stop listening. RPC_STATUS=0x" << com::LogWe(status);
  // Wait while server stops listening threads.
  status = ::RpcMgmtWaitServerListen();
  LOG_IF(WARNING, RPC_S_OK != status && RPC_S_NOT_LISTENING != status) <<
      "Failed to wait server listen. RPC_STATUS=0x" << com::LogWe(status);
  // Unregister RPC interface.
  status = ::RpcServerUnregisterIf(
      BrokerRpcServer_CeeeBroker_v1_1_s_ifspec, NULL, FALSE);
  LOG_IF(WARNING, RPC_S_OK != status && RPC_S_UNKNOWN_MGR_TYPE != status &&
                  RPC_S_UNKNOWN_IF != status) <<
      "Failed to unregister interface. RPC_STATUS=0x" << com::LogWe(status);

  return RPC_S_OK == status;
}

bool BrokerRpcServer::is_started() const {
  DCHECK(current_thread_ == ::GetCurrentThreadId());
  return is_started_;
}

static base::AtomicSequenceNumber current_broker_rpc_context(
    base::LINKER_INITIALIZED);

BrokerContextHandle BrokerRpcServer_Connect(handle_t binding_handle) {
  // TODO(vitalybuka@google.com): Add client identity check.
  ceee_module_util::LockModule();
  return reinterpret_cast<void*>(current_broker_rpc_context.GetNext() + 1);
}

void BrokerRpcServer_Disconnect(
    handle_t binding_handle,
    BrokerContextHandle* context) {
  DCHECK(context != NULL);
  if (context)
    *context = NULL;
  ceee_module_util::UnlockModule();
}

// Called when client process terminated without releasing context handle.
void __RPC_USER BrokerContextHandle_rundown(BrokerContextHandle context) {
  DCHECK(context != NULL);
  ceee_module_util::UnlockModule();
}

void BrokerRpcServer_FireEvent(
    handle_t binding_handle,
    BrokerContextHandle context,
    const char* event_name,
    const char* event_args) {
  DCHECK(ChromePostman::GetInstance());
  if (ChromePostman::GetInstance())
    ChromePostman::GetInstance()->FireEvent(event_name, event_args);
}

void BrokerRpcServer_SendUmaHistogramTimes(handle_t binding_handle,
                                           const char* name,
                                           int sample) {
  // We can't unfortunately use the HISTOGRAM_*_TIMES here because they use
  // static variables to save time.
  base::AutoLock lock(g_metrics_lock);
  scoped_refptr<base::Histogram> counter =
      base::Histogram::FactoryTimeGet(name,
          base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromSeconds(10),
          50, base::Histogram::kUmaTargetedHistogramFlag);
  DCHECK_EQ(name, counter->histogram_name());
  if (counter.get())
    counter->AddTime(base::TimeDelta::FromMilliseconds(sample));
}

void BrokerRpcServer_SendUmaHistogramData(handle_t binding_handle,
                                          const char* name,
                                          int sample,
                                          int min, int max,
                                          int bucket_count) {
  // We can't unfortunately use the HISTOGRAM_*_COUNT here because they use
  // static variables to save time.
  base::AutoLock lock(g_metrics_lock);
  scoped_refptr<base::Histogram> counter =
      base::Histogram::FactoryGet(name, min, max, bucket_count,
          base::Histogram::kUmaTargetedHistogramFlag);
  DCHECK_EQ(name, counter->histogram_name());
  if (counter.get())
    counter->AddTime(base::TimeDelta::FromMilliseconds(sample));
}
