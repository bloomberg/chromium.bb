// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/omx/mock_omx.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

MockOmx* MockOmx::instance_ = NULL;

// Static stub methods. They redirect method calls back to the mock object.
static OMX_ERRORTYPE MockSendCommand(OMX_HANDLETYPE component,
                                     OMX_COMMANDTYPE command,
                                     OMX_U32 param1,
                                     OMX_PTR command_data) {
  CHECK(MockOmx::get()->component() ==
        reinterpret_cast<OMX_COMPONENTTYPE*>(component));
  return MockOmx::get()->SendCommand(command, param1, command_data);
}

static OMX_ERRORTYPE MockGetParameter(OMX_HANDLETYPE component,
                                      OMX_INDEXTYPE param_index,
                                      OMX_PTR structure) {
  CHECK(MockOmx::get()->component() ==
        reinterpret_cast<OMX_COMPONENTTYPE*>(component));
  return MockOmx::get()->GetParameter(param_index, structure);
}

static OMX_ERRORTYPE MockSetParameter(OMX_HANDLETYPE component,
                                      OMX_INDEXTYPE param_index,
                                      OMX_PTR structure) {
  CHECK(MockOmx::get()->component() ==
        reinterpret_cast<OMX_COMPONENTTYPE*>(component));
  return MockOmx::get()->SetParameter(param_index, structure);
}

static OMX_ERRORTYPE MockGetConfig(OMX_HANDLETYPE component,
                                   OMX_INDEXTYPE index,
                                   OMX_PTR structure) {
  CHECK(MockOmx::get()->component() ==
        reinterpret_cast<OMX_COMPONENTTYPE*>(component));
  return MockOmx::get()->GetConfig(index, structure);
}

static OMX_ERRORTYPE MockSetConfig(OMX_HANDLETYPE component,
                                   OMX_INDEXTYPE index,
                                   OMX_PTR structure) {
  CHECK(MockOmx::get()->component() ==
        reinterpret_cast<OMX_COMPONENTTYPE*>(component));
  return MockOmx::get()->SetConfig(index, structure);
}

static OMX_ERRORTYPE MockAllocateBuffer(OMX_HANDLETYPE component,
                                        OMX_BUFFERHEADERTYPE** buffer,
                                        OMX_U32 port_index,
                                        OMX_PTR app_private,
                                        OMX_U32 size_bytes) {
  CHECK(MockOmx::get()->component() ==
        reinterpret_cast<OMX_COMPONENTTYPE*>(component));
  return MockOmx::get()->AllocateBuffer(buffer, port_index, app_private,
                                        size_bytes);
}

static OMX_ERRORTYPE MockUseBuffer(OMX_HANDLETYPE component,
                                   OMX_BUFFERHEADERTYPE** buffer,
                                   OMX_U32 port_index,
                                   OMX_PTR app_private,
                                   OMX_U32 size_bytes,
                                   OMX_U8* pBuffer) {
  CHECK(MockOmx::get()->component() ==
        reinterpret_cast<OMX_COMPONENTTYPE*>(component));
  return MockOmx::get()->UseBuffer(buffer, port_index, app_private,
                                   size_bytes, pBuffer);
}

static OMX_ERRORTYPE MockFreeBuffer(OMX_HANDLETYPE component,
                                    OMX_U32 port_index,
                                    OMX_BUFFERHEADERTYPE* buffer) {
  CHECK(MockOmx::get()->component() ==
        reinterpret_cast<OMX_COMPONENTTYPE*>(component));
  return MockOmx::get()->FreeBuffer(port_index, buffer);
}

static OMX_ERRORTYPE MockEmptyThisBuffer(OMX_HANDLETYPE component,
                                         OMX_BUFFERHEADERTYPE* buffer) {
  CHECK(MockOmx::get()->component() ==
        reinterpret_cast<OMX_COMPONENTTYPE*>(component));
  return MockOmx::get()->EmptyThisBuffer(buffer);
}

static OMX_ERRORTYPE MockFillThisBuffer(OMX_HANDLETYPE component,
                                        OMX_BUFFERHEADERTYPE* buffer) {
  CHECK(MockOmx::get()->component() ==
        reinterpret_cast<OMX_COMPONENTTYPE*>(component));
  return MockOmx::get()->FillThisBuffer(buffer);
}

// Stub methods to export symbols used for OpenMAX.
extern "C" {
OMX_ERRORTYPE OMX_Init() {
  return MockOmx::get()->Init();
}

OMX_ERRORTYPE OMX_Deinit() {
  return MockOmx::get()->Deinit();
}

OMX_ERRORTYPE OMX_GetHandle(
    OMX_HANDLETYPE* handle, OMX_STRING name, OMX_PTR app_private,
    OMX_CALLBACKTYPE* callbacks) {
  return MockOmx::get()->GetHandle(handle, name, app_private, callbacks);
}

OMX_ERRORTYPE OMX_FreeHandle(OMX_HANDLETYPE handle) {
  return MockOmx::get()->FreeHandle(handle);
}

OMX_ERRORTYPE OMX_GetComponentsOfRole(OMX_STRING name, OMX_U32* roles,
                                      OMX_U8** component_names) {
  return MockOmx::get()->GetComponentsOfRole(name, roles, component_names);
}
}  // extern "C"

MockOmx::MockOmx() {
  memset(&callbacks_, 0, sizeof(callbacks_));
  memset(&component_, 0, sizeof(component_));

  // Setup the function pointers to the static methods. They will redirect back
  // to this mock object.
  component_.SendCommand = &MockSendCommand;
  component_.GetParameter = &MockGetParameter;
  component_.SetParameter = &MockSetParameter;
  component_.GetConfig = &MockGetConfig;
  component_.SetConfig = &MockSetConfig;
  component_.AllocateBuffer = &MockAllocateBuffer;
  component_.UseBuffer = &MockUseBuffer;
  component_.FreeBuffer = &MockFreeBuffer;
  component_.EmptyThisBuffer = &MockEmptyThisBuffer;
  component_.FillThisBuffer = &MockFillThisBuffer;

  // Save this instance to static member.
  CHECK(!instance_);
  instance_ = this;
}

MockOmx::~MockOmx() {
  CHECK(instance_);
  instance_ = NULL;
}

// static
MockOmx* MockOmx::get() {
  return instance_;
}

}  // namespace media
