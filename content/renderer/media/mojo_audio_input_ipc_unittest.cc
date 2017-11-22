// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mojo_audio_input_ipc.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Mock;
using testing::StrictMock;

namespace content {

namespace {

const int kSessionId = 1234;
const size_t kMemoryLength = 4321;
const size_t kTotalSegments = 1;
const double kNewVolume = 0.271828;

media::AudioParameters Params() {
  return media::AudioParameters::UnavailableDeviceParams();
}

class MockStream : public media::mojom::AudioInputStream {
 public:
  MOCK_METHOD0(Record, void());
  MOCK_METHOD1(SetVolume, void(double));
};

class MockDelegate : public media::AudioInputIPCDelegate {
 public:
  MockDelegate() {}
  ~MockDelegate() override {}

  void OnStreamCreated(base::SharedMemoryHandle mem_handle,
                       base::SyncSocket::Handle socket_handle,
                       bool initially_muted) override {
    base::SharedMemory sh_mem(
        mem_handle, /*read_only*/ false);  // Releases the shared memory handle.
    base::SyncSocket socket(socket_handle);  // Releases the socket descriptor.
    GotOnStreamCreated(initially_muted);
  }

  MOCK_METHOD1(GotOnStreamCreated, void(bool initially_muted));
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(OnMuted, void(bool));
  MOCK_METHOD0(OnIPCClosed, void());
};

class FakeStreamCreator {
 public:
  FakeStreamCreator(media::mojom::AudioInputStream* stream,
                    bool initially_muted)
      : stream_(stream), initially_muted_(initially_muted) {}

  void Create(media::mojom::AudioInputStreamRequest stream_request,
              int64_t session_id,
              media::AudioParameters params,
              bool automatic_gain_control,
              uint32_t total_segments,
              media::mojom::AudioInputStreamClientPtr client,
              MojoAudioInputIPC::StreamCreatedCB on_stream_created) {
    EXPECT_EQ(binding_, base::nullopt);
    EXPECT_NE(stream_, nullptr);
    std::swap(client_, client);
    binding_.emplace(stream_, std::move(stream_request));
    base::CancelableSyncSocket foreign_socket;
    EXPECT_TRUE(
        base::CancelableSyncSocket::CreatePair(&socket_, &foreign_socket));
    std::move(on_stream_created)
        .Run(mojo::SharedBufferHandle::Create(kMemoryLength)
                 ->Clone(mojo::SharedBufferHandle::AccessMode::READ_ONLY),
             mojo::WrapPlatformFile(foreign_socket.Release()),
             initially_muted_);
  }

  MojoAudioInputIPC::StreamCreatorCB GetCallback() {
    return base::BindRepeating(&FakeStreamCreator::Create,
                               base::Unretained(this));
  }

  void Rearm() {
    binding_.reset();
    socket_.Close();
  }

  void SignalError() {
    ASSERT_TRUE(client_);
    client_->OnError();
  }

 private:
  media::mojom::AudioInputStream* stream_;
  media::mojom::AudioInputStreamClientPtr client_;
  base::Optional<mojo::Binding<media::mojom::AudioInputStream>> binding_;
  bool initially_muted_;
  base::CancelableSyncSocket socket_;
};

}  // namespace

TEST(MojoAudioInputIPC, OnStreamCreated_Propagates) {
  base::MessageLoopForIO message_loop;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, false);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(creator.GetCallback());

  EXPECT_CALL(delegate, GotOnStreamCreated(false));

  ipc->CreateStream(&delegate, kSessionId, Params(), false, kTotalSegments);
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioInputIPC, OnStreamCreated_PropagatesInitiallyMuted) {
  base::MessageLoopForIO message_loop;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, true);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(creator.GetCallback());

  EXPECT_CALL(delegate, GotOnStreamCreated(true));

  ipc->CreateStream(&delegate, kSessionId, Params(), false, kTotalSegments);
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioInputIPC, IsReusable) {
  base::MessageLoopForIO message_loop;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, false);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(creator.GetCallback());

  for (int i = 0; i < 5; ++i) {
    creator.Rearm();

    EXPECT_CALL(delegate, GotOnStreamCreated(_));

    ipc->CreateStream(&delegate, kSessionId, Params(), false, kTotalSegments);
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&delegate);

    ipc->CloseStream();
    base::RunLoop().RunUntilIdle();
  }
}

TEST(MojoAudioInputIPC, IsReusableAfterError) {
  base::MessageLoopForIO message_loop;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, false);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(creator.GetCallback());

  for (int i = 0; i < 5; ++i) {
    creator.Rearm();

    EXPECT_CALL(delegate, GotOnStreamCreated(_));

    ipc->CreateStream(&delegate, kSessionId, Params(), false, kTotalSegments);
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&delegate);

    EXPECT_CALL(delegate, OnError());
    creator.SignalError();
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&delegate);

    ipc->CloseStream();
    base::RunLoop().RunUntilIdle();
  }
}

TEST(MojoAudioInputIPC, Record_Records) {
  base::MessageLoopForIO message_loop;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, false);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(creator.GetCallback());

  EXPECT_CALL(delegate, GotOnStreamCreated(_));
  EXPECT_CALL(stream, Record());

  ipc->CreateStream(&delegate, kSessionId, Params(), false, kTotalSegments);
  ipc->RecordStream();
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioInputIPC, SetVolume_SetsVolume) {
  base::MessageLoopForIO message_loop;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, false);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(creator.GetCallback());

  EXPECT_CALL(delegate, GotOnStreamCreated(_));
  EXPECT_CALL(stream, SetVolume(kNewVolume));

  ipc->CreateStream(&delegate, kSessionId, Params(), false, kTotalSegments);
  ipc->SetVolume(kNewVolume);
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
