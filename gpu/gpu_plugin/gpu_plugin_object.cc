// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "base/logging.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/gpu_processor.h"
#include "gpu/np_utils/np_utils.h"
#include "gpu/gpu_plugin/gpu_plugin_object.h"

using ::base::SharedMemory;
using command_buffer::CommandBuffer;
using command_buffer::CommandBufferService;
using command_buffer::GPUProcessor;
using np_utils::NPBrowser;
using np_utils::NPObjectPointer;

namespace gpu_plugin {

const NPUTF8 GPUPluginObject::kPluginType[] =
    "application/vnd.google.chrome.gpu-plugin";

GPUPluginObject::GPUPluginObject(NPP npp)
    : npp_(npp),
      status_(kWaitingForNew),
      command_buffer_(new CommandBufferService),
      processor_(new GPUProcessor(command_buffer_.get())) {
  memset(&window_, 0, sizeof(window_));
}

NPError GPUPluginObject::New(NPMIMEType plugin_type,
                             int16 argc,
                             char* argn[],
                             char* argv[],
                             NPSavedData* saved) {
  if (status_ != kWaitingForNew)
    return NPERR_GENERIC_ERROR;

  status_ = kWaitingForSetWindow;

  return NPERR_NO_ERROR;
}

NPError GPUPluginObject::SetWindow(NPWindow* new_window) {
  if (status_ == kWaitingForNew || status_ == kDestroyed)
    return NPERR_GENERIC_ERROR;

  // PlatformSpecificSetWindow advances the status depending on what happens.
  NPError error = PlatformSpecificSetWindow(new_window);
  if (error == NPERR_NO_ERROR) {
    window_ = *new_window;

    if (event_sync_.Get()) {
      NPInvokeVoid(npp_,
                   event_sync_,
                   "resize",
                   static_cast<int32>(window_.width),
                   static_cast<int32>(window_.height));
    }
  } else {
    memset(&window_, 0, sizeof(window_));
  }

  return error;
}

int16 GPUPluginObject::HandleEvent(NPEvent* event) {
  return 0;
}

NPError GPUPluginObject::Destroy(NPSavedData** saved) {
  if (status_ == kWaitingForNew || status_ == kDestroyed)
    return NPERR_GENERIC_ERROR;

  if (command_buffer_.get()) {
    command_buffer_->SetPutOffsetChangeCallback(NULL);
  }

  status_ = kDestroyed;

  return NPERR_NO_ERROR;
}

void GPUPluginObject::Release() {
  DCHECK(status_ == kWaitingForNew || status_ == kDestroyed);
  NPBrowser::get()->ReleaseObject(this);
}

NPObject*GPUPluginObject::GetScriptableNPObject() {
  NPBrowser::get()->RetainObject(this);
  return this;
}

CommandBuffer* GPUPluginObject::OpenCommandBuffer() {
  if (status_ == kInitializationSuccessful)
    return command_buffer_.get();

  // SetWindow must have been called before OpenCommandBuffer.
  // PlatformSpecificSetWindow advances the status to
  // kWaitingForOpenCommandBuffer.
  if (status_ != kWaitingForOpenCommandBuffer)
    return NULL;

  scoped_ptr<SharedMemory> ring_buffer(new SharedMemory);
  if (!ring_buffer->Create(std::wstring(), false, false, kCommandBufferSize))
    return NULL;

  if (command_buffer_->Initialize(ring_buffer.release())) {
    if (processor_->Initialize(static_cast<HWND>(window_.window))) {
      command_buffer_->SetPutOffsetChangeCallback(
          NewCallback(processor_.get(),
                      &GPUProcessor::ProcessCommands));
      status_ = kInitializationSuccessful;
      return command_buffer_.get();
    }
  }

  return NULL;
}

}  // namespace gpu_plugin
