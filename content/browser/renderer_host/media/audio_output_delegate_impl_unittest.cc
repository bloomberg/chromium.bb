// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_output_delegate_impl.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/audio_manager_thread.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_observer.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_output_controller.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::NotNull;
using ::testing::StrictMock;

// TODO(maxmorin): not yet tested:
// - Interactions with AudioStreamMonitor (goes through WebContentsImpl,
//   so it's a bit tricky).
// - Logging (small risk of bugs, not worth the effort).
// - That the returned socket/memory is correctly set up.

namespace content {

namespace {

const int kRenderProcessId = 1;
const int kRenderFrameId = 5;
const int kStreamId = 50;
const char kDefaultDeviceId[] = "";

class MockAudioMirroringManager : public AudioMirroringManager {
 public:
  MOCK_METHOD3(AddDiverter,
               void(int render_process_id,
                    int render_frame_id,
                    Diverter* diverter));
  MOCK_METHOD1(RemoveDiverter, void(Diverter* diverter));
};

class MockObserver : public content::MediaObserver {
 public:
  void OnAudioCaptureDevicesChanged() override {}
  void OnVideoCaptureDevicesChanged() override {}
  void OnMediaRequestStateChanged(int render_process_id,
                                  int render_frame_id,
                                  int page_request_id,
                                  const GURL& security_origin,
                                  MediaStreamType stream_type,
                                  MediaRequestState state) override {}
  void OnSetCapturingLinkSecured(int render_process_id,
                                 int render_frame_id,
                                 int page_request_id,
                                 MediaStreamType stream_type,
                                 bool is_secure) override {}

  MOCK_METHOD2(OnCreatingAudioStream,
               void(int render_process_id, int render_frame_id));
};

class MockEventHandler : public AudioOutputDelegate::EventHandler {
 public:
  MOCK_METHOD3(OnStreamCreated,
               void(int stream_id,
                    base::SharedMemory* shared_memory,
                    base::CancelableSyncSocket* socket));
  MOCK_METHOD1(OnStreamError, void(int stream_id));
};

class DummyAudioOutputStream : public media::AudioOutputStream {
 public:
  // AudioOutputSteam implementation:
  bool Open() override { return true; }
  void Start(AudioSourceCallback* cb) override {}
  void Stop() override {}
  void SetVolume(double volume) override {}
  void GetVolume(double* volume) override { *volume = 1; }
  void Close() override {}
};

}  // namespace

class AudioOutputDelegateTest : public testing::Test {
 public:
  AudioOutputDelegateTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);

    // This test uses real UI, IO and audio threads.
    // AudioOutputDelegate mainly interacts with the IO and audio threads,
    // but interacts with UI for bad messages, so using these threads should
    // approximate the real conditions of AudioOutputDelegate well.
    thread_bundle_ = base::MakeUnique<TestBrowserThreadBundle>(
        TestBrowserThreadBundle::Options::REAL_IO_THREAD);
    audio_thread_ = base::MakeUnique<AudioManagerThread>();

    audio_manager_.reset(new media::FakeAudioManager(
        audio_thread_->task_runner(), audio_thread_->worker_task_runner(),
        &log_factory_));
    media_stream_manager_ =
        base::MakeUnique<MediaStreamManager>(audio_manager_.get());
  }

  // Test bodies are here, so that we can run them on the IO thread.
  void CreateTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_,
                OnStreamCreated(kStreamId, NotNull(), NotNull()));
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));

    {
      AudioOutputDelegateImpl delegate(
          &event_handler_, audio_manager_.get(),
          log_factory_.CreateAudioLog(
              media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER),
          &mirroring_manager_, &media_observer_, kStreamId, kRenderFrameId,
          kRenderProcessId, audio_manager_->GetDefaultOutputStreamParameters(),
          kDefaultDeviceId);

      SyncWithAllThreads();

      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    }
    SyncWithAllThreads();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done);
  }

  void PlayTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_,
                OnStreamCreated(kStreamId, NotNull(), NotNull()));
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));

    {
      AudioOutputDelegateImpl delegate(
          &event_handler_, audio_manager_.get(),
          log_factory_.CreateAudioLog(
              media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER),
          &mirroring_manager_, &media_observer_, kStreamId, kRenderFrameId,
          kRenderProcessId, audio_manager_->GetDefaultOutputStreamParameters(),
          kDefaultDeviceId);

      delegate.OnPlayStream();

      SyncWithAllThreads();

      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    }
    SyncWithAllThreads();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done);
  }

  void PauseTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_,
                OnStreamCreated(kStreamId, NotNull(), NotNull()));
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));

    {
      AudioOutputDelegateImpl delegate(
          &event_handler_, audio_manager_.get(),
          log_factory_.CreateAudioLog(
              media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER),
          &mirroring_manager_, &media_observer_, kStreamId, kRenderFrameId,
          kRenderProcessId, audio_manager_->GetDefaultOutputStreamParameters(),
          kDefaultDeviceId);

      delegate.OnPauseStream();

      SyncWithAllThreads();

      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    }
    SyncWithAllThreads();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done);
  }

  void PlayPausePlayTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_,
                OnStreamCreated(kStreamId, NotNull(), NotNull()));
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));

    {
      AudioOutputDelegateImpl delegate(
          &event_handler_, audio_manager_.get(),
          log_factory_.CreateAudioLog(
              media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER),
          &mirroring_manager_, &media_observer_, kStreamId, kRenderFrameId,
          kRenderProcessId, audio_manager_->GetDefaultOutputStreamParameters(),
          kDefaultDeviceId);

      delegate.OnPlayStream();
      delegate.OnPauseStream();
      delegate.OnPlayStream();

      SyncWithAllThreads();

      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    }
    SyncWithAllThreads();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done);
  }

  void PlayPlayTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_,
                OnStreamCreated(kStreamId, NotNull(), NotNull()));
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));

    {
      AudioOutputDelegateImpl delegate(
          &event_handler_, audio_manager_.get(),
          log_factory_.CreateAudioLog(
              media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER),
          &mirroring_manager_, &media_observer_, kStreamId, kRenderFrameId,
          kRenderProcessId, audio_manager_->GetDefaultOutputStreamParameters(),
          kDefaultDeviceId);

      delegate.OnPlayStream();
      delegate.OnPlayStream();

      SyncWithAllThreads();

      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    }
    SyncWithAllThreads();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done);
  }

  void CreateDivertTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_,
                OnStreamCreated(kStreamId, NotNull(), NotNull()));
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));

    DummyAudioOutputStream stream;
    {
      AudioOutputDelegateImpl delegate(
          &event_handler_, audio_manager_.get(),
          log_factory_.CreateAudioLog(
              media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER),
          &mirroring_manager_, &media_observer_, kStreamId, kRenderFrameId,
          kRenderProcessId, audio_manager_->GetDefaultOutputStreamParameters(),
          kDefaultDeviceId);

      delegate.GetController()->StartDiverting(&stream);

      SyncWithAllThreads();

      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    }
    SyncWithAllThreads();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done);
  }

  void CreateDivertPauseTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_,
                OnStreamCreated(kStreamId, NotNull(), NotNull()));
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));

    DummyAudioOutputStream stream;
    {
      AudioOutputDelegateImpl delegate(
          &event_handler_, audio_manager_.get(),
          log_factory_.CreateAudioLog(
              media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER),
          &mirroring_manager_, &media_observer_, kStreamId, kRenderFrameId,
          kRenderProcessId, audio_manager_->GetDefaultOutputStreamParameters(),
          kDefaultDeviceId);

      delegate.GetController()->StartDiverting(&stream);

      SyncWithAllThreads();
      delegate.OnPauseStream();

      SyncWithAllThreads();

      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    }
    SyncWithAllThreads();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done);
  }

  void PlayDivertTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_,
                OnStreamCreated(kStreamId, NotNull(), NotNull()));
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));

    DummyAudioOutputStream stream;
    {
      AudioOutputDelegateImpl delegate(
          &event_handler_, audio_manager_.get(),
          log_factory_.CreateAudioLog(
              media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER),
          &mirroring_manager_, &media_observer_, kStreamId, kRenderFrameId,
          kRenderProcessId, audio_manager_->GetDefaultOutputStreamParameters(),
          kDefaultDeviceId);

      delegate.OnPlayStream();
      delegate.GetController()->StartDiverting(&stream);

      SyncWithAllThreads();

      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    }
    SyncWithAllThreads();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done);
  }

  void ErrorTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_,
                OnStreamCreated(kStreamId, NotNull(), NotNull()));
    EXPECT_CALL(event_handler_, OnStreamError(kStreamId));
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));

    {
      AudioOutputDelegateImpl delegate(
          &event_handler_, audio_manager_.get(),
          log_factory_.CreateAudioLog(
              media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER),
          &mirroring_manager_, &media_observer_, kStreamId, kRenderFrameId,
          kRenderProcessId, audio_manager_->GetDefaultOutputStreamParameters(),
          kDefaultDeviceId);

      delegate.GetController()->OnError(nullptr);

      SyncWithAllThreads();

      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    }
    SyncWithAllThreads();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done);
  }

  void CreateAndDestroyTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));
    EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));

    {
      AudioOutputDelegateImpl delegate(
          &event_handler_, audio_manager_.get(),
          log_factory_.CreateAudioLog(
              media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER),
          &mirroring_manager_, &media_observer_, kStreamId, kRenderFrameId,
          kRenderProcessId, audio_manager_->GetDefaultOutputStreamParameters(),
          kDefaultDeviceId);
    }
    SyncWithAllThreads();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done);
  }

  void PlayAndDestroyTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_,
                OnStreamCreated(kStreamId, NotNull(), NotNull()));
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));
    EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));

    {
      AudioOutputDelegateImpl delegate(
          &event_handler_, audio_manager_.get(),
          log_factory_.CreateAudioLog(
              media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER),
          &mirroring_manager_, &media_observer_, kStreamId, kRenderFrameId,
          kRenderProcessId, audio_manager_->GetDefaultOutputStreamParameters(),
          kDefaultDeviceId);

      SyncWithAllThreads();

      delegate.OnPlayStream();
    }
    SyncWithAllThreads();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done);
  }

  void ErrorAndDestroyTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_,
                OnStreamCreated(kStreamId, NotNull(), NotNull()));
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));
    EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));

    {
      AudioOutputDelegateImpl delegate(
          &event_handler_, audio_manager_.get(),
          log_factory_.CreateAudioLog(
              media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER),
          &mirroring_manager_, &media_observer_, kStreamId, kRenderFrameId,
          kRenderProcessId, audio_manager_->GetDefaultOutputStreamParameters(),
          kDefaultDeviceId);
      SyncWithAllThreads();

      delegate.GetController()->OnError(nullptr);
    }
    SyncWithAllThreads();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done);
  }

 protected:
  // MediaStreamManager uses a DestructionObserver, so it must outlive the
  // TestBrowserThreadBundle.
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  std::unique_ptr<TestBrowserThreadBundle> thread_bundle_;
  std::unique_ptr<AudioManagerThread> audio_thread_;
  media::ScopedAudioManagerPtr audio_manager_;
  StrictMock<MockAudioMirroringManager> mirroring_manager_;
  StrictMock<MockEventHandler> event_handler_;
  StrictMock<MockObserver> media_observer_;
  media::FakeAudioLogFactory log_factory_;

 private:
  void SyncWithAllThreads() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    // New tasks might be posted while we are syncing, but in every iteration at
    // least one task will be run. 20 iterations should be enough for our code.
    for (int i = 0; i < 20; ++i) {
      {
        base::MessageLoop::ScopedNestableTaskAllower allower(
            base::MessageLoop::current());
        base::RunLoop().RunUntilIdle();
      }
      SyncWith(BrowserThread::GetTaskRunnerForThread(BrowserThread::UI));
      SyncWith(audio_manager_->GetWorkerTaskRunner());
    }
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

  DISALLOW_COPY_AND_ASSIGN(AudioOutputDelegateTest);
};

TEST_F(AudioOutputDelegateTest, Create) {
  base::RunLoop l;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&AudioOutputDelegateTest::CreateTest,
                                     base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, Play) {
  base::RunLoop l;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&AudioOutputDelegateTest::PlayTest,
                                     base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, Pause) {
  base::RunLoop l;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&AudioOutputDelegateTest::PauseTest,
                                     base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, PlayPausePlay) {
  base::RunLoop l;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AudioOutputDelegateTest::PlayPausePlayTest,
                 base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, PlayPlay) {
  base::RunLoop l;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&AudioOutputDelegateTest::PlayPlayTest,
                                     base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, PlayDivert) {
  base::RunLoop l;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&AudioOutputDelegateTest::PlayDivertTest,
                                     base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, CreateDivert) {
  base::RunLoop l;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&AudioOutputDelegateTest::CreateDivertTest,
                                     base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, Error) {
  base::RunLoop l;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&AudioOutputDelegateTest::ErrorTest,
                                     base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, CreateAndDestroy) {
  base::RunLoop l;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AudioOutputDelegateTest::CreateAndDestroyTest,
                 base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, PlayAndDestroy) {
  base::RunLoop l;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AudioOutputDelegateTest::PlayAndDestroyTest,
                 base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, ErrorAndDestroy) {
  base::RunLoop l;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AudioOutputDelegateTest::PlayAndDestroyTest,
                 base::Unretained(this), l.QuitClosure()));
  l.Run();
}

}  // namespace content
