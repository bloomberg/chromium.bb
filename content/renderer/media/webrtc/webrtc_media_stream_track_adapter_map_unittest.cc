// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter_map.h"

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebHeap.h"

namespace content {

class WebRtcMediaStreamTrackAdapterMapTest : public ::testing::Test {
 public:
  void SetUp() override {
    dependency_factory_.reset(new MockPeerConnectionDependencyFactory());
    main_thread_ = base::ThreadTaskRunnerHandle::Get();
    map_ = new WebRtcMediaStreamTrackAdapterMap(dependency_factory_.get());
  }

  void TearDown() override { blink::WebHeap::CollectAllGarbageForTesting(); }

  blink::WebMediaStreamTrack CreateLocalTrack(const std::string& id) {
    blink::WebMediaStreamSource web_source;
    web_source.Initialize(
        blink::WebString::FromUTF8(id), blink::WebMediaStreamSource::kTypeAudio,
        blink::WebString::FromUTF8("local_audio_track"), false);
    MediaStreamAudioSource* audio_source = new MediaStreamAudioSource(true);
    // Takes ownership of |audio_source|.
    web_source.SetExtraData(audio_source);

    blink::WebMediaStreamTrack web_track;
    web_track.Initialize(web_source.Id(), web_source);
    audio_source->ConnectToTrack(web_track);
    return web_track;
  }

  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
  GetOrCreateRemoteTrackAdapter(
      webrtc::MediaStreamTrackInterface* webrtc_track) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> adapter;
    dependency_factory_->GetWebRtcSignalingThread()->PostTask(
        FROM_HERE,
        base::BindOnce(&WebRtcMediaStreamTrackAdapterMapTest::
                           GetOrCreateRemoteTrackAdapterOnSignalingThread,
                       base::Unretained(this), webrtc_track, &adapter));
    RunMessageLoopsUntilIdle();
    return adapter;
  }

  void GetOrCreateRemoteTrackAdapterOnSignalingThread(
      webrtc::MediaStreamTrackInterface* webrtc_track,
      std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>* adapter) {
    DCHECK(dependency_factory_->GetWebRtcSignalingThread()
               ->BelongsToCurrentThread());
    *adapter = map_->GetOrCreateRemoteTrackAdapter(webrtc_track);
  }

  // Runs message loops on the webrtc signaling thread and the main thread until
  // idle.
  void RunMessageLoopsUntilIdle() {
    DCHECK(main_thread_->BelongsToCurrentThread());
    base::WaitableEvent waitable_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    dependency_factory_->GetWebRtcSignalingThread()->PostTask(
        FROM_HERE, base::BindOnce(&WebRtcMediaStreamTrackAdapterMapTest::
                                      RunMessageLoopUntilIdleOnSignalingThread,
                                  base::Unretained(this), &waitable_event));
    waitable_event.Wait();
    base::RunLoop().RunUntilIdle();
  }

  void RunMessageLoopUntilIdleOnSignalingThread(
      base::WaitableEvent* waitable_event) {
    DCHECK(dependency_factory_->GetWebRtcSignalingThread()
               ->BelongsToCurrentThread());
    base::RunLoop().RunUntilIdle();
    waitable_event->Signal();
  }

 protected:
  // Message loop and child processes is needed for task queues and threading to
  // work, as is necessary to create tracks and adapters.
  base::MessageLoop message_loop_;
  ChildProcess child_process_;

  std::unique_ptr<MockPeerConnectionDependencyFactory> dependency_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  scoped_refptr<WebRtcMediaStreamTrackAdapterMap> map_;
};

TEST_F(WebRtcMediaStreamTrackAdapterMapTest, AddAndRemoveLocalTrackAdapter) {
  blink::WebMediaStreamTrack web_track = CreateLocalTrack("local_track");
  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> adapter_ref =
      map_->GetOrCreateLocalTrackAdapter(web_track);
  EXPECT_TRUE(adapter_ref->is_initialized());
  EXPECT_EQ(adapter_ref->GetAdapterForTesting(),
            map_->GetLocalTrackAdapter(web_track.Id().Utf8())
                ->GetAdapterForTesting());
  EXPECT_EQ(1u, map_->GetLocalTrackCount());

  // "GetOrCreate" for already existing track.
  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> adapter_ref2 =
      map_->GetOrCreateLocalTrackAdapter(web_track);
  EXPECT_EQ(adapter_ref->GetAdapterForTesting(),
            adapter_ref2->GetAdapterForTesting());
  EXPECT_EQ(1u, map_->GetLocalTrackCount());

  adapter_ref2.reset();  // Not the last reference.
  EXPECT_TRUE(adapter_ref->GetAdapterForTesting()->is_initialized());
  EXPECT_EQ(1u, map_->GetLocalTrackCount());

  // Destroying all references to the adapter should remove it from the map and
  // dispose it.
  adapter_ref.reset();
  EXPECT_EQ(0u, map_->GetLocalTrackCount());
  EXPECT_EQ(nullptr, map_->GetLocalTrackAdapter(web_track.Id().Utf8()));
  // Allow the disposing of track to occur.
  RunMessageLoopsUntilIdle();
}

TEST_F(WebRtcMediaStreamTrackAdapterMapTest, AddAndRemoveRemoteTrackAdapter) {
  scoped_refptr<MockWebRtcAudioTrack> webrtc_track =
      MockWebRtcAudioTrack::Create("remote_track");
  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> adapter_ref =
      GetOrCreateRemoteTrackAdapter(webrtc_track.get());
  EXPECT_TRUE(adapter_ref->is_initialized());
  EXPECT_EQ(
      adapter_ref->GetAdapterForTesting(),
      map_->GetRemoteTrackAdapter(webrtc_track->id())->GetAdapterForTesting());
  EXPECT_EQ(1u, map_->GetRemoteTrackCount());

  // "GetOrCreate" for already existing track.
  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> adapter_ref2 =
      GetOrCreateRemoteTrackAdapter(webrtc_track.get());
  EXPECT_EQ(adapter_ref->GetAdapterForTesting(),
            adapter_ref2->GetAdapterForTesting());
  EXPECT_EQ(1u, map_->GetRemoteTrackCount());

  adapter_ref2.reset();  // Not the last reference.
  EXPECT_TRUE(adapter_ref->GetAdapterForTesting()->is_initialized());
  EXPECT_EQ(1u, map_->GetRemoteTrackCount());

  // Destroying all references to the adapter should remove it from the map and
  // dispose it.
  adapter_ref.reset();
  EXPECT_EQ(0u, map_->GetRemoteTrackCount());
  EXPECT_EQ(nullptr, map_->GetRemoteTrackAdapter(webrtc_track->id()));
  // Allow the disposing of track to occur.
  RunMessageLoopsUntilIdle();
}

TEST_F(WebRtcMediaStreamTrackAdapterMapTest,
       LocalAndRemoteTrackAdaptersWithSameID) {
  // Local and remote tracks should be able to use the same id without conflict.
  const char* id = "id";

  blink::WebMediaStreamTrack local_web_track = CreateLocalTrack(id);
  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> local_adapter =
      map_->GetOrCreateLocalTrackAdapter(local_web_track);
  EXPECT_TRUE(local_adapter->is_initialized());
  EXPECT_EQ(local_adapter->GetAdapterForTesting(),
            map_->GetLocalTrackAdapter(id)->GetAdapterForTesting());
  EXPECT_EQ(1u, map_->GetLocalTrackCount());

  scoped_refptr<MockWebRtcAudioTrack> remote_webrtc_track =
      MockWebRtcAudioTrack::Create(id);
  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> remote_adapter =
      GetOrCreateRemoteTrackAdapter(remote_webrtc_track.get());
  EXPECT_TRUE(remote_adapter->is_initialized());
  EXPECT_EQ(remote_adapter->GetAdapterForTesting(),
            map_->GetRemoteTrackAdapter(id)->GetAdapterForTesting());
  EXPECT_NE(local_adapter->GetAdapterForTesting(),
            remote_adapter->GetAdapterForTesting());
  EXPECT_EQ(1u, map_->GetRemoteTrackCount());

  // Destroying all references to the adapters should remove them from the map.
  local_adapter.reset();
  remote_adapter.reset();
  EXPECT_EQ(0u, map_->GetLocalTrackCount());
  EXPECT_EQ(0u, map_->GetRemoteTrackCount());
  EXPECT_EQ(nullptr, map_->GetLocalTrackAdapter(id));
  EXPECT_EQ(nullptr, map_->GetRemoteTrackAdapter(id));
  // Allow the disposing of tracks to occur.
  RunMessageLoopsUntilIdle();
}

TEST_F(WebRtcMediaStreamTrackAdapterMapTest, GetLocalTrackAdapterInvalidID) {
  EXPECT_EQ(nullptr, map_->GetLocalTrackAdapter("invalid"));
}

TEST_F(WebRtcMediaStreamTrackAdapterMapTest, GetRemoteTrackAdapterInvalidID) {
  EXPECT_EQ(nullptr, map_->GetRemoteTrackAdapter("invalid"));
}

}  // namespace content
