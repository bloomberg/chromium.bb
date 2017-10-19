// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_DIRECT_H_
#define GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_DIRECT_H_

#include "base/callback.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/gpu_export.h"

namespace gpu {

class AsyncAPIInterface;
class TransferBufferManager;
class SyncPointClientState;
class SyncPointManager;
class SyncPointOrderData;
struct SyncToken;

class GPU_EXPORT CommandBufferDirect : public CommandBuffer,
                                       public CommandBufferServiceClient,
                                       public gles2::GLES2DecoderClient {
 public:
  using MakeCurrentCallback = base::Callback<bool()>;

  CommandBufferDirect(TransferBufferManager* transfer_buffer_manager,
                      SyncPointManager* sync_point_manager);
  explicit CommandBufferDirect(TransferBufferManager* transfer_buffer_manager);

  ~CommandBufferDirect() override;

  void set_handler(AsyncAPIInterface* handler) { handler_ = handler; }
  CommandBufferServiceBase* service() { return &service_; }

  // CommandBuffer implementation:
  CommandBuffer::State GetLastState() override;
  void Flush(int32_t put_offset) override;
  void OrderingBarrier(int32_t put_offset) override;
  CommandBuffer::State WaitForTokenInRange(int32_t start, int32_t end) override;
  CommandBuffer::State WaitForGetOffsetInRange(uint32_t set_get_buffer_count,
                                               int32_t start,
                                               int32_t end) override;
  void SetGetBuffer(int32_t transfer_buffer_id) override;
  scoped_refptr<Buffer> CreateTransferBuffer(size_t size, int32_t* id) override;
  void DestroyTransferBuffer(int32_t id) override;

  // CommandBufferServiceBase implementation:
  CommandBatchProcessedResult OnCommandBatchProcessed() override;
  void OnParseError() override;

  // GLES2DecoderClient implementation
  void OnConsoleMessage(int32_t id, const std::string& message) override;
  void CacheShader(const std::string& key, const std::string& shader) override;
  void OnFenceSyncRelease(uint64_t release) override;
  bool OnWaitSyncToken(const gpu::SyncToken&) override;
  void OnDescheduleUntilFinished() override;
  void OnRescheduleAfterFinished() override;

  CommandBufferNamespace GetNamespaceID() const;
  CommandBufferId GetCommandBufferID() const;

  void SetCommandsPaused(bool paused);
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       const base::Closure& callback);

  scoped_refptr<Buffer> CreateTransferBufferWithId(size_t size, int32_t id);

  void SetGetOffsetForTest(int32_t get_offset) {
    service_.SetGetOffsetForTest(get_offset);
  }

 private:
  CommandBufferService service_;
  SyncPointManager* sync_point_manager_;

  AsyncAPIInterface* handler_ = nullptr;
  scoped_refptr<SyncPointOrderData> sync_point_order_data_;
  scoped_refptr<SyncPointClientState> sync_point_client_state_;
  bool pause_commands_ = false;
  uint32_t paused_order_num_ = 0;
  const CommandBufferId command_buffer_id_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_DIRECT_H
