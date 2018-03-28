// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_rtp_sender.h"

#include <utility>

#include "base/logging.h"
#include "content/renderer/media/webrtc/rtc_dtmf_sender_handler.h"
#include "content/renderer/media/webrtc/rtc_rtp_parameters.h"
#include "content/renderer/media/webrtc/rtc_stats.h"

namespace content {

namespace {

// TODO(hbos): Replace WebRTCVoidRequest with something resolving promises based
// on RTCError, as to surface both exception type and error message.
// https://crbug.com/790007
void OnReplaceTrackCompleted(blink::WebRTCVoidRequest request, bool result) {
  if (result)
    request.RequestSucceeded();
  else
    request.RequestFailed(blink::WebRTCError(
        blink::WebRTCErrorType::kInvalidModification, blink::WebString()));
}

}  // namespace

class RTCRtpSender::RTCRtpSenderInternal
    : public base::RefCountedThreadSafe<RTCRtpSender::RTCRtpSenderInternal> {
 public:
  RTCRtpSenderInternal(
      scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread,
      scoped_refptr<base::SingleThreadTaskRunner> signaling_thread,
      scoped_refptr<WebRtcMediaStreamAdapterMap> stream_map,
      rtc::scoped_refptr<webrtc::RtpSenderInterface> webrtc_sender,
      blink::WebMediaStreamTrack web_track,
      std::vector<blink::WebMediaStream> web_streams)
      : native_peer_connection_(std::move(native_peer_connection)),
        main_thread_(std::move(main_thread)),
        signaling_thread_(std::move(signaling_thread)),
        stream_map_(std::move(stream_map)),
        webrtc_sender_(std::move(webrtc_sender)) {
    DCHECK(main_thread_);
    DCHECK(signaling_thread_);
    DCHECK(stream_map_);
    DCHECK(webrtc_sender_);
    if (!web_track.IsNull()) {
      track_ref_ =
          stream_map_->track_adapter_map()->GetOrCreateLocalTrackAdapter(
              web_track);
    }
    for (size_t i = 0; i < web_streams.size(); ++i) {
      if (!web_streams[i].IsNull()) {
        stream_refs_.push_back(
            stream_map_->GetOrCreateLocalStreamAdapter(web_streams[i]));
      }
    }
  }

  RTCRtpSenderInternal(
      scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread,
      scoped_refptr<base::SingleThreadTaskRunner> signaling_thread,
      scoped_refptr<WebRtcMediaStreamAdapterMap> stream_map,
      rtc::scoped_refptr<webrtc::RtpSenderInterface> webrtc_sender,
      std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref,
      std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
          stream_refs)
      : native_peer_connection_(std::move(native_peer_connection)),
        main_thread_(std::move(main_thread)),
        signaling_thread_(std::move(signaling_thread)),
        stream_map_(std::move(stream_map)),
        webrtc_sender_(std::move(webrtc_sender)),
        track_ref_(std::move(track_ref)),
        stream_refs_(std::move(stream_refs)) {
    DCHECK(main_thread_);
    DCHECK(signaling_thread_);
    DCHECK(stream_map_);
    DCHECK(webrtc_sender_);
  }

  webrtc::RtpSenderInterface* webrtc_sender() const {
    return webrtc_sender_.get();
  }

  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref()
      const {
    return track_ref_ ? track_ref_->Copy() : nullptr;
  }

  std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
  stream_refs() const {
    std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
        stream_ref_copies(stream_refs_.size());
    for (size_t i = 0; i < stream_refs_.size(); ++i)
      stream_ref_copies[i] = stream_refs_[i]->Copy();
    return stream_ref_copies;
  }

  void ReplaceTrack(blink::WebMediaStreamTrack with_track,
                    base::OnceCallback<void(bool)> callback) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref;
    webrtc::MediaStreamTrackInterface* webrtc_track = nullptr;
    if (!with_track.IsNull()) {
      track_ref =
          stream_map_->track_adapter_map()->GetOrCreateLocalTrackAdapter(
              with_track);
      webrtc_track = track_ref->webrtc_track();
    }
    signaling_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RTCRtpSender::RTCRtpSenderInternal::ReplaceTrackOnSignalingThread,
            this, std::move(track_ref), base::Unretained(webrtc_track),
            std::move(callback)));
  }

  std::unique_ptr<blink::WebRTCDTMFSenderHandler> GetDtmfSender() const {
    // The webrtc_sender is a proxy, so this is a blocking call to the
    // webrtc signalling thread.
    DCHECK(main_thread_->BelongsToCurrentThread());
    auto dtmf_sender = webrtc_sender()->GetDtmfSender();
    return std::make_unique<RtcDtmfSenderHandler>(dtmf_sender);
  }

  std::unique_ptr<blink::WebRTCRtpParameters> GetParameters() const {
    webrtc::RtpParameters parameters = webrtc_sender_->GetParameters();
    return std::make_unique<blink::WebRTCRtpParameters>(
        GetWebRTCRtpParameters(parameters));
  }

  void GetStats(std::unique_ptr<blink::WebRTCStatsReportCallback> callback) {
    signaling_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RTCRtpSender::RTCRtpSenderInternal::GetStatsOnSignalingThread,
            this, std::move(callback)));
  }

  bool RemoveFromPeerConnection(webrtc::PeerConnectionInterface* pc) {
    if (!pc->RemoveTrack(webrtc_sender_))
      return false;
    // TODO(hbos): Removing the track should null the sender's track, or we
    // should do |webrtc_sender_->SetTrack(null)| but that is not allowed on a
    // stopped sender. In the meantime, there is a discrepancy between layers.
    // https://crbug.com/webrtc/7945
    track_ref_.reset();
    return true;
  }

 private:
  friend class base::RefCountedThreadSafe<RTCRtpSenderInternal>;
  virtual ~RTCRtpSenderInternal() {}

  // |webrtc_track| is passed as an argument because |track_ref->webrtc_track()|
  // cannot be accessed on the signaling thread. https://crbug.com/756436
  void ReplaceTrackOnSignalingThread(
      std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref,
      webrtc::MediaStreamTrackInterface* webrtc_track,
      base::OnceCallback<void(bool)> callback) {
    DCHECK(signaling_thread_->BelongsToCurrentThread());
    bool result = webrtc_sender_->SetTrack(webrtc_track);
    main_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RTCRtpSender::RTCRtpSenderInternal::ReplaceTrackCallback, this,
            result, std::move(track_ref), std::move(callback)));
  }

  void ReplaceTrackCallback(
      bool result,
      std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref,
      base::OnceCallback<void(bool)> callback) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    if (result)
      track_ref_ = std::move(track_ref);
    std::move(callback).Run(result);
  }

  void GetStatsOnSignalingThread(
      std::unique_ptr<blink::WebRTCStatsReportCallback> callback) {
    native_peer_connection_->GetStats(webrtc_sender_,
                                      RTCStatsCollectorCallbackImpl::Create(
                                          main_thread_, std::move(callback)));
  }

  const scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  const scoped_refptr<base::SingleThreadTaskRunner> signaling_thread_;
  const scoped_refptr<WebRtcMediaStreamAdapterMap> stream_map_;
  const rtc::scoped_refptr<webrtc::RtpSenderInterface> webrtc_sender_;
  // The track adapter is the glue between blink and webrtc layer tracks.
  // Keeping a reference to the adapter ensures it is not disposed, as is
  // required as long as the webrtc layer track is in use by the sender.
  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref_;
  // Similarly, reference needs to be kept to the stream adapters of the
  // sender's associated set of streams.
  std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
      stream_refs_;
};

uintptr_t RTCRtpSender::getId(const webrtc::RtpSenderInterface* webrtc_sender) {
  return reinterpret_cast<uintptr_t>(webrtc_sender);
}

RTCRtpSender::RTCRtpSender(
    scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    scoped_refptr<base::SingleThreadTaskRunner> signaling_thread,
    scoped_refptr<WebRtcMediaStreamAdapterMap> stream_map,
    rtc::scoped_refptr<webrtc::RtpSenderInterface> webrtc_sender,
    blink::WebMediaStreamTrack web_track,
    std::vector<blink::WebMediaStream> web_streams)
    : internal_(new RTCRtpSenderInternal(std::move(native_peer_connection),
                                         std::move(main_thread),
                                         std::move(signaling_thread),
                                         std::move(stream_map),
                                         std::move(webrtc_sender),
                                         std::move(web_track),
                                         std::move(web_streams))) {}

RTCRtpSender::RTCRtpSender(
    scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    scoped_refptr<base::SingleThreadTaskRunner> signaling_thread,
    scoped_refptr<WebRtcMediaStreamAdapterMap> stream_map,
    rtc::scoped_refptr<webrtc::RtpSenderInterface> webrtc_sender,
    std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref,
    std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
        stream_refs)
    : internal_(new RTCRtpSenderInternal(std::move(native_peer_connection),
                                         std::move(main_thread),
                                         std::move(signaling_thread),
                                         std::move(stream_map),
                                         std::move(webrtc_sender),
                                         std::move(track_ref),
                                         std::move(stream_refs))) {}

RTCRtpSender::RTCRtpSender(const RTCRtpSender& other)
    : internal_(other.internal_) {}

RTCRtpSender::~RTCRtpSender() {}

RTCRtpSender& RTCRtpSender::operator=(const RTCRtpSender& other) {
  internal_ = other.internal_;
  return *this;
}

std::unique_ptr<RTCRtpSender> RTCRtpSender::ShallowCopy() const {
  return std::make_unique<RTCRtpSender>(*this);
}

uintptr_t RTCRtpSender::Id() const {
  return getId(internal_->webrtc_sender());
}

blink::WebMediaStreamTrack RTCRtpSender::Track() const {
  auto track_ref = internal_->track_ref();
  return track_ref ? track_ref->web_track() : blink::WebMediaStreamTrack();
}

void RTCRtpSender::ReplaceTrack(blink::WebMediaStreamTrack with_track,
                                blink::WebRTCVoidRequest request) {
  internal_->ReplaceTrack(
      std::move(with_track),
      base::BindOnce(&OnReplaceTrackCompleted, std::move(request)));
}

std::unique_ptr<blink::WebRTCDTMFSenderHandler> RTCRtpSender::GetDtmfSender()
    const {
  return internal_->GetDtmfSender();
}

std::unique_ptr<blink::WebRTCRtpParameters> RTCRtpSender::GetParameters()
    const {
  return internal_->GetParameters();
}

void RTCRtpSender::GetStats(
    std::unique_ptr<blink::WebRTCStatsReportCallback> callback) {
  internal_->GetStats(std::move(callback));
}

webrtc::RtpSenderInterface* RTCRtpSender::webrtc_sender() const {
  return internal_->webrtc_sender();
}

const webrtc::MediaStreamTrackInterface* RTCRtpSender::webrtc_track() const {
  auto track_ref = internal_->track_ref();
  return track_ref ? track_ref->webrtc_track() : nullptr;
}

std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
RTCRtpSender::stream_refs() const {
  return internal_->stream_refs();
}

void RTCRtpSender::ReplaceTrack(blink::WebMediaStreamTrack with_track,
                                base::OnceCallback<void(bool)> callback) {
  internal_->ReplaceTrack(std::move(with_track), std::move(callback));
}

bool RTCRtpSender::RemoveFromPeerConnection(
    webrtc::PeerConnectionInterface* pc) {
  return internal_->RemoveFromPeerConnection(pc);
}

}  // namespace content
