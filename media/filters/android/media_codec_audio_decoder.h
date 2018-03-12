// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_ANDROID_MEDIA_CODEC_AUDIO_DECODER_H_
#define MEDIA_FILTERS_ANDROID_MEDIA_CODEC_AUDIO_DECODER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/android/media_codec_loop.h"
#include "media/base/android/media_drm_bridge_cdm_context.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"

// MediaCodecAudioDecoder is based on Android's MediaCodec API.
// The MediaCodec API is required to play encrypted (as in EME) content on
// Android. It is also a way to employ hardware-accelerated decoding.

// Implementation notes.
//
// This class provides audio decoding via MediaCodec.  It allocates the
// MediaCodecBridge instance, and hands ownership to MediaCodecLoop to drive I/O
// with the codec.  For encrypted streams, we also talk to the DRM bridge.
//
// Because both dequeuing and enqueuing of an input buffer can fail, the
// implementation puts the input |DecoderBuffer|s and the corresponding decode
// callbacks into an input queue. The decoder has a timer that periodically
// fires the decoding cycle that has two steps. The first step tries to send the
// front buffer from the input queue to MediaCodecLoop. In the case of success
// the element is removed from the queue, the decode callback is fired and the
// decoding process advances. The second step tries to dequeue an output buffer,
// and uses it in the case of success.
//
// An EOS buffer is handled differently.  Success is not signalled to the decode
// callback until the EOS is received at the output.  So, for EOS, the decode
// callback indicates that all previous decodes have completed.
//
// The failures in both steps are normal and they happen periodically since
// both input and output buffers become available at unpredictable moments. The
// timer is here to repeat the dequeueing attempts.
//
// State diagram.
//
//   [Uninitialized] <-> (init failed)
//     |         |
//   (no enc.)  (encrypted)
//     |         |
//     |        [WaitingForMediaCrypto] -- (OMCR failure) --> [Uninitialized]
//     |               | (OnMediaCryptoReady success)
//     v               v
//   (Create Codec and MediaCodecLoop)
//     |
//     \--> [Ready] -(any error)-> [Error]
//
//     -[Any state]-
//    |             |
// (Reset ok) (Reset fails)
//    |             |
// [Ready]       [Error]

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class AudioTimestampHelper;

class MEDIA_EXPORT MediaCodecAudioDecoder : public AudioDecoder,
                                            public MediaCodecLoop::Client {
 public:
  explicit MediaCodecAudioDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~MediaCodecAudioDecoder() override;

  // AudioDecoder implementation.
  std::string GetDisplayName() const override;
  void Initialize(
      const AudioDecoderConfig& config,
      CdmContext* cdm_context,
      const InitCB& init_cb,
      const OutputCB& output_cb,
      const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb) override;
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::Closure& closure) override;
  bool NeedsBitstreamConversion() const override;

  // MediaCodecLoop::Client implementation
  bool IsAnyInputPending() const override;
  MediaCodecLoop::InputData ProvideInputData() override;
  void OnInputDataQueued(bool) override;
  bool OnDecodedEos(const MediaCodecLoop::OutputBuffer& out) override;
  bool OnDecodedFrame(const MediaCodecLoop::OutputBuffer& out) override;
  bool OnOutputFormatChanged() override;
  void OnCodecLoopError() override;

 private:
  // Possible states.
  enum State {
    // A successful call to Initialize() is required.
    STATE_UNINITIALIZED,

    // Codec is initialized, but a call to SetCdm() is required.
    STATE_WAITING_FOR_MEDIA_CRYPTO,

    // Normal state.  May decode, etc.
    STATE_READY,

    // Codec must be reset.
    STATE_ERROR,
  };

  // Allocate a new MediaCodec and MediaCodecLoop, and replace |codec_loop_|.
  // Returns true on success.
  bool CreateMediaCodecLoop();

  // A helper method to start CDM initialization.  This must be called if and
  // only if we were constructed with |is_encrypted| set to true.
  void SetCdm(CdmContext* cdm_context, const InitCB& init_cb);

  // This callback is called after CDM obtained a MediaCrypto object.
  void OnMediaCryptoReady(const InitCB& init_cb,
                          JavaObjectPtr media_crypto,
                          bool requires_secure_video_codec);

  // Callback called when a new key is available.
  void OnKeyAdded();

  // Calls DecodeCB with |decode_status| for every frame in |input_queue| and
  // then clears it.
  void ClearInputQueue(DecodeStatus decode_status);

  // Helper method to change the state.
  void SetState(State new_state);

  // Helper method to set sample rate, channel count  and |timestamp_helper_|
  // from |config_|.
  void SetInitialConfiguration();

  // TODO(timav): refactor the common part out and use it here and in AVDA
  // (http://crbug.com/583082).

  // A helper function for logging.
  static const char* AsString(State state);

  // Used to post tasks. This class is single threaded and every method should
  // run on this task runner.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  State state_;

  // The queue of encoded (and maybe encrypted) buffers. The MediaCodec might
  // not be able to accept the input at the time of Decode(), thus all
  // DecoderBuffers first go to |input_queue_|.
  using BufferCBPair = std::pair<scoped_refptr<DecoderBuffer>, DecodeCB>;
  using InputQueue = base::circular_deque<BufferCBPair>;
  InputQueue input_queue_;

  // Cached decoder config.
  AudioDecoderConfig config_;

  // Indication to use passthrough decoder or not.
  bool is_passthrough_;

  // The audio sample format of the audio decoder output.
  SampleFormat sample_format_;

  // Actual channel count that comes from decoder may be different than config.
  int channel_count_;
  ChannelLayout channel_layout_;

  // Actual sample rate that comes from the decoder, may be different than
  // config.
  int sample_rate_;

  // Callback that delivers output frames.
  OutputCB output_cb_;

  std::unique_ptr<MediaCodecLoop> codec_loop_;

  std::unique_ptr<AudioTimestampHelper> timestamp_helper_;

  // CDM related stuff.

  // CDM context that knowns about MediaCrypto. Owned by CDM which is external
  // to this decoder.
  MediaDrmBridgeCdmContext* media_drm_bridge_cdm_context_;

  // MediaDrmBridge requires registration/unregistration of the player, this
  // registration id is used for this.
  int cdm_registration_id_;

  // Pool which helps avoid thrashing memory when returning audio buffers.
  scoped_refptr<AudioBufferMemoryPool> pool_;

  // The MediaCrypto object is used in the MediaCodec.configure() in case of
  // an encrypted stream.
  JavaObjectPtr media_crypto_;

  base::WeakPtrFactory<MediaCodecAudioDecoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaCodecAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_ANDROID_MEDIA_CODEC_AUDIO_DECODER_H_
