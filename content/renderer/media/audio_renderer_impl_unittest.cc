// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "content/common/child_process.h"
#include "content/common/child_thread.h"
#include "content/common/media/audio_messages.h"
#include "content/renderer/media/audio_renderer_impl.h"
#include "content/renderer/mock_content_renderer_client.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread.h"
#include "ipc/ipc_channel.h"
#include "media/base/data_buffer.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace {
// This class is a mock of the child process singleton which is needed
// to be able to create a RenderThread object.
class MockRenderProcess : public RenderProcess {
 public:
  MockRenderProcess() {}
  virtual ~MockRenderProcess() {}

  // RenderProcess implementation.
  virtual skia::PlatformCanvas* GetDrawingCanvas(TransportDIB** memory,
      const gfx::Rect& rect) { return NULL; }
  virtual void ReleaseTransportDIB(TransportDIB* memory) {}
  virtual bool UseInProcessPlugins() const { return false; }
  virtual bool HasInitializedMediaLibrary() const { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRenderProcess);
};
}

// This class defines a set of methods which will be used in combination
// with NewRunnableMethod to form tasks which will be posted on the
// IO thread. All methods emulate AudioMessageFilter::Delegate calls.
class DelegateCaller : public base::RefCountedThreadSafe<DelegateCaller> {
 public:
  explicit DelegateCaller(AudioRendererImpl* renderer)
      : renderer_(renderer) {}

  void OnCreated(base::SharedMemoryHandle handle, uint32 length) {
    if (renderer_->latency_type() == AudioRendererImpl::kHighLatency) {
      renderer_->OnCreated(handle, length);
    } else {
      renderer_->OnLowLatencyCreated(handle, 0, length);
    }
  }
  void OnStateChanged(AudioStreamState state) {
    renderer_->OnStateChanged(state);
  }
  void OnRequestPacket(AudioBuffersState buffers_state) {
    renderer_->OnRequestPacket(buffers_state);
  }
  void OnVolume(double volume) {
    renderer_->OnVolume(volume);
  }
  void DestroyCurrentMessageLoop() {
    renderer_->WillDestroyCurrentMessageLoop();
  }
 private:
  friend class base::RefCountedThreadSafe<DelegateCaller>;
  virtual ~DelegateCaller() {}

  scoped_refptr<AudioRendererImpl> renderer_;
  DISALLOW_COPY_AND_ASSIGN(DelegateCaller);
};

// This task can be posted on the IO thread and will signal an event when
// done. The caller can then wait for this signal to ensure that no
// additional tasks remain in the task queue.
class WaitTask : public Task {
 public:
  explicit WaitTask(base::WaitableEvent* event)
      : event_(event) {}
  virtual ~WaitTask() {}
  virtual void Run() {
    event_->Signal();
  }

 private:
  base::WaitableEvent* event_;
  DISALLOW_COPY_AND_ASSIGN(WaitTask);
};

// Class we would be testing. The only difference between it and "real" one
// is that test class does not open sockets and launch audio thread.
class TestAudioRendererImpl : public AudioRendererImpl {
 public:
  explicit TestAudioRendererImpl()
      : AudioRendererImpl() {
  }
 private:
  virtual void CreateSocket(base::SyncSocket::Handle socket_handle) {}
  virtual void CreateAudioThread() {}
};

class AudioRendererImplTest
    : public ::testing::Test,
      public IPC::Channel::Listener {
 public:
  static void SetUpTestCase() {
    // Set low latency mode, as it soon would be on by default.
    if (AudioRendererImpl::latency_type() ==
        AudioRendererImpl::kUninitializedLatency) {
      AudioRendererImpl::set_latency_type(AudioRendererImpl::kLowLatency);
    }
    DCHECK_EQ(AudioRendererImpl::kLowLatency,
              AudioRendererImpl::latency_type());
  }

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) {
      NOTIMPLEMENTED();
      return true;
  }

  static const int kSize;

  AudioRendererImplTest() {}
  virtual ~AudioRendererImplTest() {}

  virtual void SetUp() {
    // This part sets up a RenderThread environment to ensure that
    // RenderThread::current() (<=> TLS pointer) is valid.
    // Main parts are inspired by the RenderViewFakeResourcesTest.
    // Note that, the IPC part is not utilized in this test.
    content::GetContentClient()->set_renderer(&mock_content_renderer_client_);

    static const char kThreadName[] = "RenderThread";
    channel_.reset(new IPC::Channel(kThreadName,
        IPC::Channel::MODE_SERVER, this));
    ASSERT_TRUE(channel_->Connect());

    mock_process_.reset(new MockRenderProcess);
    render_thread_ = new RenderThread(kThreadName);
    mock_process_->set_main_thread(render_thread_);

    // Create temporary shared memory.
    CHECK(shared_mem_.CreateAnonymous(kSize));

    // Setup expectations for initialization.
    decoder_ = new media::MockAudioDecoder();

    ON_CALL(*decoder_, config())
      .WillByDefault(Return(media::AudioDecoderConfig(16,
                                                      CHANNEL_LAYOUT_MONO,
                                                      44100)));

    // Create and initialize the audio renderer.
    renderer_ = new TestAudioRendererImpl();
    renderer_->set_host(&host_);
    renderer_->Initialize(decoder_, media::NewExpectedCallback());

    // Wraps delegate calls into tasks.
    delegate_caller_ = new DelegateCaller(renderer_);

    // We need an event to verify that all tasks are done before leaving
    // our tests.
    event_.reset(new base::WaitableEvent(false, false));

    // Duplicate the shared memory handle so both the test and the callee can
    // close their copy.
    base::SharedMemoryHandle duplicated_handle;
    EXPECT_TRUE(shared_mem_.ShareToProcess(base::GetCurrentProcessHandle(),
      &duplicated_handle));

    // Set things up and ensure that the call comes from the IO thread
    // as all AudioMessageFilter::Delegate methods.
    ChildProcess::current()->io_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(delegate_caller_.get(),
            &DelegateCaller::OnCreated, duplicated_handle, kSize));
    WaitForIOThreadCompletion();
  }

  virtual void TearDown() {
    mock_process_.reset();
  }

 protected:
  // Posts a final task to the IO message loop and waits for completion.
  void WaitForIOThreadCompletion() {
    ChildProcess::current()->io_message_loop()->PostTask(
        FROM_HERE, new WaitTask(event_.get()));
    EXPECT_TRUE(event_->TimedWait(
        base::TimeDelta::FromMilliseconds(TestTimeouts::action_timeout_ms())));
  }

  MessageLoopForIO message_loop_;
  content::MockContentRendererClient mock_content_renderer_client_;
  scoped_ptr<IPC::Channel> channel_;
  RenderThread* render_thread_;  // owned by mock_process_
  scoped_ptr<MockRenderProcess> mock_process_;
  base::SharedMemory shared_mem_;
  media::MockFilterHost host_;
  scoped_refptr<media::MockAudioDecoder> decoder_;
  scoped_refptr<AudioRendererImpl> renderer_;
  scoped_ptr<base::WaitableEvent> event_;
  scoped_refptr<DelegateCaller> delegate_caller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioRendererImplTest);
};

const int AudioRendererImplTest::kSize = 1024;

TEST_F(AudioRendererImplTest, SetPlaybackRate) {
  // Execute SetPlaybackRate() codepath by toggling play/pause.
  // These methods will be called on the pipeline thread but calling from
  // here is fine for this test. Tasks will be posted internally on
  // the IO thread.
  renderer_->SetPlaybackRate(0.0f);
  renderer_->SetPlaybackRate(1.0f);
  renderer_->SetPlaybackRate(0.0f);

  renderer_->Stop(media::NewExpectedCallback());
  WaitForIOThreadCompletion();
}

TEST_F(AudioRendererImplTest, SetVolume) {
  // Execute SetVolume() codepath.
  // This method will be called on the pipeline thread IRL.
  // Tasks will be posted internally on the IO thread.
  renderer_->SetVolume(0.5f);

  renderer_->Stop(media::NewExpectedCallback());
  WaitForIOThreadCompletion();
}

TEST_F(AudioRendererImplTest, Stop) {
  // Execute Stop() codepath.
  // Tasks will be posted internally on the IO thread.
  renderer_->Stop(media::NewExpectedCallback());

  // Run AudioMessageFilter::Delegate methods, which can be executed after being
  // stopped. AudioRendererImpl shouldn't create any messages in this state.
  // All delegate method calls are posted on the IO thread since it is
  // a requirement.
  if (renderer_->latency_type() == AudioRendererImpl::kHighLatency) {
    ChildProcess::current()->io_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(delegate_caller_.get(),
        &DelegateCaller::OnRequestPacket, AudioBuffersState(kSize, 0)));
  }
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(delegate_caller_.get(),
      &DelegateCaller::OnStateChanged, kAudioStreamError));
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(delegate_caller_.get(),
      &DelegateCaller::OnStateChanged, kAudioStreamPlaying));
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(delegate_caller_.get(),
      &DelegateCaller::OnStateChanged, kAudioStreamPaused));
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(delegate_caller_.get(),
      &DelegateCaller::OnCreated, shared_mem_.handle(), kSize));
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(delegate_caller_.get(),
      &DelegateCaller::OnVolume, 0.5));

  WaitForIOThreadCompletion();

  // It's possible that the upstream decoder replies right after being stopped.
  scoped_refptr<media::Buffer> buffer(new media::DataBuffer(kSize));
  renderer_->ConsumeAudioSamples(buffer);
}

TEST_F(AudioRendererImplTest, DestroyedMessageLoop_SetPlaybackRate) {
  // Emulate "killing the message loop" and verify that SetPlaybackRate()
  // still works.
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(delegate_caller_.get(),
      &DelegateCaller::DestroyCurrentMessageLoop));
  WaitForIOThreadCompletion();

  // No tasks will be posted on the IO thread here since we are in
  // a "stopped" state.
  renderer_->SetPlaybackRate(0.0f);
  renderer_->SetPlaybackRate(1.0f);
  renderer_->SetPlaybackRate(0.0f);
  renderer_->Stop(media::NewExpectedCallback());
}

TEST_F(AudioRendererImplTest, DestroyedMessageLoop_SetVolume) {
  // Emulate "killing the message loop" and verify that SetVolume()
  // still works.
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(delegate_caller_.get(),
      &DelegateCaller::DestroyCurrentMessageLoop));
  WaitForIOThreadCompletion();

  // No tasks will be posted on the IO thread here since we are in
  // a "stopped" state.
  renderer_->SetVolume(0.5f);
  renderer_->Stop(media::NewExpectedCallback());
}

TEST_F(AudioRendererImplTest, DestroyedMessageLoop_ConsumeAudioSamples) {
  // Emulate "killing the message loop" and verify that OnReadComplete()
  // still works.
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(delegate_caller_.get(),
      &DelegateCaller::DestroyCurrentMessageLoop));
  WaitForIOThreadCompletion();

  // No tasks will be posted on the IO thread here since we are in
  // a "stopped" state.
  scoped_refptr<media::Buffer> buffer(new media::DataBuffer(kSize));
  renderer_->ConsumeAudioSamples(buffer);
  renderer_->Stop(media::NewExpectedCallback());
}

TEST_F(AudioRendererImplTest, UpdateEarliestEndTime) {
  renderer_->SetPlaybackRate(1.0f);
  WaitForIOThreadCompletion();
  base::Time time_now = base::Time();  // Null time by default.
  renderer_->set_earliest_end_time(time_now);
  renderer_->UpdateEarliestEndTime(renderer_->bytes_per_second(),
                                   base::TimeDelta::FromMilliseconds(100),
                                   time_now);
  int time_delta = (renderer_->earliest_end_time() - time_now).InMilliseconds();
  EXPECT_EQ(1100, time_delta);
  renderer_->Stop(media::NewExpectedCallback());
  WaitForIOThreadCompletion();
}

