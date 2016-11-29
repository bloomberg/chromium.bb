// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_ARC_VIDEO_ACCELERATOR_H_
#define CHROME_GPU_ARC_VIDEO_ACCELERATOR_H_

#include <vector>

#include "base/files/scoped_file.h"

namespace chromeos {
namespace arc {

enum HalPixelFormatExtension {
  // The pixel formats defined in Android but are used here. They are defined
  // in "system/core/include/system/graphics.h"
  HAL_PIXEL_FORMAT_BGRA_8888 = 5,
  HAL_PIXEL_FORMAT_YCbCr_420_888 = 0x23,

  // The following formats are not defined in Android, but used in
  // ArcVideoAccelerator to identify the input format.
  HAL_PIXEL_FORMAT_H264 = 0x34363248,
  HAL_PIXEL_FORMAT_VP8 = 0x00385056,
  HAL_PIXEL_FORMAT_VP9 = 0x00395056,
};

enum PortType {
  PORT_INPUT = 0,
  PORT_OUTPUT = 1,
  PORT_COUNT = 2,
};

struct BufferMetadata {
  int64_t timestamp = 0;  // in microseconds
  uint32_t bytes_used = 0;
};

struct VideoFormat {
  uint32_t pixel_format = 0;
  uint32_t buffer_size = 0;

  // Minimum number of buffers required to decode/encode the stream.
  uint32_t min_num_buffers = 0;
  uint32_t coded_width = 0;
  uint32_t coded_height = 0;
  uint32_t crop_left = 0;
  uint32_t crop_width = 0;
  uint32_t crop_top = 0;
  uint32_t crop_height = 0;
};

// The IPC interface between Android and Chromium for video decoding and
// encoding. Input buffers are sent from Android side and get processed in
// Chromium and the output buffers are returned back to Android side.
class ArcVideoAccelerator {
 public:
  enum Result {
    // Note: this enum is used for UMA reporting. The existing values should not
    // be rearranged, reused or removed and any additions should include updates
    // to UMA reporting and histograms.xml.
    SUCCESS = 0,
    ILLEGAL_STATE = 1,
    INVALID_ARGUMENT = 2,
    UNREADABLE_INPUT = 3,
    PLATFORM_FAILURE = 4,
    INSUFFICIENT_RESOURCES = 5,
    RESULT_MAX = 6,
  };

  struct Config {
    enum DeviceType {
      DEVICE_ENCODER = 0,
      DEVICE_DECODER = 1,
    };

    DeviceType device_type = DEVICE_DECODER;
    size_t num_input_buffers = 0;
    uint32_t input_pixel_format = 0;
    // TODO(owenlin): Add output_pixel_format. For now only the native pixel
    //                format of each VDA on Chromium is supported.
  };

  struct DmabufPlane {
    DmabufPlane(int32_t offset, int32_t stride)
        : offset(offset), stride(stride) {}
    int32_t offset;  // in bytes
    int32_t stride;  // in bytes
  };

  // The callbacks of the ArcVideoAccelerator. The user of this class should
  // implement this interface.
  class Client {
   public:
    virtual ~Client() {}

    // Called when an asynchronous error happens. The errors in Initialize()
    // will not be reported here, but will be indicated by a return value
    // there.
    virtual void OnError(Result error) = 0;

    // Called when a buffer with the specified |index| and |port| has been
    // processed and is no longer used in the accelerator. For input buffers,
    // the Client may fill them with new content. For output buffers, indicates
    // they are ready to be consumed by the client.
    virtual void OnBufferDone(PortType port,
                              uint32_t index,
                              const BufferMetadata& metadata) = 0;

    // Called when the output format has changed or the output format
    // becomes available at beginning of the stream after initial parsing.
    virtual void OnOutputFormatChanged(const VideoFormat& format) = 0;

    // Called as a completion notification for Reset().
    virtual void OnResetDone() = 0;

    // Called as a completion notification for Flush().
    virtual void OnFlushDone() = 0;
  };

  // Initializes the ArcVideoAccelerator with specific configuration. This
  // must be called before any other methods. This call is synchronous and
  // returns SUCCESS iff initialization is successful.
  virtual Result Initialize(const Config& config, Client* client) = 0;

  // Assigns a shared memory to be used for the accelerator at the specified
  // port and index. A buffer must be successfully bound before it can be passed
  // to the accelerator via UseBuffer(). Already bound buffers may be reused
  // multiple times without additional bindings.
  virtual void BindSharedMemory(PortType port,
                                uint32_t index,
                                base::ScopedFD ashmem_fd,
                                off_t offset,
                                size_t length) = 0;

  // Assigns a buffer to be used for the accelerator at the specified
  // port and index. A buffer must be successfully bound before it can be
  // passed to the accelerator via UseBuffer(). Already bound buffers may be
  // reused multiple times without additional bindings.
  virtual void BindDmabuf(PortType port,
                          uint32_t index,
                          base::ScopedFD dmabuf_fd,
                          const std::vector<DmabufPlane>& dmabuf_planes) = 0;

  // Passes a buffer to the accelerator. For input buffer, the accelerator
  // will process it. For output buffer, the accelerator will output content
  // to it.
  virtual void UseBuffer(PortType port,
                         uint32_t index,
                         const BufferMetadata& metadata) = 0;

  // Sets the number of output buffers. When it fails, Client::OnError() will
  // be called.
  virtual void SetNumberOfOutputBuffers(size_t number) = 0;

  // Resets the accelerator. When it is done, Client::OnResetDone() will
  // be called. Afterwards, all buffers won't be accessed by the accelerator
  // and there won't be more callbacks.
  virtual void Reset() = 0;

  // Flushes the accelerator. After all the output buffers pending decode have
  // been returned to client by OnBufferDone(), Client::OnFlushDone() will be
  // called.
  virtual void Flush() = 0;

  virtual ~ArcVideoAccelerator() {}
};

}  // namespace arc
}  // namespace chromeos

#endif  // CHROME_GPU_ARC_VIDEO_ACCELERATOR_H_
