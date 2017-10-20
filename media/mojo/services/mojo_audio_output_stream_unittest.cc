// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_output_stream.h"

#include <utility>

#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sync_socket.h"
#include "media/audio/audio_output_controller.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

const double kNewVolume = 0.618;
// Not actually used, but sent from the AudioOutputDelegate.
const int kStreamId = 0;
const int kShmemSize = 100;

using testing::_;
using testing::Mock;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using testing::Test;
using AudioOutputStream = mojom::AudioOutputStream;
using AudioOutputStreamPtr = mojo::InterfacePtr<AudioOutputStream>;

class TestCancelableSyncSocket : public base::CancelableSyncSocket {
 public:
  TestCancelableSyncSocket() {}

  void ExpectOwnershipTransfer() { expect_ownership_transfer_ = true; }

  ~TestCancelableSyncSocket() override {
    // When the handle is sent over mojo, mojo takes ownership over it and
    // closes it. We have to make sure we do not also retain the handle in the
    // sync socket, as the sync socket closes the handle on destruction.
    if (expect_ownership_transfer_)
      EXPECT_EQ(handle(), kInvalidHandle);
  }

 private:
  bool expect_ownership_transfer_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestCancelableSyncSocket);
};

class MockDelegate : public AudioOutputDelegate {
 public:
  MockDelegate() {}
  ~MockDelegate() {}

  MOCK_METHOD0(GetStreamId, int());
  MOCK_METHOD0(OnPlayStream, void());
  MOCK_METHOD0(OnPauseStream, void());
  MOCK_METHOD1(OnSetVolume, void(double));
};

class MockDelegateFactory {
 public:
  void PrepareDelegateForCreation(
      std::unique_ptr<AudioOutputDelegate> delegate) {
    ASSERT_EQ(nullptr, delegate_);
    delegate_.swap(delegate);
  }

  std::unique_ptr<AudioOutputDelegate> CreateDelegate(
      AudioOutputDelegate::EventHandler* handler) {
    MockCreateDelegate(handler);
    EXPECT_NE(nullptr, delegate_);
    return std::move(delegate_);
  }

  MOCK_METHOD1(MockCreateDelegate, void(AudioOutputDelegate::EventHandler*));

 private:
  std::unique_ptr<AudioOutputDelegate> delegate_;
};

class MockDeleter {
 public:
  MOCK_METHOD0(Finished, void());
};

class MockClient : public mojom::AudioOutputStreamClient {
 public:
  MockClient() {}

  void Initialized(mojo::ScopedSharedBufferHandle shared_buffer,
                   mojo::ScopedHandle socket_handle) {
    ASSERT_TRUE(shared_buffer.is_valid());
    ASSERT_TRUE(socket_handle.is_valid());

    base::PlatformFile fd;
    mojo::UnwrapPlatformFile(std::move(socket_handle), &fd);
    socket_ = base::MakeUnique<base::CancelableSyncSocket>(fd);
    EXPECT_NE(socket_->handle(), base::CancelableSyncSocket::kInvalidHandle);

    size_t memory_length;
    base::SharedMemoryHandle shmem_handle;
    bool read_only;
    EXPECT_EQ(
        mojo::UnwrapSharedMemoryHandle(std::move(shared_buffer), &shmem_handle,
                                       &memory_length, &read_only),
        MOJO_RESULT_OK);
    EXPECT_FALSE(read_only);
    buffer_ = base::MakeUnique<base::SharedMemory>(shmem_handle, read_only);

    GotNotification();
  }

  MOCK_METHOD0(GotNotification, void());

  MOCK_METHOD0(OnError, void());

 private:
  std::unique_ptr<base::SharedMemory> buffer_;
  std::unique_ptr<base::CancelableSyncSocket> socket_;
};

std::unique_ptr<AudioOutputDelegate> CreateNoDelegate(
    AudioOutputDelegate::EventHandler* event_handler) {
  return nullptr;
}

void NotCalled(mojo::ScopedSharedBufferHandle shared_buffer,
               mojo::ScopedHandle socket_handle) {
  EXPECT_TRUE(false) << "The StreamCreated callback was called despite the "
                        "test expecting it not to.";
}

}  // namespace

class MojoAudioOutputStreamTest : public Test {
 public:
  MojoAudioOutputStreamTest()
      : foreign_socket_(base::MakeUnique<TestCancelableSyncSocket>()),
        client_binding_(&client_, mojo::MakeRequest(&client_ptr_)) {}

  AudioOutputStreamPtr CreateAudioOutput() {
    AudioOutputStreamPtr p;
    ExpectDelegateCreation();
    impl_ = base::MakeUnique<MojoAudioOutputStream>(
        mojo::MakeRequest(&p), std::move(client_ptr_),
        base::BindOnce(&MockDelegateFactory::CreateDelegate,
                       base::Unretained(&mock_delegate_factory_)),
        base::Bind(&MockClient::Initialized, base::Unretained(&client_)),
        base::BindOnce(&MockDeleter::Finished, base::Unretained(&deleter_)));
    EXPECT_TRUE(p.is_bound());
    return p;
  }

 protected:
  void ExpectDelegateCreation() {
    delegate_ = new StrictMock<MockDelegate>();
    mock_delegate_factory_.PrepareDelegateForCreation(
        base::WrapUnique(delegate_));
    EXPECT_TRUE(
        base::CancelableSyncSocket::CreatePair(&local_, foreign_socket_.get()));
    EXPECT_TRUE(mem_.CreateAnonymous(kShmemSize));
    EXPECT_CALL(mock_delegate_factory_, MockCreateDelegate(NotNull()))
        .WillOnce(SaveArg<0>(&delegate_event_handler_));
  }

  base::MessageLoop loop_;
  base::CancelableSyncSocket local_;
  std::unique_ptr<TestCancelableSyncSocket> foreign_socket_;
  base::SharedMemory mem_;
  StrictMock<MockDelegate>* delegate_ = nullptr;
  AudioOutputDelegate::EventHandler* delegate_event_handler_ = nullptr;
  StrictMock<MockDelegateFactory> mock_delegate_factory_;
  StrictMock<MockDeleter> deleter_;
  StrictMock<MockClient> client_;
  media::mojom::AudioOutputStreamClientPtr client_ptr_;
  mojo::Binding<media::mojom::AudioOutputStreamClient> client_binding_;
  std::unique_ptr<MojoAudioOutputStream> impl_;
};

TEST_F(MojoAudioOutputStreamTest, NoDelegate_SignalsError) {
  bool deleter_called = false;
  EXPECT_CALL(client_, OnError()).Times(1);
  mojom::AudioOutputStreamPtr stream_ptr;
  MojoAudioOutputStream stream(
      mojo::MakeRequest(&stream_ptr), std::move(client_ptr_),
      base::BindOnce(&CreateNoDelegate), base::Bind(&NotCalled),
      base::BindOnce([](bool* p) { *p = true; }, &deleter_called));
  EXPECT_FALSE(deleter_called)
      << "Stream shouldn't call the deleter from its constructor.";
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(deleter_called);
}

TEST_F(MojoAudioOutputStreamTest, Play_Plays) {
  AudioOutputStreamPtr audio_output_ptr = CreateAudioOutput();
  EXPECT_CALL(*delegate_, OnPlayStream());

  audio_output_ptr->Play();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoAudioOutputStreamTest, Pause_Pauses) {
  AudioOutputStreamPtr audio_output_ptr = CreateAudioOutput();
  EXPECT_CALL(*delegate_, OnPauseStream());

  audio_output_ptr->Pause();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoAudioOutputStreamTest, SetVolume_SetsVolume) {
  AudioOutputStreamPtr audio_output_ptr = CreateAudioOutput();
  EXPECT_CALL(*delegate_, OnSetVolume(kNewVolume));

  audio_output_ptr->SetVolume(kNewVolume);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoAudioOutputStreamTest, DestructWithCallPending_Safe) {
  AudioOutputStreamPtr audio_output_ptr = CreateAudioOutput();
  EXPECT_CALL(client_, GotNotification());
  base::RunLoop().RunUntilIdle();

  ASSERT_NE(nullptr, delegate_event_handler_);
  foreign_socket_->ExpectOwnershipTransfer();
  delegate_event_handler_->OnStreamCreated(kStreamId, &mem_,
                                           std::move(foreign_socket_));
  audio_output_ptr->Play();
  impl_.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoAudioOutputStreamTest, Created_NotifiesClient) {
  AudioOutputStreamPtr audio_output_ptr = CreateAudioOutput();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(client_, GotNotification());

  ASSERT_NE(nullptr, delegate_event_handler_);
  foreign_socket_->ExpectOwnershipTransfer();
  delegate_event_handler_->OnStreamCreated(kStreamId, &mem_,
                                           std::move(foreign_socket_));

  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoAudioOutputStreamTest, SetVolumeTooLarge_Error) {
  AudioOutputStreamPtr audio_output_ptr = CreateAudioOutput();
  EXPECT_CALL(deleter_, Finished());
  EXPECT_CALL(client_, OnError()).Times(1);

  audio_output_ptr->SetVolume(15);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&deleter_);
}

TEST_F(MojoAudioOutputStreamTest, SetVolumeNegative_Error) {
  AudioOutputStreamPtr audio_output_ptr = CreateAudioOutput();
  EXPECT_CALL(deleter_, Finished());
  EXPECT_CALL(client_, OnError()).Times(1);

  audio_output_ptr->SetVolume(-0.5);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&deleter_);
}

TEST_F(MojoAudioOutputStreamTest, DelegateErrorBeforeCreated_PropagatesError) {
  AudioOutputStreamPtr audio_output_ptr = CreateAudioOutput();
  EXPECT_CALL(deleter_, Finished());
  EXPECT_CALL(client_, OnError()).Times(1);

  ASSERT_NE(nullptr, delegate_event_handler_);
  delegate_event_handler_->OnStreamError(kStreamId);

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&deleter_);
}

TEST_F(MojoAudioOutputStreamTest, DelegateErrorAfterCreated_PropagatesError) {
  AudioOutputStreamPtr audio_output_ptr = CreateAudioOutput();
  EXPECT_CALL(client_, GotNotification());
  EXPECT_CALL(deleter_, Finished());
  EXPECT_CALL(client_, OnError()).Times(1);
  base::RunLoop().RunUntilIdle();

  ASSERT_NE(nullptr, delegate_event_handler_);
  foreign_socket_->ExpectOwnershipTransfer();
  delegate_event_handler_->OnStreamCreated(kStreamId, &mem_,
                                           std::move(foreign_socket_));
  delegate_event_handler_->OnStreamError(kStreamId);

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&deleter_);
}

TEST_F(MojoAudioOutputStreamTest, RemoteEndGone_Error) {
  AudioOutputStreamPtr audio_output_ptr = CreateAudioOutput();
  EXPECT_CALL(deleter_, Finished());
  audio_output_ptr.reset();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&deleter_);
}

}  // namespace media
