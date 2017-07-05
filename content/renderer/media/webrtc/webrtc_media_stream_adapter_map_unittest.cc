// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_media_stream_adapter_map.h"

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_audio_device_factory.h"
#include "content/renderer/media/mock_media_stream_video_source.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter_map.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebHeap.h"

using ::testing::_;

namespace content {

class WebRtcMediaStreamAdapterMapTest : public ::testing::Test {
 public:
  void SetUp() override {
    dependency_factory_.reset(new MockPeerConnectionDependencyFactory());
    main_thread_ = base::ThreadTaskRunnerHandle::Get();
    map_ = new WebRtcMediaStreamAdapterMap(
        dependency_factory_.get(),
        new WebRtcMediaStreamTrackAdapterMap(dependency_factory_.get()));
  }

  void TearDown() override { blink::WebHeap::CollectAllGarbageForTesting(); }

  blink::WebMediaStream CreateLocalStream(const std::string& id) {
    blink::WebVector<blink::WebMediaStreamTrack> web_video_tracks(
        static_cast<size_t>(1));
    blink::WebMediaStreamSource video_source;
    video_source.Initialize("video_source",
                            blink::WebMediaStreamSource::kTypeVideo,
                            "video_source", false /* remote */);
    MediaStreamVideoSource* native_source = new MockMediaStreamVideoSource();
    video_source.SetExtraData(native_source);
    web_video_tracks[0] = MediaStreamVideoTrack::CreateVideoTrack(
        native_source, MediaStreamVideoSource::ConstraintsCallback(), true);

    blink::WebMediaStream web_stream;
    web_stream.Initialize(blink::WebString::FromUTF8(id),
                          blink::WebVector<blink::WebMediaStreamTrack>(),
                          web_video_tracks);
    web_stream.SetExtraData(new MediaStream());
    return web_stream;
  }

 protected:
  // Message loop and child processes is needed for task queues and threading to
  // work, as is necessary to create tracks and adapters.
  base::MessageLoop message_loop_;
  ChildProcess child_process_;

  std::unique_ptr<MockPeerConnectionDependencyFactory> dependency_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  scoped_refptr<WebRtcMediaStreamAdapterMap> map_;
};

TEST_F(WebRtcMediaStreamAdapterMapTest, AddAndRemoveLocalStreamAdapter) {
  blink::WebMediaStream local_stream = CreateLocalStream("local_stream");
  std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef> adapter_ref =
      map_->GetOrCreateLocalStreamAdapter(local_stream);
  EXPECT_TRUE(adapter_ref);
  EXPECT_TRUE(adapter_ref->adapter().IsEqual(local_stream));
  EXPECT_EQ(1u, map_->GetLocalStreamCount());

  std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef> adapter_ref2 =
      map_->GetLocalStreamAdapter("local_stream");
  EXPECT_TRUE(adapter_ref2);
  EXPECT_EQ(&adapter_ref2->adapter(), &adapter_ref->adapter());
  EXPECT_EQ(1u, map_->GetLocalStreamCount());

  adapter_ref.reset();
  EXPECT_EQ(1u, map_->GetLocalStreamCount());
  adapter_ref2.reset();
  EXPECT_EQ(0u, map_->GetLocalStreamCount());
}

TEST_F(WebRtcMediaStreamAdapterMapTest, GetLocalStreamAdapterInvalidID) {
  EXPECT_FALSE(map_->GetLocalStreamAdapter("invalid"));
}

}  // namespace content
