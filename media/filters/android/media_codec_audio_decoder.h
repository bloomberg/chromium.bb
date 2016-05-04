// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_ANDROID_MEDIA_CODEC_AUDIO_DECODER_H_
#define MEDIA_FILTERS_ANDROID_MEDIA_CODEC_AUDIO_DECODER_H_

#include <deque>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_drm_bridge_cdm_context.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"

// MediaCodecAudioDecoder is based on Android's MediaCodec API.
// The MediaCodec API is required to play encrypted (as in EME) content on
// Android. It is also a way to employ hardware-accelerated decoding.

// TODO(timav): This class has the logic to manipulate the MediaCodec that is
// the common for audio and video stream and can and should be refactored out
// (http://crbug.com/583082).

// Implementation notes.
//
// The MediaCodec
// (http://developer.android.com/reference/android/media/MediaCodec.html) works
// by exchanging buffers between the client and the codec itself. On the input
// side an "empty" buffer has to be dequeued from the codec, filled with data
// and queued back. On the output side a "full" buffer with data should be
// dequeued, the data is to be used somehow (copied out, or rendered to a pre-
// defined texture for video) and the buffer has to be returned back (released).
// Neither input nor output dequeue operations are guaranteed to succeed: the
// codec might not have available input buffers yet, or not every encoded buffer
// has arrived to complete an output frame. In such case the client should try
// to dequeue a buffer again at a later time.
//
// There is also a special situation related to an encrypted stream, where the
// enqueuing of a filled input buffer might fail due to lack of the relevant key
// in the CDM module.
//
// Because both dequeuing and enqueuing of an input buffer can fail, the
// implementation puts the input |DecoderBuffer|s and the corresponding decode
// callbacks into an input queue. The decoder has a timer that periodically
// fires the decoding cycle that has two steps. The first step tries to send the
// front buffer from the input queue to the MediaCodec. In the case of success
// the element is removed from the queue, the decode callback is fired and the
// decoding process advances. The second step tries to dequeue an output buffer,
// and uses it in the case of success.
//
// The failures in both steps are normal and they happen periodically since
// both input and output buffers become available at unpredictable moments. The
// timer is here to repeat the dequeueing attempts.
//
// Although one can specify a delay in the MediaCodec's dequeue operations,
// this implementation follows the simple logic which is similar to
// AndroidVideoDecodeAccelerator: no delay for either input or output buffers,
// the processing is initated by the timer with short period (10 ms). Using no
// delay for enqueue operations has an extra benefit of not blocking the current
// thread.
//
// This implementation detects the MediaCodec idle run (no input or output
// buffer processing) and after being idle for a predefined time the timer
// stops. Every Decode() wakes the timer up.
//
// The current implementation is single threaded. Every method is supposed to
// run on the same thread.
//
// State diagram.
//
//   [Uninitialized] <-> (init failed)          [Ready]
//          |                                      |
//   (init succeeded)                     (MEDIA_CODEC_NO_KEY)
//          |                                      |
//       [Ready]                             [WaitingForKey]
//          |                                      |
//   (EOS enqueued)                           (OnKeyAdded)
//          |                                      |
//      [Draining]                              [Ready]
//          |
// (EOS dequeued on output)
//          |
//      [Drained]
//
//   [Ready, WaitingForKey, Draining]          -[Any state]-
//                 |                          |             |
//         (MediaCodec error)             (Reset ok) (Reset fails)
//                 |                          |             |
//              [Error]                    [Ready]       [Error]

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class AudioTimestampHelper;

class MEDIA_EXPORT MediaCodecAudioDecoder : public AudioDecoder {
 public:
  explicit MediaCodecAudioDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~MediaCodecAudioDecoder() override;

  // AudioDecoder implementation.
  std::string GetDisplayName() const override;
  void Initialize(const AudioDecoderConfig& config,
                  CdmContext* cdm_context,
                  const InitCB& init_cb,
                  const OutputCB& output_cb) override;
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::Closure& closure) override;
  bool NeedsBitstreamConversion() const override;

 private:
  // Possible states.
  enum State {
    STATE_UNINITIALIZED,
    STATE_WAITING_FOR_MEDIA_CRYPTO,
    STATE_READY,
    STATE_WAITING_FOR_KEY,
    STATE_DRAINING,
    STATE_DRAINED,
    STATE_ERROR,
  };

  // Information about dequeued input buffer.
  struct InputBufferInfo {
    int buf_index;    // The codec input buffers are referred to by this index.
    bool is_pending;  // True if we tried to enqueue this buffer before.
    InputBufferInfo(int i, bool p) : buf_index(i), is_pending(p) {}
  };

  // Information about the MediaCodec's output buffer.
  struct OutputBufferInfo {
    int buf_index;  // The codec output buffers are referred to by this index.
    size_t offset;  // Position in the buffer where data starts.
    size_t size;    // The size of the buffer (includes offset).
    base::TimeDelta pts;  // Presentation timestamp.
    bool is_eos;          // true if this buffer is the end of stream.
    bool is_key_frame;
  };

  // A helper method to start CDM initialization.
  void SetCdm(CdmContext* cdm_context, const InitCB& init_cb);

  // This callback is called after CDM obtained a MediaCrypto object.
  void OnMediaCryptoReady(
      const InitCB& init_cb,
      media::MediaDrmBridgeCdmContext::JavaObjectPtr media_crypto,
      bool needs_protected_surface);

  // Callback called when a new key is available after the codec received
  // the status MEDIA_CODEC_NO_KEY.
  void OnKeyAdded();

  // Does the MediaCodec processing cycle: enqueues an input buffer, then
  // dequeues output buffers.
  void DoIOTask();

  // Enqueues pending input buffers into MediaCodec as long as it can happen
  // without delay in dequeuing and enqueueing input buffers.
  // Returns true if any input was processed.
  bool QueueInput();

  // Enqueues one pending input buffer into MediaCodec if MediaCodec has room.
  // Returns true if any input was processed.
  bool QueueOneInputBuffer();

  // A helper method for QueueInput(). Dequeues an empty input buffer from the
  // codec and returns the information about it. OutputBufferInfo.buf_index is
  // the index of the dequeued buffer or -1 if the codec is busy or an error
  // occured.  OutputBufferInfo.is_pending is set to true if we tried to enqueue
  // this buffer before. In this case the buffer is already filled with data.
  // In the case of an error sets STATE_ERROR.
  InputBufferInfo DequeueInputBuffer();

  // A helper method for QueueInput(). Fills an input buffer referred to by
  // |input_info| with data and enqueues it to the codec. Returns true if
  // succeeded or false if an error occurs or there is no good CDM key.
  // May set STATE_DRAINING, STATE_WAITING_FOR_KEY or STATE_ERROR.
  bool EnqueueInputBuffer(const InputBufferInfo& input_info);

  // Calls DecodeCB with |decode_status| for every frame in |input_queue| and
  // then clears it.
  void ClearInputQueue(DecodeStatus decode_status);

  // Dequeues all output buffers from MediaCodec that are immediately available.
  // Returns true if any output buffer was received from MediaCodec.
  bool DequeueOutput();

  // Start the timer immediately if |start| is true or stop it based on elapsed
  // idle time if |start| is false.
  void ManageTimer(bool start);

  // Helper method to change the state.
  void SetState(State new_state);

  // Helper method for DequeueOutput(), processes end of stream.
  void OnDecodedEos(const OutputBufferInfo& out);

  // The following helper methods OnDecodedFrame() and OnOutputFormatChanged()
  // are specific to the stream (audio/video), but others seem to apply to any
  // MediaCodec decoder.
  // TODO(timav): refactor the common part out and use it here and in AVDA
  // (http://crbug.com/583082).

  // Processes the output buffer after it comes from MediaCodec.
  void OnDecodedFrame(const OutputBufferInfo& out);

  // Processes the output format change.
  void OnOutputFormatChanged();

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
  using InputQueue = std::deque<BufferCBPair>;
  InputQueue input_queue_;

  // Cached decoder config.
  AudioDecoderConfig config_;

  // Actual channel count that comes from decoder may be different than config.
  int channel_count_;

  // Callback that delivers output frames.
  OutputCB output_cb_;

  std::unique_ptr<MediaCodecBridge> media_codec_;

  std::unique_ptr<AudioTimestampHelper> timestamp_helper_;

  // Repeating timer that kicks MediaCodec operation.
  base::RepeatingTimer io_timer_;

  // Time at which we last did useful work on |io_timer_|.
  base::TimeTicks idle_time_begin_;

  // Index of the dequeued and filled buffer that we keep trying to enqueue.
  // Such buffer appears in MEDIA_CODEC_NO_KEY processing. The -1 value means
  // there is no such buffer.
  int pending_input_buf_index_;

  // CDM related stuff.

  // CDM context that knowns about MediaCrypto. Owned by CDM which is external
  // to this decoder.
  MediaDrmBridgeCdmContext* media_drm_bridge_cdm_context_;

  // MediaDrmBridge requires registration/unregistration of the player, this
  // registration id is used for this.
  int cdm_registration_id_;

  // The MediaCrypto object is used in the MediaCodec.configure() in case of
  // an encrypted stream.
  media::MediaDrmBridgeCdmContext::JavaObjectPtr media_crypto_;

  base::WeakPtrFactory<MediaCodecAudioDecoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaCodecAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_ANDROID_MEDIA_CODEC_AUDIO_DECODER_H_
