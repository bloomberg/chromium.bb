// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mojo_audio_output_ipc.h"

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
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using testing::_;
using testing::AtLeast;
using testing::Mock;
using testing::StrictMock;

namespace content {

namespace {

const int kSessionId = 1234;
const size_t kMemoryLength = 4321;
const char kDeviceId[] = "device_id";
const char kReturnedDeviceId[] = "returned_device_id";
const double kNewVolume = 0.271828;

url::Origin Origin() {
  return {};
}

media::AudioParameters Params() {
  return media::AudioParameters::UnavailableDeviceParams();
}

MojoAudioOutputIPC::FactoryAccessorCB NullAccessor() {
  return base::BindRepeating(
      []() -> mojom::RendererAudioOutputStreamFactory* { return nullptr; });
}

class TestRemoteFactory : public mojom::RendererAudioOutputStreamFactory {
 public:
  TestRemoteFactory()
      : expect_request_(false),
        binding_(this, mojo::MakeRequest(&this_proxy_)) {}

  ~TestRemoteFactory() override {}

  void RequestDeviceAuthorization(
      media::mojom::AudioOutputStreamProviderRequest stream_provider_request,
      int64_t session_id,
      const std::string& device_id,
      RequestDeviceAuthorizationCallback callback) override {
    EXPECT_EQ(session_id, expected_session_id_);
    EXPECT_EQ(device_id, expected_device_id_);
    EXPECT_TRUE(expect_request_);
    if (provider_) {
      std::move(callback).Run(
          media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK, Params(),
          std::string(kReturnedDeviceId));
      provider_binding_.emplace(provider_.get(),
                                std::move(stream_provider_request));
    } else {
      std::move(callback).Run(
          media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED,
          Params(), std::string(""));
    }
    expect_request_ = false;
  }

  void PrepareProviderForAuthorization(
      int64_t session_id,
      const std::string& device_id,
      std::unique_ptr<media::mojom::AudioOutputStreamProvider> provider) {
    EXPECT_FALSE(expect_request_);
    expect_request_ = true;
    expected_session_id_ = session_id;
    expected_device_id_ = device_id;
    provider_binding_.reset();
    std::swap(provider_, provider);
  }

  void RefuseNextRequest(int64_t session_id, const std::string& device_id) {
    EXPECT_FALSE(expect_request_);
    expect_request_ = true;
    expected_session_id_ = session_id;
    expected_device_id_ = device_id;
  }

  void Disconnect() {
    binding_.Close();
    this_proxy_.reset();
    binding_.Bind(mojo::MakeRequest(&this_proxy_));
    provider_binding_.reset();
    provider_.reset();
    expect_request_ = false;
  }

  MojoAudioOutputIPC::FactoryAccessorCB GetAccessor() {
    return base::BindRepeating(&TestRemoteFactory::get, base::Unretained(this));
  }

 private:
  mojom::RendererAudioOutputStreamFactory* get() { return this_proxy_.get(); }

  bool expect_request_;
  int64_t expected_session_id_;
  std::string expected_device_id_;

  mojom::RendererAudioOutputStreamFactoryPtr this_proxy_;
  mojo::Binding<mojom::RendererAudioOutputStreamFactory> binding_;
  std::unique_ptr<media::mojom::AudioOutputStreamProvider> provider_;
  base::Optional<mojo::Binding<media::mojom::AudioOutputStreamProvider>>
      provider_binding_;
};

class TestStreamProvider : public media::mojom::AudioOutputStreamProvider {
 public:
  explicit TestStreamProvider(media::mojom::AudioOutputStream* stream)
      : stream_(stream) {}

  ~TestStreamProvider() override {
    // If we expected a stream to be acquired, make sure it is so.
    if (stream_)
      EXPECT_TRUE(binding_);
  }

  void Acquire(media::mojom::AudioOutputStreamRequest stream_request,
               const media::AudioParameters& params,
               const AcquireCallback& callback) override {
    EXPECT_EQ(binding_, base::nullopt);
    EXPECT_NE(stream_, nullptr);
    binding_.emplace(stream_, std::move(stream_request));
    base::CancelableSyncSocket foreign_socket;
    EXPECT_TRUE(
        base::CancelableSyncSocket::CreatePair(&socket_, &foreign_socket));
    std::move(callback).Run(mojo::SharedBufferHandle::Create(kMemoryLength),
                            mojo::WrapPlatformFile(foreign_socket.Release()));
  }

 private:
  media::mojom::AudioOutputStream* stream_;
  base::Optional<mojo::Binding<media::mojom::AudioOutputStream>> binding_;
  base::CancelableSyncSocket socket_;
};

class MockStream : public media::mojom::AudioOutputStream {
 public:
  MOCK_METHOD0(Play, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD1(SetVolume, void(double));
};

class MockDelegate : public media::AudioOutputIPCDelegate {
 public:
  MockDelegate() {}
  ~MockDelegate() override {}

  void OnStreamCreated(base::SharedMemoryHandle mem_handle,
                       base::SyncSocket::Handle socket_handle,
                       int length) {
    base::SharedMemory sh_mem(
        mem_handle, /*read_only*/ false);  // Releases the shared memory handle.
    base::SyncSocket socket(socket_handle);  // Releases the socket descriptor.
    GotOnStreamCreated(length);
  }

  MOCK_METHOD0(OnError, void());
  MOCK_METHOD3(OnDeviceAuthorized,
               void(media::OutputDeviceStatus device_status,
                    const media::AudioParameters& output_params,
                    const std::string& matched_device_id));
  MOCK_METHOD1(GotOnStreamCreated, void(int length));
  MOCK_METHOD0(OnIPCClosed, void());
};

}  // namespace

TEST(MojoAudioOutputIPC, AuthorizeWithoutFactory_CallsAuthorizedWithError) {
  base::MessageLoopForIO message_loop;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      base::MakeUnique<MojoAudioOutputIPC>(NullAccessor());

  EXPECT_CALL(delegate,
              OnDeviceAuthorized(media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL, _,
                                 std::string()));
  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId, Origin());
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, DeviceAuthorized_Propagates) {
  base::MessageLoopForIO message_loop;
  TestRemoteFactory stream_factory;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      base::MakeUnique<MojoAudioOutputIPC>(stream_factory.GetAccessor());
  stream_factory.PrepareProviderForAuthorization(
      kSessionId, kDeviceId, base::MakeUnique<TestStreamProvider>(nullptr));

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId, Origin());

  EXPECT_CALL(delegate, OnDeviceAuthorized(
                            media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                            _, std::string(kReturnedDeviceId)));
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, OnDeviceCreated_Propagates) {
  base::MessageLoopForIO message_loop;
  TestRemoteFactory stream_factory;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      base::MakeUnique<MojoAudioOutputIPC>(stream_factory.GetAccessor());
  stream_factory.PrepareProviderForAuthorization(
      kSessionId, kDeviceId, base::MakeUnique<TestStreamProvider>(&stream));

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId, Origin());
  ipc->CreateStream(&delegate, Params());

  EXPECT_CALL(delegate, OnDeviceAuthorized(
                            media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                            _, std::string(kReturnedDeviceId)));
  EXPECT_CALL(delegate, GotOnStreamCreated(kMemoryLength));
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC,
     CreateWithoutAuthorization_RequestsAuthorizationFirst) {
  base::MessageLoopForIO message_loop;
  TestRemoteFactory stream_factory;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;
  const std::unique_ptr<media::AudioOutputIPC> ipc =
      base::MakeUnique<MojoAudioOutputIPC>(stream_factory.GetAccessor());

  // Note: This call implicitly EXPECTs that authorization is requested,
  // and constructing the TestStreamProvider with a |&stream| EXPECTs that the
  // stream is created. This implicit request should always be for the default
  // device and no session id.
  stream_factory.PrepareProviderForAuthorization(
      0, std::string(media::AudioDeviceDescription::kDefaultDeviceId),
      base::MakeUnique<TestStreamProvider>(&stream));

  ipc->CreateStream(&delegate, Params());

  EXPECT_CALL(delegate, GotOnStreamCreated(kMemoryLength));
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, IsReusable) {
  base::MessageLoopForIO message_loop;
  TestRemoteFactory stream_factory;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      base::MakeUnique<MojoAudioOutputIPC>(stream_factory.GetAccessor());

  for (int i = 0; i < 5; ++i) {
    stream_factory.PrepareProviderForAuthorization(
        kSessionId, kDeviceId, base::MakeUnique<TestStreamProvider>(&stream));

    ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId, Origin());
    ipc->CreateStream(&delegate, Params());

    EXPECT_CALL(
        delegate,
        OnDeviceAuthorized(media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                           _, std::string(kReturnedDeviceId)));
    EXPECT_CALL(delegate, GotOnStreamCreated(kMemoryLength));
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&delegate);

    ipc->CloseStream();
    base::RunLoop().RunUntilIdle();
  }
}

TEST(MojoAudioOutputIPC, IsReusableAfterError) {
  base::MessageLoopForIO message_loop;
  TestRemoteFactory stream_factory;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      base::MakeUnique<MojoAudioOutputIPC>(stream_factory.GetAccessor());

  stream_factory.PrepareProviderForAuthorization(
      kSessionId, kDeviceId, base::MakeUnique<TestStreamProvider>(nullptr));
  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId, Origin());

  EXPECT_CALL(delegate, OnDeviceAuthorized(
                            media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                            _, std::string(kReturnedDeviceId)));
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate);

  EXPECT_CALL(delegate, OnError()).Times(AtLeast(1));
  stream_factory.Disconnect();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate);

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();

  for (int i = 0; i < 5; ++i) {
    stream_factory.PrepareProviderForAuthorization(
        kSessionId, kDeviceId, base::MakeUnique<TestStreamProvider>(&stream));

    ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId, Origin());
    ipc->CreateStream(&delegate, Params());

    EXPECT_CALL(
        delegate,
        OnDeviceAuthorized(media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                           _, std::string(kReturnedDeviceId)));
    EXPECT_CALL(delegate, GotOnStreamCreated(kMemoryLength));
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&delegate);

    ipc->CloseStream();
    base::RunLoop().RunUntilIdle();
  }
}

TEST(MojoAudioOutputIPC, DeviceNotAuthorized_Propagates) {
  base::MessageLoopForIO message_loop;
  TestRemoteFactory stream_factory;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      base::MakeUnique<MojoAudioOutputIPC>(stream_factory.GetAccessor());
  stream_factory.RefuseNextRequest(kSessionId, kDeviceId);

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId, Origin());

  EXPECT_CALL(
      delegate,
      OnDeviceAuthorized(
          media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED,
          _, std::string()));
  EXPECT_CALL(delegate, OnError()).Times(AtLeast(0));
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC,
     FactoryDisconnectedBeforeAuthorizationReply_CallsAuthorizedAnyways) {
  // The authorization IPC message might be aborted by the remote end
  // disconnecting. In this case, the MojoAudioOutputIPC object must still
  // send a notification to unblock the AudioOutputIPCDelegate.
  base::MessageLoopForIO message_loop;
  TestRemoteFactory stream_factory;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      base::MakeUnique<MojoAudioOutputIPC>(stream_factory.GetAccessor());

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId, Origin());

  EXPECT_CALL(
      delegate,
      OnDeviceAuthorized(
          media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL, _,
          std::string()));
  stream_factory.Disconnect();
  EXPECT_CALL(delegate, OnError());
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC,
     FactoryDisconnectedAfterAuthorizationReply_CallsAuthorizedOnlyOnce) {
  // This test makes sure that the MojoAudioOutputIPC doesn't callback for
  // authorization when the factory disconnects if it already got a callback
  // for authorization.
  base::MessageLoopForIO message_loop;
  TestRemoteFactory stream_factory;
  stream_factory.PrepareProviderForAuthorization(
      kSessionId, kDeviceId, base::MakeUnique<TestStreamProvider>(nullptr));
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      base::MakeUnique<MojoAudioOutputIPC>(stream_factory.GetAccessor());

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId, Origin());

  EXPECT_CALL(delegate, OnDeviceAuthorized(
                            media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                            _, std::string(kReturnedDeviceId)));
  base::RunLoop().RunUntilIdle();

  stream_factory.Disconnect();
  EXPECT_CALL(delegate, OnError());
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, AuthorizeNoClose_DCHECKs) {
  base::MessageLoopForIO message_loop;
  TestRemoteFactory stream_factory;
  StrictMock<MockDelegate> delegate;

  stream_factory.PrepareProviderForAuthorization(
      kSessionId, kDeviceId, base::MakeUnique<TestStreamProvider>(nullptr));

  std::unique_ptr<media::AudioOutputIPC> ipc =
      base::MakeUnique<MojoAudioOutputIPC>(stream_factory.GetAccessor());

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId, Origin());
  EXPECT_DCHECK_DEATH(ipc.reset());
  ipc->CloseStream();
  ipc.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, CreateNoClose_DCHECKs) {
  base::MessageLoopForIO message_loop;
  TestRemoteFactory stream_factory;
  StrictMock<MockDelegate> delegate;
  StrictMock<MockStream> stream;

  stream_factory.PrepareProviderForAuthorization(
      0, std::string(media::AudioDeviceDescription::kDefaultDeviceId),
      base::MakeUnique<TestStreamProvider>(&stream));

  std::unique_ptr<media::AudioOutputIPC> ipc =
      base::MakeUnique<MojoAudioOutputIPC>(stream_factory.GetAccessor());

  ipc->CreateStream(&delegate, Params());
  EXPECT_DCHECK_DEATH(ipc.reset());
  ipc->CloseStream();
  ipc.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, Play_Plays) {
  base::MessageLoopForIO message_loop;
  TestRemoteFactory stream_factory;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      base::MakeUnique<MojoAudioOutputIPC>(stream_factory.GetAccessor());
  stream_factory.PrepareProviderForAuthorization(
      kSessionId, kDeviceId, base::MakeUnique<TestStreamProvider>(&stream));

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId, Origin());
  ipc->CreateStream(&delegate, Params());
  ipc->PlayStream();

  EXPECT_CALL(delegate, OnDeviceAuthorized(
                            media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                            _, std::string(kReturnedDeviceId)));
  EXPECT_CALL(delegate, GotOnStreamCreated(kMemoryLength));
  EXPECT_CALL(stream, Play());
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, Pause_Pauses) {
  base::MessageLoopForIO message_loop;
  TestRemoteFactory stream_factory;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      base::MakeUnique<MojoAudioOutputIPC>(stream_factory.GetAccessor());
  stream_factory.PrepareProviderForAuthorization(
      kSessionId, kDeviceId, base::MakeUnique<TestStreamProvider>(&stream));

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId, Origin());
  ipc->CreateStream(&delegate, Params());
  ipc->PauseStream();

  EXPECT_CALL(delegate, OnDeviceAuthorized(
                            media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                            _, std::string(kReturnedDeviceId)));
  EXPECT_CALL(delegate, GotOnStreamCreated(kMemoryLength));
  EXPECT_CALL(stream, Pause());
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, SetVolume_SetsVolume) {
  base::MessageLoopForIO message_loop;
  TestRemoteFactory stream_factory;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      base::MakeUnique<MojoAudioOutputIPC>(stream_factory.GetAccessor());
  stream_factory.PrepareProviderForAuthorization(
      kSessionId, kDeviceId, base::MakeUnique<TestStreamProvider>(&stream));

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId, Origin());
  ipc->CreateStream(&delegate, Params());
  ipc->SetVolume(kNewVolume);

  EXPECT_CALL(delegate, OnDeviceAuthorized(
                            media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                            _, std::string(kReturnedDeviceId)));
  EXPECT_CALL(delegate, GotOnStreamCreated(kMemoryLength));
  EXPECT_CALL(stream, SetVolume(kNewVolume));
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
