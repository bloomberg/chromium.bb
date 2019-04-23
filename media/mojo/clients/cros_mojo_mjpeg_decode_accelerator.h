// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_CROS_MOJO_MJPEG_DECODE_ACCELERATOR_H_
#define MEDIA_MOJO_CLIENTS_CROS_MOJO_MJPEG_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "components/chromeos_camera/common/mjpeg_decode_accelerator.mojom.h"
#include "media/video/mjpeg_decode_accelerator.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {

// A MjpegDecodeAccelerator, for use in the browser process, that proxies to a
// chromeos_camera::mojom::MjpegDecodeAccelerator. Created on the owner's
// thread, otherwise operating and deleted on |io_task_runner|.
class CrOSMojoMjpegDecodeAccelerator : public MjpegDecodeAccelerator {
 public:
  CrOSMojoMjpegDecodeAccelerator(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      chromeos_camera::mojom::MjpegDecodeAcceleratorPtrInfo jpeg_decoder);
  ~CrOSMojoMjpegDecodeAccelerator() override;

  // MjpegDecodeAccelerator implementation.
  // |client| is called on the IO thread, but is never called into after the
  // CrOSMojoMjpegDecodeAccelerator is destroyed.
  bool Initialize(Client* client) override;
  void InitializeAsync(Client* client, InitCB init_cb) override;
  void Decode(const BitstreamBuffer& bitstream_buffer,
              const scoped_refptr<VideoFrame>& video_frame) override;
  bool IsSupported() override;

 private:
  void OnInitializeDone(InitCB init_cb,
                        MjpegDecodeAccelerator::Client* client,
                        bool success);
  void OnDecodeAck(int32_t bitstream_buffer_id,
                   ::media::MjpegDecodeAccelerator::Error error);
  void OnLostConnectionToJpegDecoder();

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  Client* client_ = nullptr;

  // Used to safely pass the chromeos_camera::mojom::MjpegDecodeAcceleratorPtr
  // from one thread to another. It is set in the constructor and consumed in
  // InitializeAsync().
  // TODO(mcasas): s/jpeg_decoder_/jda_/ https://crbug.com/699255.
  chromeos_camera::mojom::MjpegDecodeAcceleratorPtrInfo jpeg_decoder_info_;

  chromeos_camera::mojom::MjpegDecodeAcceleratorPtr jpeg_decoder_;

  DISALLOW_COPY_AND_ASSIGN(CrOSMojoMjpegDecodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_CROS_MOJO_MJPEG_DECODE_ACCELERATOR_H_
