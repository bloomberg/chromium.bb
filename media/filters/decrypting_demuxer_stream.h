// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DECRYPTING_DEMUXER_STREAM_H_
#define MEDIA_FILTERS_DECRYPTING_DEMUXER_STREAM_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

class DecoderBuffer;

// Decryptor-based DemuxerStream implementation that converts a potentially
// encrypted demuxer stream to a clear demuxer stream.
// All public APIs and callbacks are trampolined to the |message_loop_| so
// that no locks are required for thread safety.
class MEDIA_EXPORT DecryptingDemuxerStream : public DemuxerStream {
 public:
  DecryptingDemuxerStream(
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      const SetDecryptorReadyCB& set_decryptor_ready_cb);
  virtual ~DecryptingDemuxerStream();

  void Initialize(DemuxerStream* stream,
                  const PipelineStatusCB& status_cb);
  void Reset(const base::Closure& closure);

  // DemuxerStream implementation.
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual const AudioDecoderConfig& audio_decoder_config() OVERRIDE;
  virtual const VideoDecoderConfig& video_decoder_config() OVERRIDE;
  virtual Type type() OVERRIDE;
  virtual void EnableBitstreamConverter() OVERRIDE;

 private:
  // For a detailed state diagram please see this link: http://goo.gl/8jAok
  // TODO(xhwang): Add a ASCII state diagram in this file after this class
  // stabilizes.
  // TODO(xhwang): Update this diagram for DecryptingDemuxerStream.
  enum State {
    kUninitialized = 0,
    kDecryptorRequested,
    kIdle,
    kPendingDemuxerRead,
    kPendingDecrypt,
    kWaitingForKey,
  };

  // Callback for DecryptorHost::RequestDecryptor().
  void SetDecryptor(Decryptor* decryptor);

  // Callback for DemuxerStream::Read().
  void DecryptBuffer(DemuxerStream::Status status,
                     const scoped_refptr<DecoderBuffer>& buffer);

  void DecryptPendingBuffer();

  // Callback for Decryptor::Decrypt().
  void DeliverBuffer(Decryptor::Status status,
                     const scoped_refptr<DecoderBuffer>& decrypted_buffer);

  // Callback for the |decryptor_| to notify this object that a new key has been
  // added.
  void OnKeyAdded();

  // Resets decoder and calls |reset_cb_|.
  void DoReset();

  // Returns Decryptor::StreamType converted from |stream_type_|.
  Decryptor::StreamType GetDecryptorStreamType() const;

  // Creates and initializes either |audio_config_| or |video_config_| based on
  // |demuxer_stream_|.
  void InitializeDecoderConfig();

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  base::WeakPtrFactory<DecryptingDemuxerStream> weak_factory_;
  base::WeakPtr<DecryptingDemuxerStream> weak_this_;

  State state_;

  PipelineStatusCB init_cb_;
  ReadCB read_cb_;
  base::Closure reset_cb_;

  // Pointer to the input demuxer stream that will feed us encrypted buffers.
  DemuxerStream* demuxer_stream_;

  scoped_ptr<AudioDecoderConfig> audio_config_;
  scoped_ptr<VideoDecoderConfig> video_config_;

  // Callback to request/cancel decryptor creation notification.
  SetDecryptorReadyCB set_decryptor_ready_cb_;

  Decryptor* decryptor_;

  // The buffer returned by the demuxer that needs to be decrypted.
  scoped_refptr<media::DecoderBuffer> pending_buffer_to_decrypt_;

  // Indicates the situation where new key is added during pending decryption
  // (in other words, this variable can only be set in state kPendingDecrypt).
  // If this variable is true and kNoKey is returned then we need to try
  // decrypting again in case the newly added key is the correct decryption key.
  bool key_added_while_decrypt_pending_;

  DISALLOW_COPY_AND_ASSIGN(DecryptingDemuxerStream);
};

}  // namespace media

#endif  // MEDIA_FILTERS_DECRYPTING_DEMUXER_STREAM_H_
