// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "content/common/child_process.h"
#include "content/common/child_thread.h"
#include "content/renderer/media/audio_renderer_impl.h"
#include "content/renderer/mock_content_renderer_client.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
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

// This callback can be posted on the IO thread and will signal an event when
// done. The caller can then wait for this signal to ensure that no
// additional tasks remain in the task queue.
void WaitCallback(base::WaitableEvent* event) {
  event->Signal();
}

// Class we would be testing.
class TestAudioRendererImpl : public AudioRendererImpl {
 public:
  explicit TestAudioRendererImpl()
      : AudioRendererImpl() {
  }
};

class AudioRendererImplTest
    : public ::testing::Test,
      public IPC::Channel::Listener {
 public:
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
    render_thread_ = new RenderThreadImpl(kThreadName);
    mock_process_->set_main_thread(render_thread_);

    // Setup expectations for initialization.
    decoder_ = new media::MockAudioDecoder();

    EXPECT_CALL(*decoder_, bits_per_channel())
        .WillRepeatedly(Return(16));
    EXPECT_CALL(*decoder_, channel_layout())
        .WillRepeatedly(Return(CHANNEL_LAYOUT_MONO));
    EXPECT_CALL(*decoder_, samples_per_second())
        .WillRepeatedly(Return(44100));

    // Create and initialize the audio renderer.
    renderer_ = new TestAudioRendererImpl();
    renderer_->Initialize(decoder_, media::NewExpectedClosure(),
                          NewUnderflowClosure());

    // We need an event to verify that all tasks are done before leaving
    // our tests.
    event_.reset(new base::WaitableEvent(false, false));
  }

  virtual void TearDown() {
    mock_process_.reset();
  }

  MOCK_METHOD0(OnUnderflow, void());

  base::Closure NewUnderflowClosure() {
    return base::Bind(&AudioRendererImplTest::OnUnderflow,
                      base::Unretained(this));
  }

 protected:
  // Posts a final task to the IO message loop and waits for completion.
  void WaitForIOThreadCompletion() {
    ChildProcess::current()->io_message_loop()->PostTask(
        FROM_HERE, base::Bind(&WaitCallback, base::Unretained(event_.get())));
    EXPECT_TRUE(event_->TimedWait(
        base::TimeDelta::FromMilliseconds(TestTimeouts::action_timeout_ms())));
  }

  MessageLoopForIO message_loop_;
  content::MockContentRendererClient mock_content_renderer_client_;
  scoped_ptr<IPC::Channel> channel_;
  RenderThreadImpl* render_thread_;  // owned by mock_process_
  scoped_ptr<MockRenderProcess> mock_process_;
  scoped_refptr<media::MockAudioDecoder> decoder_;
  scoped_refptr<AudioRendererImpl> renderer_;
  scoped_ptr<base::WaitableEvent> event_;

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

  renderer_->Stop(media::NewExpectedClosure());
  WaitForIOThreadCompletion();
}

TEST_F(AudioRendererImplTest, SetVolume) {
  // Execute SetVolume() codepath.
  // This method will be called on the pipeline thread IRL.
  // Tasks will be posted internally on the IO thread.
  renderer_->SetVolume(0.5f);

  renderer_->Stop(media::NewExpectedClosure());
  WaitForIOThreadCompletion();
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
  renderer_->Stop(media::NewExpectedClosure());
  WaitForIOThreadCompletion();
}
