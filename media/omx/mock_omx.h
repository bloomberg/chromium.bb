// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_OMX_MOCK_OMX_H_
#define MEDIA_OMX_MOCK_OMX_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/openmax/il/OMX_Component.h"
#include "third_party/openmax/il/OMX_Core.h"

namespace media {

class MockOmx {
 public:
  MockOmx();
  virtual ~MockOmx();

  // The following mock methods are component specific.
  MOCK_METHOD3(SendCommand, OMX_ERRORTYPE(
      OMX_COMMANDTYPE command,
      OMX_U32 param1,
      OMX_PTR command_data));

  MOCK_METHOD2(GetParameter, OMX_ERRORTYPE(
      OMX_INDEXTYPE param_index,
      OMX_PTR structure));

  MOCK_METHOD2(SetParameter, OMX_ERRORTYPE(
      OMX_INDEXTYPE param_index,
      OMX_PTR structure));

  MOCK_METHOD2(GetConfig, OMX_ERRORTYPE(
      OMX_INDEXTYPE index,
      OMX_PTR structure));

  MOCK_METHOD2(SetConfig, OMX_ERRORTYPE(
      OMX_INDEXTYPE index,
      OMX_PTR structure));

  MOCK_METHOD4(AllocateBuffer, OMX_ERRORTYPE(
      OMX_BUFFERHEADERTYPE** buffer,
      OMX_U32 port_index,
      OMX_PTR app_private,
      OMX_U32 size_bytes));

  MOCK_METHOD5(UseBuffer, OMX_ERRORTYPE(
      OMX_BUFFERHEADERTYPE** buffer,
      OMX_U32 port_index,
      OMX_PTR app_private,
      OMX_U32 size_bytes,
      OMX_U8* pBuffer));

  MOCK_METHOD2(FreeBuffer, OMX_ERRORTYPE(
      OMX_U32 port_index,
      OMX_BUFFERHEADERTYPE* buffer));

  MOCK_METHOD1(EmptyThisBuffer, OMX_ERRORTYPE(
      OMX_BUFFERHEADERTYPE* buffer));

  MOCK_METHOD1(FillThisBuffer, OMX_ERRORTYPE(
      OMX_BUFFERHEADERTYPE* buffer));

  // The following mock methods are defined global.
  MOCK_METHOD0(Init, OMX_ERRORTYPE());
  MOCK_METHOD0(Deinit, OMX_ERRORTYPE());

  MOCK_METHOD4(GetHandle, OMX_ERRORTYPE(
      OMX_HANDLETYPE* handle,
      OMX_STRING name,
      OMX_PTR app_private,
      OMX_CALLBACKTYPE* callbacks));

  MOCK_METHOD1(FreeHandle, OMX_ERRORTYPE(
      OMX_HANDLETYPE handle));

  MOCK_METHOD3(GetComponentsOfRole, OMX_ERRORTYPE(
      OMX_STRING name,
      OMX_U32* roles,
      OMX_U8** component_names));

  OMX_CALLBACKTYPE* callbacks() { return &callbacks_; }
  OMX_COMPONENTTYPE* component() { return &component_; }

  // Getter for the global instance of MockOmx.
  static MockOmx* get();

 private:
  static MockOmx* instance_;

  OMX_CALLBACKTYPE callbacks_;
  OMX_COMPONENTTYPE component_;

  DISALLOW_COPY_AND_ASSIGN(MockOmx);
};

}  // namespace media

#endif  // MEDIA_OMX_MOCK_OMX_H_
