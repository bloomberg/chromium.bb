// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for AudioOutputAuthorizationHandler.

#include "content/browser/renderer_host/media/audio_output_authorization_handler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "content/browser/browser_thread_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/audio_thread_impl.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::_;

namespace content {

namespace {
const int kRenderProcessId = 42;
const int kRenderFrameId = 31415;
const int kSessionId = 1618;
const char kSecurityOriginString[] = "http://localhost";
const char kBadSecurityOriginString[] = "about:about";
const char kDefaultDeviceId[] = "default";
const char kEmptyDeviceId[] = "";
const char kInvalidDeviceId[] = "invalid-device-id";
const char kSalt[] = "salt";

using MockAuthorizationCallback = base::MockCallback<
    AudioOutputAuthorizationHandler::AuthorizationCompletedCallback>;

url::Origin SecurityOrigin() {
  return url::Origin(GURL(kSecurityOriginString));
}

url::Origin BadSecurityOrigin() {
  return url::Origin(GURL(kBadSecurityOriginString));
}
}  //  namespace

class AudioOutputAuthorizationHandlerTest : public testing::Test {
 public:
  AudioOutputAuthorizationHandlerTest() {
    // Not threadsafe, thus set before threads are started:
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);

    thread_bundle_ = base::MakeUnique<TestBrowserThreadBundle>(
        TestBrowserThreadBundle::Options::REAL_IO_THREAD);
    audio_manager_.reset(new media::FakeAudioManager(
        base::MakeUnique<media::AudioThreadImpl>(), &log_factory_));
    audio_system_ = media::AudioSystemImpl::Create(audio_manager_.get());
    media_stream_manager_ =
        base::MakeUnique<MediaStreamManager>(audio_system_.get());
    // Make sure everything is done initializing:
    SyncWithAllThreads();
  }

  ~AudioOutputAuthorizationHandlerTest() override {
    SyncWithAllThreads();
    audio_manager_->Shutdown();
  }

 protected:
  MediaStreamManager* GetMediaStreamManager() {
    return media_stream_manager_.get();
  }

  media::AudioSystem* GetAudioSystem() { return audio_system_.get(); }

  void SyncWithAllThreads() {
    // New tasks might be posted while we are syncing, but in
    // every iteration at least one task will be run. 20 iterations should be
    // enough for our code.
    for (int i = 0; i < 20; ++i) {
      base::RunLoop().RunUntilIdle();
      SyncWith(BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
      SyncWith(audio_manager_->GetWorkerTaskRunner());
    }
  }

  std::string GetRawNondefaultId() {
    std::string id;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &AudioOutputAuthorizationHandlerTest::GetRawNondefaultIdOnIOThread,
            base::Unretained(this), base::Unretained(&id)));
    SyncWithAllThreads();
    return id;
  }

 private:
  void SyncWith(scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    CHECK(task_runner);
    CHECK(!task_runner->BelongsToCurrentThread());
    base::WaitableEvent e = {base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED};
    task_runner->PostTask(FROM_HERE, base::Bind(&base::WaitableEvent::Signal,
                                                base::Unretained(&e)));
    e.Wait();
  }

  void GetRawNondefaultIdOnIOThread(std::string* out) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
    devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = true;

    media_stream_manager_->media_devices_manager()->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(
            [](std::string* out, const MediaDeviceEnumeration& result) {
              // Index 0 is default, so use 1.
              CHECK(result[MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT]
                        .size() > 1)
                  << "Expected to have a nondefault device.";
              *out = result[MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT][1]
                         .device_id;
            },
            base::Unretained(out)));
  }

  // media_stream_manager must die after threads since it's a
  // DestructionObserver.
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  std::unique_ptr<TestBrowserThreadBundle> thread_bundle_;
  media::FakeAudioLogFactory log_factory_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputAuthorizationHandlerTest);
};

TEST_F(AudioOutputAuthorizationHandlerTest, AuthorizeDefaultDevice_Ok) {
  MockAuthorizationCallback listener;
  EXPECT_CALL(listener,
              Run(media::OUTPUT_DEVICE_STATUS_OK, false, _, kDefaultDeviceId))
      .Times(1);
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      base::MakeUnique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), kRenderProcessId, kSalt);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      (base::Bind(&AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
                  base::Unretained(handler.get()), kRenderFrameId, 0,
                  kDefaultDeviceId, SecurityOrigin(), listener.Get())));

  SyncWithAllThreads();
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, handler.release());
  SyncWithAllThreads();
}

TEST_F(AudioOutputAuthorizationHandlerTest,
       AuthorizeDefaultDeviceByEmptyId_Ok) {
  MockAuthorizationCallback listener;
  EXPECT_CALL(listener,
              Run(media::OUTPUT_DEVICE_STATUS_OK, false, _, kDefaultDeviceId))
      .Times(1);
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      base::MakeUnique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), kRenderProcessId, kSalt);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      (base::Bind(&AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
                  base::Unretained(handler.get()), kRenderFrameId, 0,
                  kEmptyDeviceId, SecurityOrigin(), listener.Get())));

  SyncWithAllThreads();
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, handler.release());
  SyncWithAllThreads();
}

TEST_F(AudioOutputAuthorizationHandlerTest,
       AuthorizeNondefaultDeviceIdWithoutPermission_NotAuthorized) {
  std::string raw_nondefault_id = GetRawNondefaultId();
  std::string hashed_id = MediaStreamManager::GetHMACForMediaDeviceID(
      kSalt, SecurityOrigin(), raw_nondefault_id);

  MockAuthorizationCallback listener;
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      base::MakeUnique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), kRenderProcessId, kSalt);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &AudioOutputAuthorizationHandler::OverridePermissionsForTesting,
          base::Unretained(handler.get()), false));
  SyncWithAllThreads();

  EXPECT_CALL(listener, Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED,
                            false, _, std::string()))
      .Times(1);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      (base::Bind(&AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
                  base::Unretained(handler.get()), kRenderFrameId, 0, hashed_id,
                  SecurityOrigin(), listener.Get())));

  SyncWithAllThreads();
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, handler.release());
  SyncWithAllThreads();
}

TEST_F(AudioOutputAuthorizationHandlerTest,
       AuthorizeNondefaultDeviceIdWithPermission_Ok) {
  std::string raw_nondefault_id = GetRawNondefaultId();
  std::string hashed_id = MediaStreamManager::GetHMACForMediaDeviceID(
      kSalt, SecurityOrigin(), raw_nondefault_id);
  MockAuthorizationCallback listener;
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      base::MakeUnique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), kRenderProcessId, kSalt);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &AudioOutputAuthorizationHandler::OverridePermissionsForTesting,
          base::Unretained(handler.get()), true));

  EXPECT_CALL(listener,
              Run(media::OUTPUT_DEVICE_STATUS_OK, false, _, raw_nondefault_id))
      .Times(1);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      (base::Bind(&AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
                  base::Unretained(handler.get()), kRenderFrameId, 0, hashed_id,
                  SecurityOrigin(), listener.Get())));

  SyncWithAllThreads();
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, handler.release());
  SyncWithAllThreads();
}

TEST_F(AudioOutputAuthorizationHandlerTest, AuthorizeInvalidDeviceId_NotFound) {
  std::unique_ptr<TestBrowserContext> context =
      base::MakeUnique<TestBrowserContext>();
  std::unique_ptr<MockRenderProcessHost> RPH =
      base::MakeUnique<MockRenderProcessHost>(context.get());
  MockAuthorizationCallback listener;
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      base::MakeUnique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), RPH->GetID(), kSalt);
  EXPECT_EQ(RPH->bad_msg_count(), 0);

  EXPECT_CALL(listener,
              Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND, _, _, _))
      .Times(1);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      (base::Bind(&AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
                  base::Unretained(handler.get()), kRenderFrameId, 0,
                  kInvalidDeviceId, SecurityOrigin(), listener.Get())));

  SyncWithAllThreads();
  // It is possible to request an invalid device id from JS APIs,
  // so we don't want to crash the renderer for this.
  EXPECT_EQ(RPH->bad_msg_count(), 0);
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, handler.release());
  SyncWithAllThreads();
  RPH.reset();
  context.reset();
}

TEST_F(AudioOutputAuthorizationHandlerTest,
       AuthorizeNondefaultDeviceIdWithBadOrigin_BadMessage) {
  std::string raw_nondefault_id = GetRawNondefaultId();
  std::string hashed_id = MediaStreamManager::GetHMACForMediaDeviceID(
      kSalt, SecurityOrigin(), raw_nondefault_id);
  std::unique_ptr<TestBrowserContext> context =
      base::MakeUnique<TestBrowserContext>();
  std::unique_ptr<MockRenderProcessHost> RPH =
      base::MakeUnique<MockRenderProcessHost>(context.get());
  MockAuthorizationCallback listener;
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      base::MakeUnique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), RPH->GetID(), kSalt);

  EXPECT_EQ(RPH->bad_msg_count(), 0);
  // We must still get a callback by the contract of RequestDeviceAuthorization.
  EXPECT_CALL(listener, Run(_, _, _, _)).Times(1);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      (base::Bind(&AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
                  base::Unretained(handler.get()), kRenderFrameId, 0, hashed_id,
                  BadSecurityOrigin(), listener.Get())));

  SyncWithAllThreads();
  EXPECT_EQ(RPH->bad_msg_count(), 1);
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, handler.release());
  SyncWithAllThreads();
  RPH.reset();
  context.reset();
}

TEST_F(AudioOutputAuthorizationHandlerTest,
       AuthorizeWithSessionIdWithoutDevice_GivesDefault) {
  MockAuthorizationCallback listener;
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      base::MakeUnique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), kRenderProcessId, kSalt);

  EXPECT_CALL(listener,
              Run(media::OUTPUT_DEVICE_STATUS_OK, false, _, kDefaultDeviceId))
      .Times(1);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      (base::Bind(&AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
                  base::Unretained(handler.get()), kRenderFrameId, kSessionId,
                  std::string(), BadSecurityOrigin(), listener.Get())));

  SyncWithAllThreads();
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, handler.release());
  SyncWithAllThreads();
}

}  // namespace content
