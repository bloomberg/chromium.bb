// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_MOJO_MEDIA_STREAM_DISPATCHER_HOST_H_
#define CONTENT_RENDERER_MEDIA_MOCK_MOJO_MEDIA_STREAM_DISPATCHER_HOST_H_

#include <string>

#include "base/macros.h"
#include "content/common/media/media_stream.mojom.h"
#include "content/common/media/media_stream_controls.h"
#include "content/public/common/media_stream_request.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockMojoMediaStreamDispatcherHost
    : public mojom::MediaStreamDispatcherHost {
 public:
  MockMojoMediaStreamDispatcherHost();
  ~MockMojoMediaStreamDispatcherHost();

  mojom::MediaStreamDispatcherHostPtr CreateInterfacePtrAndBind();

  MOCK_METHOD4(GenerateStream,
               void(int32_t, int32_t, const StreamControls&, bool));
  MOCK_METHOD2(CancelGenerateStream, void(int32_t, int32_t));
  MOCK_METHOD2(StopStreamDevice, void(int32_t, const std::string&));
  MOCK_METHOD4(OpenDevice,
               void(int32_t, int32_t, const std::string&, MediaStreamType));
  MOCK_METHOD1(CloseDevice, void(const std::string&));
  MOCK_METHOD3(SetCapturingLinkSecured, void(int32_t, MediaStreamType, bool));
  MOCK_METHOD1(StreamStarted, void(const std::string&));

 private:
  mojo::BindingSet<mojom::MediaStreamDispatcherHost> bindings_;

  DISALLOW_COPY_AND_ASSIGN(MockMojoMediaStreamDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MOCK_MOJO_MEDIA_STREAM_DISPATCHER_HOST_H_
