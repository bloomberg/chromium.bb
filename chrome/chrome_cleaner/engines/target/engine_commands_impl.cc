// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/engine_commands_impl.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "chrome/chrome_cleaner/crash/crash_keys.h"
#include "chrome/chrome_cleaner/engines/target/cleaner_engine_requests_proxy.h"
#include "chrome/chrome_cleaner/engines/target/engine_cleanup_results_proxy.h"
#include "chrome/chrome_cleaner/engines/target/engine_file_requests_proxy.h"
#include "chrome/chrome_cleaner/engines/target/engine_requests_proxy.h"
#include "chrome/chrome_cleaner/engines/target/engine_scan_results_proxy.h"

namespace chrome_cleaner {

using mojom::CleanerEngineRequestsAssociatedPtr;
using mojom::CleanerEngineRequestsAssociatedPtrInfo;
using mojom::EngineCleanupResultsAssociatedPtr;
using mojom::EngineCleanupResultsAssociatedPtrInfo;
using mojom::EngineFileRequestsAssociatedPtr;
using mojom::EngineFileRequestsAssociatedPtrInfo;
using mojom::EngineRequestsAssociatedPtr;
using mojom::EngineRequestsAssociatedPtrInfo;
using mojom::EngineScanResultsAssociatedPtr;
using mojom::EngineScanResultsAssociatedPtrInfo;

namespace {

constexpr char kStageCrashKey[] = "stage";

class ScopedCrashStageRecorder {
 public:
  explicit ScopedCrashStageRecorder(const std::string& stage) : stage_(stage) {
    SetCrashKey(kStageCrashKey, stage_);
  }

  ~ScopedCrashStageRecorder() {
    stage_ += "-done";
    SetCrashKey(kStageCrashKey, stage_);
  }

 private:
  std::string stage_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCrashStageRecorder);
};

}  // namespace

EngineCommandsImpl::EngineCommandsImpl(
    scoped_refptr<EngineDelegate> engine_delegate,
    mojom::EngineCommandsRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::OnceClosure error_handler)
    : engine_delegate_(engine_delegate),
      binding_(this, std::move(request)),
      task_runner_(task_runner) {
  binding_.set_connection_error_handler(std::move(error_handler));
}

EngineCommandsImpl::~EngineCommandsImpl() = default;

void EngineCommandsImpl::Initialize(
    mojom::EngineFileRequestsAssociatedPtrInfo file_requests,
    const base::FilePath& log_directory_path,
    InitializeCallback callback) {
  ScopedCrashStageRecorder crash_stage(__func__);

  // Create proxies to pass requests to the broker process over Mojo.
  mojom::EngineFileRequestsAssociatedPtr file_requests_ptr;
  file_requests_ptr.Bind(std::move(file_requests));
  scoped_refptr<EngineFileRequestsProxy> file_requests_proxy =
      base::MakeRefCounted<EngineFileRequestsProxy>(
          std::move(file_requests_ptr), task_runner_);

  // This object is not retained because it outlives the callback: it's
  // destroyed on this sequence, once the main thread returns, which should only
  // happen after initialization has completed. If the broker process
  // terminates, then this process will be also be terminated by the connection
  // error handler, and there is not need to add complexity to handle it.
  engine_delegate_->Initialize(
      log_directory_path, file_requests_proxy,
      base::BindOnce(&EngineCommandsImpl::PostInitializeCallback,
                     base::Unretained(this), base::Passed(&callback)));
}

void EngineCommandsImpl::StartScan(
    const std::vector<UwSId>& enabled_uws,
    const std::vector<UwS::TraceLocation>& enabled_trace_locations,
    bool include_details,
    mojom::EngineFileRequestsAssociatedPtrInfo file_requests,
    mojom::EngineRequestsAssociatedPtrInfo sandboxed_engine_requests,
    EngineScanResultsAssociatedPtrInfo scan_results_info,
    StartScanCallback callback) {
  ScopedCrashStageRecorder crash_stage(__func__);

  // Create proxies to pass requests to the broker process over Mojo.
  EngineFileRequestsAssociatedPtr file_requests_ptr;
  file_requests_ptr.Bind(std::move(file_requests));
  scoped_refptr<EngineFileRequestsProxy> file_requests_proxy =
      base::MakeRefCounted<EngineFileRequestsProxy>(
          std::move(file_requests_ptr), task_runner_);

  EngineRequestsAssociatedPtr engine_requests_ptr;
  engine_requests_ptr.Bind(std::move(sandboxed_engine_requests));
  scoped_refptr<EngineRequestsProxy> engine_requests_proxy =
      base::MakeRefCounted<EngineRequestsProxy>(std::move(engine_requests_ptr),
                                                task_runner_);

  // Create an EngineScanResults proxy to send results back over the
  // Mojo connection.
  EngineScanResultsAssociatedPtr scan_results_ptr;
  scan_results_ptr.Bind(std::move(scan_results_info));
  scoped_refptr<EngineScanResultsProxy> scan_results_proxy =
      base::MakeRefCounted<EngineScanResultsProxy>(std::move(scan_results_ptr),
                                                   task_runner_);

  uint32_t result_code = engine_delegate_->StartScan(
      enabled_uws, enabled_trace_locations, include_details,
      file_requests_proxy, engine_requests_proxy, scan_results_proxy);
  std::move(callback).Run(result_code);
}

void EngineCommandsImpl::StartCleanup(
    const std::vector<UwSId>& enabled_uws,
    mojom::EngineFileRequestsAssociatedPtrInfo file_requests,
    mojom::EngineRequestsAssociatedPtrInfo sandboxed_engine_requests,
    mojom::CleanerEngineRequestsAssociatedPtrInfo
        sandboxed_cleaner_engine_requests,
    mojom::EngineCleanupResultsAssociatedPtrInfo clean_results_info,
    StartCleanupCallback callback) {
  ScopedCrashStageRecorder crash_stage(__func__);

  // Create proxies to pass requests to the broker process over Mojo.
  EngineFileRequestsAssociatedPtr file_requests_ptr;
  file_requests_ptr.Bind(std::move(file_requests));
  scoped_refptr<EngineFileRequestsProxy> file_requests_proxy =
      base::MakeRefCounted<EngineFileRequestsProxy>(
          std::move(file_requests_ptr), task_runner_);

  EngineRequestsAssociatedPtr engine_requests_ptr;
  engine_requests_ptr.Bind(std::move(sandboxed_engine_requests));
  scoped_refptr<EngineRequestsProxy> engine_requests_proxy =
      base::MakeRefCounted<EngineRequestsProxy>(std::move(engine_requests_ptr),
                                                task_runner_);

  CleanerEngineRequestsAssociatedPtr cleaner_engine_requests_ptr;
  cleaner_engine_requests_ptr.Bind(
      std::move(sandboxed_cleaner_engine_requests));
  scoped_refptr<CleanerEngineRequestsProxy> cleaner_engine_requests_proxy =
      base::MakeRefCounted<CleanerEngineRequestsProxy>(
          std::move(cleaner_engine_requests_ptr), task_runner_);

  // Create an EngineCleanupResults proxy to send results back over the
  // Mojo connection.
  mojom::EngineCleanupResultsAssociatedPtr cleanup_results_ptr;
  cleanup_results_ptr.Bind(std::move(clean_results_info));
  scoped_refptr<EngineCleanupResultsProxy> cleanup_results_proxy =
      base::MakeRefCounted<EngineCleanupResultsProxy>(
          std::move(cleanup_results_ptr), task_runner_);

  uint32_t result_code = engine_delegate_->StartCleanup(
      enabled_uws, file_requests_proxy, engine_requests_proxy,
      cleaner_engine_requests_proxy, cleanup_results_proxy);
  std::move(callback).Run(result_code);
}

void EngineCommandsImpl::Finalize(FinalizeCallback callback) {
  ScopedCrashStageRecorder crash_stage(__func__);
  uint32_t result_code = engine_delegate_->Finalize();
  std::move(callback).Run(result_code);
}

void EngineCommandsImpl::PostInitializeCallback(
    mojom::EngineCommands::InitializeCallback callback,
    uint32_t result_code) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), result_code));
}

}  // namespace chrome_cleaner
