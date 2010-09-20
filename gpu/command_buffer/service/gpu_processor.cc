// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "app/gfx/gl/gl_context.h"
#include "gpu/command_buffer/service/gpu_processor.h"

using ::base::SharedMemory;

namespace gpu {

GPUProcessor::GPUProcessor(CommandBuffer* command_buffer)
    : command_buffer_(command_buffer),
      commands_per_update_(100),
      method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(command_buffer);
  decoder_.reset(gles2::GLES2Decoder::Create(&group_));
  decoder_->set_engine(this);
}

GPUProcessor::GPUProcessor(CommandBuffer* command_buffer,
                           gles2::GLES2Decoder* decoder,
                           CommandParser* parser,
                           int commands_per_update)
    : command_buffer_(command_buffer),
      commands_per_update_(commands_per_update),
      method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(command_buffer);
  decoder_.reset(decoder);
  parser_.reset(parser);
}

GPUProcessor::~GPUProcessor() {
  Destroy();
}

bool GPUProcessor::InitializeCommon(gfx::GLContext* context,
                                    const gfx::Size& size,
                                    gles2::GLES2Decoder* parent_decoder,
                                    uint32 parent_texture_id) {
  DCHECK(context);

  // Map the ring buffer and create the parser.
  Buffer ring_buffer = command_buffer_->GetRingBuffer();
  if (ring_buffer.ptr) {
    parser_.reset(new CommandParser(ring_buffer.ptr,
                                    ring_buffer.size,
                                    0,
                                    ring_buffer.size,
                                    0,
                                    decoder_.get()));
  } else {
    parser_.reset(new CommandParser(NULL, 0, 0, 0, 0,
                                    decoder_.get()));
  }

  // Initialize the decoder with either the view or pbuffer GLContext.
  if (!decoder_->Initialize(context,
                            size,
                            parent_decoder,
                            parent_texture_id)) {
    LOG(ERROR) << "GPUProcessor::InitializeCommon failed because decoder "
               << "failed to initialize.";
    Destroy();
    return false;
  }

  return true;
}

void GPUProcessor::DestroyCommon() {
  bool have_context = false;
  if (decoder_.get()) {
    have_context = decoder_->MakeCurrent();
    decoder_->Destroy();
    decoder_.reset();
  }

  group_.Destroy(have_context);

  parser_.reset();
}

void GPUProcessor::ProcessCommands() {
  CommandBuffer::State state = command_buffer_->GetState();
  if (state.error != error::kNoError)
    return;

  if (decoder_.get()) {
    if (!decoder_->MakeCurrent()) {
      LOG(ERROR) << "Context lost because MakeCurrent failed.";
      command_buffer_->SetParseError(error::kLostContext);
      return;
    }
  }

  parser_->set_put(state.put_offset);

  int commands_processed = 0;
  while (commands_processed < commands_per_update_ && !parser_->IsEmpty()) {
    error::Error error = parser_->ProcessCommand();
    if (error != error::kNoError) {
      command_buffer_->SetParseError(error);
      return;
    }
    ++commands_processed;
  }

  command_buffer_->SetGetOffset(static_cast<int32>(parser_->get()));

  if (!parser_->IsEmpty()) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        method_factory_.NewRunnableMethod(&GPUProcessor::ProcessCommands));
  }
}

Buffer GPUProcessor::GetSharedMemoryBuffer(int32 shm_id) {
  return command_buffer_->GetTransferBuffer(shm_id);
}

void GPUProcessor::set_token(int32 token) {
  command_buffer_->SetToken(token);
}

bool GPUProcessor::SetGetOffset(int32 offset) {
  if (parser_->set_get(offset)) {
    command_buffer_->SetGetOffset(static_cast<int32>(parser_->get()));
    return true;
  }
  return false;
}

int32 GPUProcessor::GetGetOffset() {
  return parser_->get();
}

void GPUProcessor::ResizeOffscreenFrameBuffer(const gfx::Size& size) {
  decoder_->ResizeOffscreenFrameBuffer(size);
}

void GPUProcessor::SetSwapBuffersCallback(
    Callback0::Type* callback) {
  wrapped_swap_buffers_callback_.reset(callback);
  decoder_->SetSwapBuffersCallback(
      NewCallback(this,
                  &GPUProcessor::WillSwapBuffers));
}

}  // namespace gpu
