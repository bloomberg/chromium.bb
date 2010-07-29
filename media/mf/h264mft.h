// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// Decodes H.264 Annex B streams using the Media Foundation H.264 decoder as
// a standalone Media Foundation Transform (MFT).
// Note: A single H264Mft instance is only for 1 H.264 video stream only.
// Inputting streams consisting of more than 1 video to a single instance
// may result in undefined behavior.

#ifndef MEDIA_MF_H264MFT_H_
#define MEDIA_MF_H264MFT_H_

#include <string>

#include <mfidl.h>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/scoped_comptr_win.h"
#include "media/base/video_frame.h"

struct IDirect3DDeviceManager9;
struct IMFSample;
struct IMFTransform;

namespace media {

// A decoder that takes samples of Annex B streams then outputs decoded frames.
class H264Mft {
 public:
  enum DecoderOutputState {
    kOutputOk = 0,
    kResetOutputStreamFailed,
    kResetOutputStreamOk,
    kNeedMoreInput,
    kNoMoreOutput,
    kUnspecifiedError,
    kNoMemory,
    kOutputSampleError
  };
  explicit H264Mft(bool use_dxva);
  ~H264Mft();

  // Initializes the decoder. |dev_manager| is not required if the decoder does
  // not use DXVA.
  // If the other arguments are not known, leave them as 0. They can be
  // provided to the decoder to try to avoid an initial output format change,
  // but it is not necessary to have them.
  bool Init(IDirect3DDeviceManager9* dev_manager,
            int frame_rate_num, int frame_rate_denom,
            int width, int height,
            int aspect_num, int aspect_denom);

  // Sends an Annex B stream to the decoder. The times here should be given
  // in 100ns units. This creates a IMFSample, copies the stream over to the
  // sample, and sends the sample to the decoder.
  // Returns: true if the sample was sent successfully.
  bool SendInput(uint8* data, int size, int64 timestamp, int64 duration);

  // Tries to get an output sample from the decoder.
  // Returns: status of the decoder, and if successful, a decoded sample.
  DecoderOutputState GetOutput(scoped_refptr<VideoFrame>* decoded_frame);

  // Sends a drain message to the decoder to indicate no more input will be
  // sent. SendInput() should not be called after calling this method.
  // Returns: true if the drain message was sent successfully.
  bool SendDrainMessage();

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
  bool InitDecoder(IDirect3DDeviceManager9* dev_manager,
                   int frame_rate_num, int frame_rate_denom,
                   int width, int height,
                   int aspect_num, int aspect_denom);
  bool CheckDecoderProperties();
  bool CheckDecoderDxvaSupport();
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

  ScopedComPtr<IMFTransform> decoder_;
  bool initialized_;
  bool use_dxva_;
  bool drain_message_sent_;

  // Minimum input and output buffer sizes as required by the decoder.
  int in_buffer_size_;
  int out_buffer_size_;
  int frames_read_;
  int frames_decoded_;
  int width_;
  int height_;
  int stride_;

  DISALLOW_COPY_AND_ASSIGN(H264Mft);
};

}  // namespace media

#endif  // MEDIA_MF_H264MFT_H_
