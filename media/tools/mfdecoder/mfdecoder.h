// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_TOOLS_MFDECODER_MFDECODER_H_
#define MEDIA_TOOLS_MFDECODER_MFDECODER_H_

#include "base/basictypes.h"

struct IDirect3DDeviceManager9;
struct IMFAttributes;
struct IMFSample;
struct IMFSourceReader;

namespace media {

class MFDecoder {
 public:
  explicit MFDecoder(bool use_dxva2);
  ~MFDecoder();

  // This method is to be called after the constructor. This method
  // creates a source reader with the given URL, and initializes the member
  // variables that are related to the video, such as the dimensions of the
  // video, stride, index of video stream, etc.
  // If DXVA2 was specified in the constructor, then the given device manager
  // is passed into the source reader so that it can do hardware accelerated
  // decoding.
  // Returns: true on success.
  bool Init(const wchar_t* source_url, IDirect3DDeviceManager9* dev_manager);
  int width() const { return width_; }
  int height() const { return height_; }
  bool use_dxva2() const { return use_dxva2_; }
  bool initialized() const { return initialized_; }
  int mfbuffer_stride() const { return mfbuffer_stride_; }
  bool end_of_stream() const { return end_of_stream_; }

  // Reads a single video sample. If end of stream is reached, |end_of_stream_|
  // will be set to true.
  // Returns: Pointer to a IMFSample on success, NULL otherwise. Caller is
  // responsible for releasing the sample.
  IMFSample* ReadVideoSample();

 private:
  // Initializes the COM and MF libraries for this decoder. The two libraries
  // are either both initialized, or both uninitialized.
  // Returns: true if both libraries were successfully initialized.
  bool InitLibraries();

  // Initializes the source reader with the given URL, and device manager if
  // DXVA2 is enabled on the decoder.
  // Returns: true on success.
  bool InitSourceReader(const wchar_t* source_url,
                        IDirect3DDeviceManager9* dev_manager);

  // Called by InitSourceReader() if DXVA2 is to be used. Creates an attribute
  // store that can be passed to the source reader constructor.
  // Caller is responsible for releasing the attribute object.
  // Returns: pointer to an IMFAttributes object if successful, NULL otherwise.
  IMFAttributes* GetDXVA2AttributesForSourceReader(
      IDirect3DDeviceManager9* dev_manager);

  // Deselects any non-video streams, ensures the video stream is selected, and
  // initializes |video_stream_index_| to that video stream.
  // Returns: true on success.
  bool SelectVideoStreamOnly();

  // Obtains information about the video (height, width, etc.) and sets the
  // output format to YV12.
  // Returns: true on success.
  bool InitVideoInfo(IDirect3DDeviceManager9* dev_manager);

  int width_;
  int height_;
  bool use_dxva2_;
  bool initialized_;
  bool com_lib_initialized_;
  bool mf_lib_initialized_;
  IMFSourceReader* reader_;
  int video_stream_index_;
  int mfbuffer_stride_;
  bool end_of_stream_;

  DISALLOW_COPY_AND_ASSIGN(MFDecoder);
};

}  // namespace media

#endif  // MEDIA_TOOLS_MFDECODER_MFDECODER_H_
