// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/webgpu_decoder_impl.h"

#include <dawn_native/DawnNative.h>
#include <dawn_wire/WireServer.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "gpu/command_buffer/common/webgpu_cmd_format.h"
#include "gpu/command_buffer/common/webgpu_cmd_ids.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
#include "ipc/ipc_channel.h"

namespace gpu {
namespace webgpu {

namespace {

constexpr size_t kMaxWireBufferSize =
    std::min(IPC::Channel::kMaximumMessageSize,
             static_cast<size_t>(1024 * 1024));

class WireServerCommandSerializer : public dawn_wire::CommandSerializer {
 public:
  explicit WireServerCommandSerializer(DecoderClient* client);
  ~WireServerCommandSerializer() override = default;
  void* GetCmdSpace(size_t size) final;
  bool Flush() final;

 private:
  DecoderClient* client_;
  std::vector<uint8_t> buffer_;
  size_t put_offset_;
};

WireServerCommandSerializer::WireServerCommandSerializer(DecoderClient* client)
    : client_(client), buffer_(kMaxWireBufferSize), put_offset_(0) {}

void* WireServerCommandSerializer::GetCmdSpace(size_t size) {
  // TODO(enga): Handle chunking commands if size > kMaxWireBufferSize.
  if (size > kMaxWireBufferSize) {
    NOTREACHED();
    return nullptr;
  }

  // |next_offset| should never be more than kMaxWireBufferSize +
  // kMaxWireBufferSize.
  DCHECK_LE(put_offset_, kMaxWireBufferSize);
  DCHECK_LE(size, kMaxWireBufferSize);
  static_assert(base::CheckAdd(kMaxWireBufferSize, kMaxWireBufferSize)
                    .IsValid<uint32_t>(),
                "");
  uint32_t next_offset = put_offset_ + size;

  if (next_offset > buffer_.size()) {
    Flush();
    // TODO(enga): Keep track of how much command space the application is using
    // and adjust the buffer size accordingly.

    DCHECK_EQ(put_offset_, 0u);
    next_offset = size;
  }

  uint8_t* ptr = &buffer_[put_offset_];
  put_offset_ = next_offset;
  return ptr;
}

bool WireServerCommandSerializer::Flush() {
  if (put_offset_ > 0) {
    client_->HandleReturnData(base::make_span(buffer_.data(), put_offset_));
    put_offset_ = 0;
  }
  return true;
}

}  // namespace

class WebGPUDecoderImpl final : public WebGPUDecoder {
 public:
  WebGPUDecoderImpl(DecoderClient* client,
                    CommandBufferServiceBase* command_buffer_service,
                    gles2::Outputter* outputter);
  ~WebGPUDecoderImpl() override;

  // DecoderContext implementation.
  base::WeakPtr<DecoderContext> AsWeakPtr() override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  ContextResult Initialize(
      const scoped_refptr<gl::GLSurface>& surface,
      const scoped_refptr<gl::GLContext>& context,
      bool offscreen,
      const gles2::DisallowedFeatures& disallowed_features,
      const ContextCreationAttribs& attrib_helper) override {
    return ContextResult::kSuccess;
  }
  const gles2::ContextState* GetContextState() override {
    NOTREACHED();
    return nullptr;
  }
  void Destroy(bool have_context) override {}
  bool MakeCurrent() override { return true; }
  gl::GLContext* GetGLContext() override { return nullptr; }
  gl::GLSurface* GetGLSurface() override {
    NOTREACHED();
    return nullptr;
  }
  const gles2::FeatureInfo* GetFeatureInfo() const override {
    NOTREACHED();
    return nullptr;
  }
  Capabilities GetCapabilities() override { return {}; }
  void RestoreGlobalState() const override { NOTREACHED(); }
  void ClearAllAttributes() const override { NOTREACHED(); }
  void RestoreAllAttributes() const override { NOTREACHED(); }
  void RestoreState(const gles2::ContextState* prev_state) override {
    NOTREACHED();
  }
  void RestoreActiveTexture() const override { NOTREACHED(); }
  void RestoreAllTextureUnitAndSamplerBindings(
      const gles2::ContextState* prev_state) const override {
    NOTREACHED();
  }
  void RestoreActiveTextureUnitBinding(unsigned int target) const override {
    NOTREACHED();
  }
  void RestoreBufferBinding(unsigned int target) override { NOTREACHED(); }
  void RestoreBufferBindings() const override { NOTREACHED(); }
  void RestoreFramebufferBindings() const override { NOTREACHED(); }
  void RestoreRenderbufferBindings() override { NOTREACHED(); }
  void RestoreProgramBindings() const override { NOTREACHED(); }
  void RestoreTextureState(unsigned service_id) override { NOTREACHED(); }
  void RestoreTextureUnitBindings(unsigned unit) const override {
    NOTREACHED();
  }
  void RestoreVertexAttribArray(unsigned index) override { NOTREACHED(); }
  void RestoreAllExternalTextureBindingsIfNeeded() override { NOTREACHED(); }
  QueryManager* GetQueryManager() override {
    NOTREACHED();
    return nullptr;
  }
  void SetQueryCallback(unsigned int query_client_id,
                        base::OnceClosure callback) override {
    NOTREACHED();
  }
  gles2::GpuFenceManager* GetGpuFenceManager() override {
    NOTREACHED();
    return nullptr;
  }
  bool HasPendingQueries() const override { return false; }
  void ProcessPendingQueries(bool did_finish) override {}
  bool HasMoreIdleWork() const override { return false; }
  void PerformIdleWork() override { NOTREACHED(); }
  bool HasPollingWork() const override { return false; }
  void PerformPollingWork() override { NOTREACHED(); }
  TextureBase* GetTextureBase(uint32_t client_id) override {
    NOTREACHED();
    return nullptr;
  }
  void SetLevelInfo(uint32_t client_id,
                    int level,
                    unsigned internal_format,
                    unsigned width,
                    unsigned height,
                    unsigned depth,
                    unsigned format,
                    unsigned type,
                    const gfx::Rect& cleared_rect) override {
    NOTREACHED();
  }
  bool WasContextLost() const override {
    NOTIMPLEMENTED();
    return false;
  }
  bool WasContextLostByRobustnessExtension() const override {
    NOTREACHED();
    return false;
  }
  void MarkContextLost(error::ContextLostReason reason) override {
    NOTIMPLEMENTED();
  }
  bool CheckResetStatus() override {
    NOTREACHED();
    return false;
  }
  void BeginDecoding() override {}
  void EndDecoding() override {}
  const char* GetCommandName(unsigned int command_id) const;
  error::Error DoCommands(unsigned int num_commands,
                          const volatile void* buffer,
                          int num_entries,
                          int* entries_processed) override;
  base::StringPiece GetLogPrefix() override {
    NOTIMPLEMENTED();
    return "";
  }
  void BindImage(uint32_t client_texture_id,
                 uint32_t texture_target,
                 gl::GLImage* image,
                 bool can_bind_to_sampler) override {
    NOTREACHED();
  }
  gles2::ContextGroup* GetContextGroup() override { return nullptr; }
  gles2::ErrorState* GetErrorState() override {
    NOTREACHED();
    return nullptr;
  }
  std::unique_ptr<gles2::AbstractTexture> CreateAbstractTexture(
      GLenum target,
      GLenum internal_format,
      GLsizei width,
      GLsizei height,
      GLsizei depth,
      GLint border,
      GLenum format,
      GLenum type) override {
    NOTREACHED();
    return nullptr;
  }
  bool IsCompressedTextureFormat(unsigned format) override {
    NOTREACHED();
    return false;
  }
  bool ClearLevel(gles2::Texture* texture,
                  unsigned target,
                  int level,
                  unsigned format,
                  unsigned type,
                  int xoffset,
                  int yoffset,
                  int width,
                  int height) override {
    NOTREACHED();
    return false;
  }
  bool ClearCompressedTextureLevel(gles2::Texture* texture,
                                   unsigned target,
                                   int level,
                                   unsigned format,
                                   int width,
                                   int height) override {
    NOTREACHED();
    return false;
  }
  bool ClearLevel3D(gles2::Texture* texture,
                    unsigned target,
                    int level,
                    unsigned format,
                    unsigned type,
                    int width,
                    int height,
                    int depth) override {
    NOTREACHED();
    return false;
  }
  bool initialized() const override { return true; }
  void SetLogCommands(bool log_commands) override { NOTIMPLEMENTED(); }
  gles2::Outputter* outputter() const override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  int GetRasterDecoderId() const override {
    NOTREACHED();
    return -1;
  }

 private:
  typedef error::Error (WebGPUDecoderImpl::*CmdHandler)(
      uint32_t immediate_data_size,
      const volatile void* data);

  // A struct to hold info about each command.
  struct CommandInfo {
    CmdHandler cmd_handler;
    uint8_t arg_flags;   // How to handle the arguments for this command
    uint8_t cmd_flags;   // How to handle this command
    uint16_t arg_count;  // How many arguments are expected for this command.
  };

  // A table of CommandInfo for all the commands.
  static const CommandInfo command_info[kNumCommands - kFirstWebGPUCommand];

// Generate a member function prototype for each command in an automated and
// typesafe way.
#define WEBGPU_CMD_OP(name) \
  Error Handle##name(uint32_t immediate_data_size, const volatile void* data);
  WEBGPU_COMMAND_LIST(WEBGPU_CMD_OP)
#undef WEBGPU_CMD_OP

  // The current decoder error communicates the decoder error through command
  // processing functions that do not return the error value. Should be set
  // only if not returning an error.
  error::Error current_decoder_error_ = error::kNoError;

  DawnDevice CreateDefaultDevice();

  std::unique_ptr<WireServerCommandSerializer> wire_serializer_;
  std::unique_ptr<dawn_native::Instance> dawn_instance_;
  DawnProcTable dawn_procs_;
  DawnDevice dawn_device_ = nullptr;
  std::unique_ptr<dawn_wire::WireServer> wire_server_;

  DISALLOW_COPY_AND_ASSIGN(WebGPUDecoderImpl);
};

constexpr WebGPUDecoderImpl::CommandInfo WebGPUDecoderImpl::command_info[] = {
#define WEBGPU_CMD_OP(name)                                \
  {                                                        \
      &WebGPUDecoderImpl::Handle##name,                    \
      cmds::name::kArgFlags,                               \
      cmds::name::cmd_flags,                               \
      sizeof(cmds::name) / sizeof(CommandBufferEntry) - 1, \
  }, /* NOLINT */
    WEBGPU_COMMAND_LIST(WEBGPU_CMD_OP)
#undef WEBGPU_CMD_OP
};

WebGPUDecoder* CreateWebGPUDecoderImpl(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    gles2::Outputter* outputter) {
  return new WebGPUDecoderImpl(client, command_buffer_service, outputter);
}

WebGPUDecoderImpl::WebGPUDecoderImpl(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    gles2::Outputter* outputter)
    : WebGPUDecoder(client, command_buffer_service, outputter),
      wire_serializer_(new WireServerCommandSerializer(client)),
      dawn_instance_(new dawn_native::Instance()),
      dawn_procs_(dawn_native::GetProcs()),
      dawn_device_(CreateDefaultDevice()),
      wire_server_(new dawn_wire::WireServer(dawn_device_,
                                             dawn_procs_,
                                             wire_serializer_.get())) {}

WebGPUDecoderImpl::~WebGPUDecoderImpl() {}

DawnDevice WebGPUDecoderImpl::CreateDefaultDevice() {
  dawn_instance_->DiscoverDefaultAdapters();
  std::vector<dawn_native::Adapter> adapters = dawn_instance_->GetAdapters();
  for (size_t i = 0; i < adapters.size(); ++i) {
    return adapters[i].CreateDevice();
  }
  NOTREACHED();
  return {};
}

const char* WebGPUDecoderImpl::GetCommandName(unsigned int command_id) const {
  if (command_id >= kFirstWebGPUCommand && command_id < kNumCommands) {
    return webgpu::GetCommandName(static_cast<CommandId>(command_id));
  }
  return GetCommonCommandName(static_cast<cmd::CommandId>(command_id));
}

error::Error WebGPUDecoderImpl::DoCommands(unsigned int num_commands,
                                           const volatile void* buffer,
                                           int num_entries,
                                           int* entries_processed) {
  DCHECK(entries_processed);
  int commands_to_process = num_commands;
  error::Error result = error::kNoError;
  const volatile CommandBufferEntry* cmd_data =
      static_cast<const volatile CommandBufferEntry*>(buffer);
  int process_pos = 0;
  CommandId command = static_cast<CommandId>(0);

  while (process_pos < num_entries && result == error::kNoError &&
         commands_to_process--) {
    const unsigned int size = cmd_data->value_header.size;
    command = static_cast<CommandId>(cmd_data->value_header.command);

    if (size == 0) {
      result = error::kInvalidSize;
      break;
    }

    if (static_cast<int>(size) + process_pos > num_entries) {
      result = error::kOutOfBounds;
      break;
    }

    const unsigned int arg_count = size - 1;
    unsigned int command_index = command - kFirstWebGPUCommand;
    if (command_index < base::size(command_info)) {
      const CommandInfo& info = command_info[command_index];
      unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
      if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
          (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
        uint32_t immediate_data_size = (arg_count - info_arg_count) *
                                       sizeof(CommandBufferEntry);  // NOLINT
        result = (this->*info.cmd_handler)(immediate_data_size, cmd_data);
      } else {
        result = error::kInvalidArguments;
      }
    } else {
      result = DoCommonCommand(command, arg_count, cmd_data);
    }

    if (result == error::kNoError &&
        current_decoder_error_ != error::kNoError) {
      result = current_decoder_error_;
      current_decoder_error_ = error::kNoError;
    }

    if (result != error::kDeferCommandUntilLater) {
      process_pos += size;
      cmd_data += size;
    }
  }

  *entries_processed = process_pos;

  if (error::IsError(result)) {
    LOG(ERROR) << "Error: " << result << " for Command "
               << GetCommandName(command);
  }

  return result;
}

error::Error WebGPUDecoderImpl::HandleDummy(uint32_t immediate_data_size,
                                            const volatile void* cmd_data) {
  DLOG(ERROR) << "WebGPUDecoderImpl::HandleDummy";
  return error::kNoError;
}

error::Error WebGPUDecoderImpl::HandleDawnCommands(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile webgpu::cmds::DawnCommands& c =
      *static_cast<const volatile webgpu::cmds::DawnCommands*>(cmd_data);

  uint32_t size = static_cast<uint32_t>(c.size);
  uint32_t commands_shm_id = static_cast<uint32_t>(c.commands_shm_id);
  uint32_t commands_shm_offset = static_cast<uint32_t>(c.commands_shm_offset);

  const volatile char* shm_commands = GetSharedMemoryAs<const volatile char*>(
      commands_shm_id, commands_shm_offset, size);
  if (shm_commands == nullptr) {
    return error::kOutOfBounds;
  }

  std::vector<char> commands(shm_commands, shm_commands + size);
  wire_server_->HandleCommands(commands.data(), size);
  wire_serializer_->Flush();
  return error::kNoError;
}

}  // namespace webgpu
}  // namespace gpu
