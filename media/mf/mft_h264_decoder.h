// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Decodes H.264 Annex B streams using the Media Foundation H.264 decoder as
// a standalone Media Foundation Transform (MFT).
// Note: A single MftH264Decoder instance is only for 1 H.264 video stream only.
// Inputting streams consisting of more than 1 video to a single instance
// may result in undefined behavior.

#ifndef MEDIA_MF_MFT_H264_DECODER_H_
#define MEDIA_MF_MFT_H264_DECODER_H_

#include <string>

#include <mfidl.h>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "base/scoped_comptr_win.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

struct IDirect3DDeviceManager9;
struct IMFTransform;

namespace media {

class VideoFrame;

// A decoder that takes samples of Annex B streams then outputs decoded frames.
class MftH264Decoder : public base::RefCountedThreadSafe<MftH264Decoder> {
 public:
  enum DecoderOutputState {
    kOutputOk = 0,
    kResetOutputStreamFailed,
    kNoMoreOutput,
    kUnspecifiedError,
    kNoMemory,
    kOutputSampleError
  };
  typedef Callback4<uint8**, int*, int64*, int64*>::Type ReadInputCallback;
  typedef Callback1<scoped_refptr<VideoFrame> >::Type OutputReadyCallback;

  explicit MftH264Decoder(bool use_dxva);
  ~MftH264Decoder();

  // Initializes the decoder. |dev_manager| is not required if the decoder does
  // not use DXVA.
  // If the other arguments are not known, leave them as 0. They can be
  // provided to the decoder to try to avoid an initial output format change,
  // but it is not necessary to have them.
  // The object takes ownership of the callbacks. However, the caller must
  // make sure the objects associated with the callbacks outlives the time
  // when GetOutput() will be called.
  bool Init(IDirect3DDeviceManager9* dev_manager,
            int frame_rate_num, int frame_rate_denom,
            int width, int height,
            int aspect_num, int aspect_denom,
            ReadInputCallback* read_input_cb,
            OutputReadyCallback* output_avail_cb);

  // Sends an Annex B stream to the decoder. The times here should be given
  // in 100ns units. This creates a IMFSample, copies the stream over to the
  // sample, and sends the sample to the decoder.
  // Returns: true if the sample was sent successfully.
  bool SendInput(uint8* data, int size, int64 timestamp, int64 duration);

  // Tries to get an output sample from the decoder, and if successful, calls
  // the callback with the sample.
  // Returns: status of the decoder.
  DecoderOutputState GetOutput();

  bool initialized() const { return initialized_; }
  bool use_dxva() const { return use_dxva_; }
  bool drain_message_sent() const { return drain_message_sent_; }
  int in_buffer_size() const { return in_buffer_size_; }
  int out_buffer_size() const { return out_buffer_size_; }
  int frames_read() const { return frames_read_; }
  int frames_decoded() const { return frames_decoded_; }
  int width() const { return width_; }
  int height() const { return height_; }

 private:
  friend class MftH264DecoderTest;
  FRIEND_TEST(MftH264DecoderTest, SendDrainMessageBeforeInitDeathTest);
  FRIEND_TEST(MftH264DecoderTest, SendDrainMessageAtInit);
  FRIEND_TEST(MftH264DecoderTest, DrainOnEndOfInputStream);
  FRIEND_TEST(MftH264DecoderTest, NoOutputOnGarbageInput);

  bool InitComMfLibraries();
  bool InitDecoder(IDirect3DDeviceManager9* dev_manager,
                   int frame_rate_num, int frame_rate_denom,
                   int width, int height,
                   int aspect_num, int aspect_denom);
  bool SetDecoderD3d9Manager(IDirect3DDeviceManager9* dev_manager);
  bool SetDecoderMediaTypes(int frame_rate_num, int frame_rate_denom,
                            int width, int height,
                            int aspect_num, int aspect_denom);
  bool SetDecoderInputMediaType(int frame_rate_num, int frame_rate_denom,
                                int width, int height,
                                int aspect_num, int aspect_denom);
  bool SetDecoderOutputMediaType(const GUID subtype);
  bool SendStartMessage();
  bool GetStreamsInfoAndBufferReqs();
  bool ReadAndProcessInput();

  // Sends a drain message to the decoder to indicate no more input will be
  // sent. SendInput() should not be called after calling this method.
  // Returns: true if the drain message was sent successfully.
  bool SendDrainMessage();

  // |output_error_callback_| should stop the message loop.
  scoped_ptr<ReadInputCallback> read_input_callback_;
  scoped_ptr<OutputReadyCallback> output_avail_callback_;
  IMFTransform* decoder_;
  bool initialized_;
  bool use_dxva_;
  bool drain_message_sent_;

  // Minimum input and output buffer sizes/alignment required by the decoder.
  // If |buffer_alignment_| is zero, then the buffer needs not be aligned.
  int in_buffer_size_;
  int in_buffer_alignment_;
  int out_buffer_size_;
  int out_buffer_alignment_;
  int frames_read_;
  int frames_decoded_;
  int width_;
  int height_;
  int stride_;
  const GUID output_format_;

  DISALLOW_COPY_AND_ASSIGN(MftH264Decoder);
};

}  // namespace media

#endif  // MEDIA_MF_MFT_H264_DECODER_H_

