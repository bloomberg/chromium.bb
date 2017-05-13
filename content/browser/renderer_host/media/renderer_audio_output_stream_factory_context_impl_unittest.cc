// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/renderer_audio_output_stream_factory_context_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/shared_memory.h"
#include "base/memory/shared_memory_handle.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sync_socket.h"
#include "cc/base/math_util.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/common/media/renderer_audio_output_stream_factory.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_output_controller.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/audio_thread_impl.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using testing::_;
using testing::StrictMock;
using testing::Return;
using testing::Test;
using AudioOutputStreamFactory = mojom::RendererAudioOutputStreamFactory;
using AudioOutputStreamFactoryPtr =
    mojo::InterfacePtr<AudioOutputStreamFactory>;
using AudioOutputStreamFactoryRequest =
    mojo::InterfaceRequest<AudioOutputStreamFactory>;
using AudioOutputStream = media::mojom::AudioOutputStream;
using AudioOutputStreamPtr = mojo::InterfacePtr<AudioOutputStream>;
using AudioOutputStreamRequest = mojo::InterfaceRequest<AudioOutputStream>;
using AudioOutputStreamProvider = media::mojom::AudioOutputStreamProvider;
using AudioOutputStreamProviderPtr =
    mojo::InterfacePtr<AudioOutputStreamProvider>;
using AudioOutputStreamProviderRequest =
    mojo::InterfaceRequest<AudioOutputStreamProvider>;

const int kRenderProcessId = 42;
const int kRenderFrameId = 24;
const int kNoSessionId = 0;
const float kWaveFrequency = 440.f;
const int kChannels = 1;
const int kBuffers = 1000;
const int kSampleFrequency = 44100;
const int kBitsPerSample = 16;
const int kSamplesPerBuffer = kSampleFrequency / 100;
const char kSalt[] = "salt";

std::unique_ptr<media::AudioOutputStream::AudioSourceCallback>
GetTestAudioSource() {
  return base::MakeUnique<media::SineWaveAudioSource>(kChannels, kWaveFrequency,
                                                      kSampleFrequency);
}

media::AudioParameters GetTestAudioParameters() {
  return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_MONO, kSampleFrequency,
                                kBitsPerSample, kSamplesPerBuffer);
}

void SyncWith(scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  CHECK(task_runner);
  CHECK(!task_runner->BelongsToCurrentThread());
  base::WaitableEvent e = {base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED};
  task_runner->PostTask(FROM_HERE, base::Bind(&base::WaitableEvent::Signal,
                                              base::Unretained(&e)));
  e.Wait();
}

void SyncWithAllThreads() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // New tasks might be posted while we are syncing, but in every iteration at
  // least one task will be run. 20 iterations should be enough for our code.
  for (int i = 0; i < 20; ++i) {
    {
      base::MessageLoop::ScopedNestableTaskAllower allower(
          base::MessageLoop::current());
      base::RunLoop().RunUntilIdle();
    }
    SyncWith(BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
    SyncWith(media::AudioManager::Get()->GetWorkerTaskRunner());
  }
}

class MockAudioManager : public media::AudioManagerBase {
 public:
  MockAudioManager(std::unique_ptr<media::AudioThread> audio_thread,
                   media::AudioLogFactory* audio_log_factory)
      : media::AudioManagerBase(std::move(audio_thread), audio_log_factory) {
    ON_CALL(*this, HasAudioOutputDevices()).WillByDefault(Return(true));
  }

  ~MockAudioManager() override = default;

  MOCK_METHOD2(MakeLinearOutputStream,
               media::AudioOutputStream*(const media::AudioParameters& params,
                                         const LogCallback& log_callback));
  MOCK_METHOD3(MakeLowLatencyOutputStream,
               media::AudioOutputStream*(const media::AudioParameters& params,
                                         const std::string& device_id,
                                         const LogCallback& log_callback));
  MOCK_METHOD3(MakeLinearInputStream,
               media::AudioInputStream*(const media::AudioParameters& params,
                                        const std::string& device_id,
                                        const LogCallback& log_callback));
  MOCK_METHOD3(MakeLowLatencyInputStream,
               media::AudioInputStream*(const media::AudioParameters& params,
                                        const std::string& device_id,
                                        const LogCallback& log_callback));

 protected:
  MOCK_METHOD0(HasAudioOutputDevices, bool());
  MOCK_METHOD0(HasAudioInputDevices, bool());
  MOCK_METHOD0(GetName, const char*());
  media::AudioParameters GetPreferredOutputStreamParameters(
      const std::string& device_id,
      const media::AudioParameters& params) {
    return GetTestAudioParameters();
  }
};

class MockAudioOutputStream : public media::AudioOutputStream,
                              public base::PlatformThread::Delegate {
 public:
  explicit MockAudioOutputStream(MockAudioManager* audio_manager)
      : done_(base::WaitableEvent::ResetPolicy::MANUAL,
              base::WaitableEvent::InitialState::NOT_SIGNALED),
        audio_manager_(audio_manager) {}

  ~MockAudioOutputStream() override {
    base::PlatformThread::Join(thread_handle_);
  }

  void Start(AudioSourceCallback* callback) override {
    callback_ = callback;
    EXPECT_TRUE(base::PlatformThread::CreateWithPriority(
        0, this, &thread_handle_, base::ThreadPriority::REALTIME_AUDIO));
  }

  void Stop() override {
    done_.Wait();
    callback_ = nullptr;
  }

  bool Open() override { return true; }
  void SetVolume(double volume) override {}
  void GetVolume(double* volume) override { *volume = 1; }
  void Close() override {
    Stop();
    audio_manager_->ReleaseOutputStream(this);
  }

  void ThreadMain() override {
    std::unique_ptr<media::AudioOutputStream::AudioSourceCallback>
        expected_audio = GetTestAudioSource();
    media::AudioParameters params = GetTestAudioParameters();
    std::unique_ptr<media::AudioBus> dest = media::AudioBus::Create(params);
    std::unique_ptr<media::AudioBus> expected_buffer =
        media::AudioBus::Create(params);
    for (int i = 0; i < kBuffers; ++i) {
      expected_audio->OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), 0,
                                 expected_buffer.get());
      callback_->OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), 0,
                            dest.get());
      for (int frame = 0; frame < params.frames_per_buffer(); ++frame) {
        // Using EXPECT here causes massive log spam in case of a broken test,
        // and ASSERT causes it to hang, so we use CHECK.
        CHECK(cc::MathUtil::IsFloatNearlyTheSame(
            expected_buffer->channel(0)[frame], dest->channel(0)[frame]))
            << "Got " << dest->channel(0)[frame] << ", expected "
            << expected_buffer->channel(0)[frame];
      }
    }
    done_.Signal();
  }

 private:
  base::OnceClosure sync_closure_;
  base::PlatformThreadHandle thread_handle_;
  base::WaitableEvent done_;
  MockAudioManager* audio_manager_;
  AudioSourceCallback* callback_;
};

void AuthCallback(base::OnceClosure sync_closure,
                  media::OutputDeviceStatus* status_out,
                  media::AudioParameters* params_out,
                  std::string* id_out,
                  media::OutputDeviceStatus status,
                  const media::AudioParameters& params,
                  const std::string& id) {
  *status_out = status;
  *params_out = params;
  *id_out = id;
  std::move(sync_closure).Run();
}

// "Renderer-side" audio client. Provides the signal given by
// GetTestAudioSource() from a dedicated thread when given sync socket and
// shared memory.
// TODO(maxmorin): Replace with an instance of the real client, when it exists.
class TestIPCClient : public base::PlatformThread::Delegate {
 public:
  TestIPCClient() {}

  ~TestIPCClient() override { base::PlatformThread::Join(thread_handle_); }

  // Starts thread, sets up IPC primitives and sends signal on thread.
  void Start(mojo::ScopedSharedBufferHandle shared_buffer,
             mojo::ScopedHandle socket_handle) {
    EXPECT_TRUE(socket_handle.is_valid());
    // Set up socket.
    base::PlatformFile fd;
    mojo::UnwrapPlatformFile(std::move(socket_handle), &fd);
    socket_ = base::MakeUnique<base::CancelableSyncSocket>(fd);
    EXPECT_NE(socket_->handle(), base::CancelableSyncSocket::kInvalidHandle);

    // Set up memory.
    EXPECT_TRUE(shared_buffer.is_valid());
    size_t memory_length;
    base::SharedMemoryHandle shmem_handle;
    bool read_only;
    EXPECT_EQ(
        mojo::UnwrapSharedMemoryHandle(std::move(shared_buffer), &shmem_handle,
                                       &memory_length, &read_only),
        MOJO_RESULT_OK);
    EXPECT_EQ(memory_length, sizeof(media::AudioOutputBufferParameters) +
                                 media::AudioBus::CalculateMemorySize(
                                     GetTestAudioParameters()));
    EXPECT_EQ(read_only, false);
    memory_ = base::MakeUnique<base::SharedMemory>(shmem_handle, read_only);
    EXPECT_TRUE(memory_->Map(memory_length));

    EXPECT_TRUE(base::PlatformThread::CreateWithPriority(
        0, this, &thread_handle_, base::ThreadPriority::REALTIME_AUDIO));
  }

  void ThreadMain() override {
    std::unique_ptr<media::AudioOutputStream::AudioSourceCallback>
        audio_source = GetTestAudioSource();

    media::AudioOutputBuffer* buffer =
        reinterpret_cast<media::AudioOutputBuffer*>(memory_->memory());
    std::unique_ptr<media::AudioBus> output_bus =
        media::AudioBus::WrapMemory(GetTestAudioParameters(), buffer->audio);

    // Send s.
    for (uint32_t i = 0; i < kBuffers;) {
      uint32_t pending_data = 0;
      size_t bytes_read = socket_->Receive(&pending_data, sizeof(pending_data));
      // Use check here, since there's a risk of hangs in case of a bug.
      PCHECK(sizeof(pending_data) == bytes_read)
          << "Tried to read " << sizeof(pending_data) << " bytes but only read "
          << bytes_read << " bytes";
      CHECK_EQ(0u, pending_data);

      ++i;
      audio_source->OnMoreData(base::TimeDelta(), base::TimeTicks(), 0,
                               output_bus.get());

      size_t bytes_written = socket_->Send(&i, sizeof(i));
      PCHECK(sizeof(pending_data) == bytes_written)
          << "Tried to write " << sizeof(pending_data)
          << " bytes but only wrote " << bytes_written << " bytes";
    }
  }

 private:
  base::PlatformThreadHandle thread_handle_;
  std::unique_ptr<base::CancelableSyncSocket> socket_;
  std::unique_ptr<base::SharedMemory> memory_;
};

}  // namespace

// TODO(maxmorin): Add test for play, pause and set volume.
class RendererAudioOutputStreamFactoryIntegrationTest : public Test {
 public:
  RendererAudioOutputStreamFactoryIntegrationTest()
      : media_stream_manager_(),
        thread_bundle_(TestBrowserThreadBundle::Options::REAL_IO_THREAD),
        log_factory_(),
        audio_manager_(
            new MockAudioManager(base::MakeUnique<media::AudioThreadImpl>(),
                                 &log_factory_)),
        audio_system_(media::AudioSystemImpl::Create(audio_manager_.get())) {
    media_stream_manager_ =
        base::MakeUnique<MediaStreamManager>(audio_system_.get());
  }
  ~RendererAudioOutputStreamFactoryIntegrationTest() override {
    audio_manager_->Shutdown();
  }

  void CreateAndBindFactory(AudioOutputStreamFactoryRequest request) {
    factory_context_.reset(new RendererAudioOutputStreamFactoryContextImpl(
        kRenderProcessId, audio_system_.get(), audio_manager_.get(),
        media_stream_manager_.get(), kSalt));
    factory_context_->CreateFactory(kRenderFrameId, std::move(request));
  }

  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  TestBrowserThreadBundle thread_bundle_;
  media::FakeAudioLogFactory log_factory_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  std::unique_ptr<RendererAudioOutputStreamFactoryContextImpl,
                  BrowserThread::DeleteOnIOThread>
      factory_context_;
};

TEST_F(RendererAudioOutputStreamFactoryIntegrationTest, StreamIntegrationTest) {
  // Sets up the factory on the IO thread and runs client code on the UI thread.
  // Send a sine wave from the client and makes sure it's received by the output
  // stream.
  MockAudioOutputStream* stream = new MockAudioOutputStream(
      static_cast<MockAudioManager*>(audio_manager_.get()));

  // Make sure the mock audio manager uses our mock stream.
  EXPECT_CALL(*static_cast<MockAudioManager*>(audio_manager_.get()),
              MakeLowLatencyOutputStream(_, "", _))
      .WillOnce(Return(stream));

  AudioOutputStreamFactoryPtr factory_ptr;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RendererAudioOutputStreamFactoryIntegrationTest::
                     CreateAndBindFactory,
                 base::Unretained(this),
                 base::Passed(mojo::MakeRequest(&factory_ptr))));

  AudioOutputStreamProviderPtr provider_ptr;
  base::RunLoop loop;
  media::OutputDeviceStatus status;
  media::AudioParameters params;
  std::string id;
  factory_ptr->RequestDeviceAuthorization(
      mojo::MakeRequest(&provider_ptr), kNoSessionId, "default",
      base::Bind(&AuthCallback, base::Passed(loop.QuitWhenIdleClosure()),
                 base::Unretained(&status), base::Unretained(&params),
                 base::Unretained(&id)));
  loop.Run();
  ASSERT_EQ(status, media::OUTPUT_DEVICE_STATUS_OK);
  ASSERT_EQ(GetTestAudioParameters().AsHumanReadableString(),
            params.AsHumanReadableString());
  ASSERT_TRUE(id.empty());

  AudioOutputStreamPtr stream_ptr;
  {
    TestIPCClient client;
    provider_ptr->Acquire(
        mojo::MakeRequest(&stream_ptr), params,
        base::Bind(&TestIPCClient::Start, base::Unretained(&client)));
    SyncWithAllThreads();
    stream_ptr->Play();
    SyncWithAllThreads();
  }  // Joining client thread.
  stream_ptr.reset();
  SyncWithAllThreads();
}

}  // namespace content
