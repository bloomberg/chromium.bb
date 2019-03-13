// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_STREAM_BUFFER_MANAGER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_STREAM_BUFFER_MANAGER_H_

#include <cstring>
#include <initializer_list>
#include <memory>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "media/capture/video/chromeos/camera_device_delegate.h"
#include "media/capture/video/chromeos/mojo/camera3.mojom.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace gfx {

class GpuMemoryBuffer;

}  // namespace gfx

namespace media {

class CameraBufferFactory;
class CameraDeviceContext;

struct BufferInfo;

// StreamBufferManager is responsible for managing the buffers of the
// stream.  StreamBufferManager allocates buffers according to the given
// stream configuration.
class CAPTURE_EXPORT StreamBufferManager final {
 public:
  StreamBufferManager(
      CameraDeviceContext* device_context,
      std::unique_ptr<CameraBufferFactory> camera_buffer_factory);
  ~StreamBufferManager();

  gfx::GpuMemoryBuffer* GetBufferById(StreamType stream_type,
                                      uint64_t buffer_id);

  VideoCaptureFormat GetStreamCaptureFormat(StreamType stream_type);

  // Checks if all streams are available. For output stream, it is available if
  // it has free buffers. For input stream, it is always available.
  bool HasFreeBuffers(const std::set<StreamType>& stream_types);

  // Checks if the target stream types have been configured or not.
  bool HasStreamsConfigured(std::initializer_list<StreamType> stream_types);

  // Sets up the stream context and allocate buffers according to the
  // configuration specified in |stream|.
  void SetUpStreamsAndBuffers(
      VideoCaptureFormat capture_format,
      const cros::mojom::CameraMetadataPtr& static_metadata,
      std::vector<cros::mojom::Camera3StreamPtr> streams);

  cros::mojom::Camera3StreamPtr GetStreamConfiguration(StreamType stream_type);

  // Requests buffer for specific stream type. If the |buffer_id| is provided,
  // it will use |buffer_id| as buffer id rather than using id from free
  // buffers.
  base::Optional<BufferInfo> RequestBuffer(StreamType stream_type,
                                           base::Optional<uint64_t> buffer_id);

  // Releases buffer by marking it as free buffer.
  void ReleaseBuffer(StreamType stream_type, uint64_t buffer_id);

  bool IsReprocessSupported();

 private:
  friend class RequestManagerTest;

  static uint64_t GetBufferIpcId(StreamType stream_type, size_t index);

  static size_t GetBufferIndex(uint64_t buffer_id);

  // Destroy current streams and unmap mapped buffers.
  void DestroyCurrentStreamsAndBuffers();

  struct StreamContext {
    StreamContext();
    ~StreamContext();
    // The actual pixel format used in the capture request.
    VideoCaptureFormat capture_format;
    // The camera HAL stream.
    cros::mojom::Camera3StreamPtr stream;
    // The allocated buffers of this stream.
    std::vector<std::unique_ptr<gfx::GpuMemoryBuffer>> buffers;
    // The free buffers of this stream.  The queue stores indices into the
    // |buffers| vector.
    std::queue<uint64_t> free_buffers;
  };

  // The context for the set of active streams.
  std::unordered_map<StreamType, std::unique_ptr<StreamContext>>
      stream_context_;

  CameraDeviceContext* device_context_;

  std::unique_ptr<CameraBufferFactory> camera_buffer_factory_;

  base::WeakPtrFactory<StreamBufferManager> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamBufferManager);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_STREAM_BUFFER_MANAGER_H_
