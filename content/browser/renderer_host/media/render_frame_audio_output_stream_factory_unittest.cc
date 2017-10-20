// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/render_frame_audio_output_stream_factory.h"

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/memory/shared_memory.h"
#include "base/memory/shared_memory_handle.h"
#include "base/run_loop.h"
#include "base/sync_socket.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/renderer_audio_output_stream_factory_context.h"
#include "content/common/media/renderer_audio_output_stream_factory.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/base/audio_parameters.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using testing::Test;
using AudioOutputStreamFactory = mojom::RendererAudioOutputStreamFactory;
using AudioOutputStreamFactoryPtr =
    mojo::InterfacePtr<AudioOutputStreamFactory>;
using AudioOutputStreamFactoryRequest =
    mojo::InterfaceRequest<AudioOutputStreamFactory>;
using AudioOutputStream = media::mojom::AudioOutputStream;
using AudioOutputStreamPtr = mojo::InterfacePtr<AudioOutputStream>;
using AudioOutputStreamRequest = mojo::InterfaceRequest<AudioOutputStream>;
using AudioOutputStreamClient = media::mojom::AudioOutputStreamClient;
using AudioOutputStreamClientPtr = mojo::InterfacePtr<AudioOutputStreamClient>;
using AudioOutputStreamClientRequest =
    mojo::InterfaceRequest<AudioOutputStreamClient>;
using AudioOutputStreamProvider = media::mojom::AudioOutputStreamProvider;
using AudioOutputStreamProviderPtr =
    mojo::InterfacePtr<AudioOutputStreamProvider>;
using AudioOutputStreamProviderRequest =
    mojo::InterfaceRequest<AudioOutputStreamProvider>;

const int kStreamId = 0;
const int kNoSessionId = 0;
const int kRenderProcessId = 42;
const int kRenderFrameId = 24;
const int kSampleFrequency = 44100;
const int kBitsPerSample = 16;
const int kSamplesPerBuffer = kSampleFrequency / 100;
const char kSalt[] = "salt";

media::AudioParameters GetTestAudioParameters() {
  return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_MONO, kSampleFrequency,
                                kBitsPerSample, kSamplesPerBuffer);
}

class MockAudioOutputDelegate : public media::AudioOutputDelegate {
 public:
  // |on_destruction| can be used to observe the destruction of the delegate.
  explicit MockAudioOutputDelegate(
      base::OnceClosure on_destruction = base::OnceClosure())
      : on_destruction_(std::move(on_destruction)) {}

  ~MockAudioOutputDelegate() {
    if (on_destruction_)
      std::move(on_destruction_).Run();
  }

  MOCK_METHOD0(GetStreamId, int());
  MOCK_METHOD0(OnPlayStream, void());
  MOCK_METHOD0(OnPauseStream, void());
  MOCK_METHOD1(OnSetVolume, void(double));

 private:
  base::OnceClosure on_destruction_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioOutputDelegate);
};

class MockContext : public RendererAudioOutputStreamFactoryContext {
 public:
  explicit MockContext(bool auth_ok) : salt_(kSalt), auth_ok_(auth_ok) {}

  ~MockContext() override { EXPECT_EQ(nullptr, delegate_); }

  int GetRenderProcessId() const override { return kRenderProcessId; }

  void RequestDeviceAuthorization(
      int render_frame_id,
      int session_id,
      const std::string& device_id,
      AuthorizationCompletedCallback cb) const override {
    EXPECT_EQ(render_frame_id, kRenderFrameId);
    EXPECT_EQ(session_id, 0);
    if (auth_ok_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(cb),
                         media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                         GetTestAudioParameters(), "default", std::string()));
      return;
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(cb),
                       media::OutputDeviceStatus::
                           OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED,
                       media::AudioParameters::UnavailableDeviceParams(),
                       std::string(), std::string()));
  }

  // The event handler for the delegate will be stored at
  // |*event_handler_location| when the delegate is created.
  void PrepareDelegateForCreation(
      std::unique_ptr<media::AudioOutputDelegate> delegate,
      media::AudioOutputDelegate::EventHandler** event_handler_location) {
    EXPECT_EQ(nullptr, delegate_);
    EXPECT_EQ(nullptr, delegate_event_handler_location_);
    delegate_ = std::move(delegate);
    delegate_event_handler_location_ = event_handler_location;
  }

  std::unique_ptr<media::AudioOutputDelegate> CreateDelegate(
      const std::string& unique_device_id,
      int render_frame_id,
      const media::AudioParameters& params,
      media::AudioOutputDelegate::EventHandler* handler) override {
    EXPECT_NE(nullptr, delegate_);
    EXPECT_NE(nullptr, delegate_event_handler_location_);
    *delegate_event_handler_location_ = handler;
    delegate_event_handler_location_ = nullptr;
    return std::move(delegate_);
  }

  AudioOutputStreamFactoryPtr CreateFactory() {
    DCHECK(!factory_);
    AudioOutputStreamFactoryPtr ret;
    factory_ = base::MakeUnique<RenderFrameAudioOutputStreamFactory>(
        kRenderFrameId, this);
    factory_binding_ = base::MakeUnique<
        mojo::Binding<mojom::RendererAudioOutputStreamFactory>>(
        factory_.get(), mojo::MakeRequest(&ret));
    return ret;
  }

 private:
  const std::string salt_;
  const bool auth_ok_;
  std::unique_ptr<RenderFrameAudioOutputStreamFactory> factory_;
  std::unique_ptr<mojo::Binding<mojom::RendererAudioOutputStreamFactory>>
      factory_binding_;
  std::unique_ptr<media::AudioOutputDelegate> delegate_;
  media::AudioOutputDelegate::EventHandler** delegate_event_handler_location_ =
      nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockContext);
};

class MockClient : public AudioOutputStreamClient {
 public:
  MockClient() {}
  ~MockClient() override {}

  void StreamCreated(mojo::ScopedSharedBufferHandle handle1,
                     mojo::ScopedHandle handle2) {
    was_called_ = true;
  }

  bool was_called() { return was_called_; }

  MOCK_METHOD0(OnError, void());

 private:
  bool was_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockClient);
};

void AuthCallback(media::OutputDeviceStatus* status_out,
                  media::AudioParameters* params_out,
                  std::string* id_out,
                  media::OutputDeviceStatus status,
                  const media::AudioParameters& params,
                  const std::string& id) {
  *status_out = status;
  *params_out = params;
  *id_out = id;
}

}  // namespace

// This test authorizes and creates a stream, and checks that
// 1. the authorization callback is called with appropriate parameters.
// 2. the AudioOutputDelegate is created.
// 3. when the delegate calls OnStreamCreated, this is propagated to the client.
TEST(RenderFrameAudioOutputStreamFactoryTest, CreateStream) {
  content::TestBrowserThreadBundle thread_bundle;
  AudioOutputStreamProviderPtr provider;
  AudioOutputStreamPtr output_stream;
  MockClient client;
  AudioOutputStreamClientPtr client_ptr;
  mojo::Binding<AudioOutputStreamClient> client_binding(
      &client, mojo::MakeRequest(&client_ptr));
  media::AudioOutputDelegate::EventHandler* event_handler = nullptr;
  auto factory_context = base::MakeUnique<MockContext>(true);
  factory_context->PrepareDelegateForCreation(
      base::MakeUnique<MockAudioOutputDelegate>(), &event_handler);
  AudioOutputStreamFactoryPtr factory_ptr = factory_context->CreateFactory();

  media::OutputDeviceStatus status;
  media::AudioParameters params;
  std::string id;
  factory_ptr->RequestDeviceAuthorization(
      mojo::MakeRequest(&provider), kNoSessionId, "default",
      base::BindOnce(&AuthCallback, base::Unretained(&status),
                     base::Unretained(&params), base::Unretained(&id)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, media::OUTPUT_DEVICE_STATUS_OK);
  EXPECT_EQ(params.AsHumanReadableString(),
            GetTestAudioParameters().AsHumanReadableString());
  EXPECT_TRUE(id.empty());

  provider->Acquire(
      mojo::MakeRequest(&output_stream), std::move(client_ptr), params,
      base::BindOnce(&MockClient::StreamCreated, base::Unretained(&client)));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(event_handler, nullptr);

  base::SharedMemory shared_memory;
  ASSERT_TRUE(shared_memory.CreateAndMapAnonymous(100));

  auto local = base::MakeUnique<base::CancelableSyncSocket>();
  auto remote = base::MakeUnique<base::CancelableSyncSocket>();
  ASSERT_TRUE(
      base::CancelableSyncSocket::CreatePair(local.get(), remote.get()));
  event_handler->OnStreamCreated(kStreamId, &shared_memory, std::move(remote));

  base::RunLoop().RunUntilIdle();
  // Make sure we got the callback from creating stream.
  EXPECT_TRUE(client.was_called());
}

TEST(RenderFrameAudioOutputStreamFactoryTest, NotAuthorized_Denied) {
  content::TestBrowserThreadBundle thread_bundle;
  AudioOutputStreamProviderPtr output_provider;
  auto factory_context = base::MakeUnique<MockContext>(false);
  AudioOutputStreamFactoryPtr factory_ptr = factory_context->CreateFactory();

  media::OutputDeviceStatus status;
  media::AudioParameters params;
  std::string id;
  factory_ptr->RequestDeviceAuthorization(
      mojo::MakeRequest(&output_provider), kNoSessionId, "default",
      base::BindOnce(&AuthCallback, base::Unretained(&status),
                     base::Unretained(&params), base::Unretained(&id)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(status, media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED);
  EXPECT_TRUE(id.empty());
}

TEST(RenderFrameAudioOutputStreamFactoryTest, ConnectionError_DeletesStream) {
  content::TestBrowserThreadBundle thread_bundle;
  AudioOutputStreamProviderPtr provider;
  AudioOutputStreamPtr output_stream;
  MockClient client;
  AudioOutputStreamClientPtr client_ptr;
  mojo::Binding<AudioOutputStreamClient> client_binding(
      &client, mojo::MakeRequest(&client_ptr));
  bool delegate_is_destructed = false;
  media::AudioOutputDelegate::EventHandler* event_handler = nullptr;
  auto factory_context = base::MakeUnique<MockContext>(true);
  factory_context->PrepareDelegateForCreation(
      base::MakeUnique<MockAudioOutputDelegate>(
          base::BindOnce([](bool* destructed) { *destructed = true; },
                         &delegate_is_destructed)),
      &event_handler);
  AudioOutputStreamFactoryPtr factory_ptr = factory_context->CreateFactory();

  factory_ptr->RequestDeviceAuthorization(
      mojo::MakeRequest(&provider), kNoSessionId, "default",
      base::BindOnce([](media::OutputDeviceStatus status,
                        const media::AudioParameters& params,
                        const std::string& id) {}));
  base::RunLoop().RunUntilIdle();

  provider->Acquire(
      mojo::MakeRequest(&output_stream), std::move(client_ptr),
      GetTestAudioParameters(),
      base::BindOnce(&MockClient::StreamCreated, base::Unretained(&client)));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(event_handler, nullptr);
  EXPECT_FALSE(delegate_is_destructed);
  output_stream.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(delegate_is_destructed);
}

TEST(RenderFrameAudioOutputStreamFactoryTest, DelegateError_DeletesStream) {
  content::TestBrowserThreadBundle thread_bundle;
  AudioOutputStreamProviderPtr provider;
  AudioOutputStreamPtr output_stream;
  MockClient client;
  AudioOutputStreamClientPtr client_ptr;
  mojo::Binding<AudioOutputStreamClient> client_binding(
      &client, mojo::MakeRequest(&client_ptr));
  bool delegate_is_destructed = false;
  media::AudioOutputDelegate::EventHandler* event_handler = nullptr;
  auto factory_context = base::MakeUnique<MockContext>(true);
  factory_context->PrepareDelegateForCreation(
      base::MakeUnique<MockAudioOutputDelegate>(
          base::BindOnce([](bool* destructed) { *destructed = true; },
                         &delegate_is_destructed)),
      &event_handler);
  AudioOutputStreamFactoryPtr factory_ptr = factory_context->CreateFactory();

  factory_ptr->RequestDeviceAuthorization(
      mojo::MakeRequest(&provider), kNoSessionId, "default",
      base::BindOnce([](media::OutputDeviceStatus status,
                        const media::AudioParameters& params,
                        const std::string& id) {}));
  base::RunLoop().RunUntilIdle();

  provider->Acquire(
      mojo::MakeRequest(&output_stream), std::move(client_ptr),
      GetTestAudioParameters(),
      base::BindOnce(&MockClient::StreamCreated, base::Unretained(&client)));
  base::RunLoop().RunUntilIdle();
  ASSERT_NE(event_handler, nullptr);
  EXPECT_FALSE(delegate_is_destructed);
  event_handler->OnStreamError(kStreamId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(delegate_is_destructed);
}

TEST(RenderFrameAudioOutputStreamFactoryTest, OutOfRangeSessionId_BadMessage) {
  // This test checks that we get a bad message if session_id is too large
  // to fit in an integer. This ensures that we don't overflow when casting the
  // int64_t to an int
  if (sizeof(int) >= sizeof(int64_t)) {
    // In this case, any int64_t would fit in an int, and the case we are
    // checking for is impossible.
    return;
  }

  bool got_bad_message = false;
  mojo::edk::SetDefaultProcessErrorCallback(
      base::Bind([](bool* got_bad_message,
                    const std::string& s) { *got_bad_message = true; },
                 &got_bad_message));

  TestBrowserThreadBundle thread_bundle;

  AudioOutputStreamProviderPtr output_provider;
  auto factory_context = base::MakeUnique<MockContext>(true);
  auto factory_ptr = factory_context->CreateFactory();

  int64_t session_id = std::numeric_limits<int>::max();
  ++session_id;

  EXPECT_FALSE(got_bad_message);
  factory_ptr->RequestDeviceAuthorization(
      mojo::MakeRequest(&output_provider), session_id, "default",
      base::BindOnce([](media::OutputDeviceStatus,
                        const media::AudioParameters&, const std::string&) {}));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(got_bad_message);
}

}  // namespace content
