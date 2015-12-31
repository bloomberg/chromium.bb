// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_LINUX_V4L2_VIDEO_CAPTURE_DELEGATE_H_
#define MEDIA_VIDEO_CAPTURE_LINUX_V4L2_VIDEO_CAPTURE_DELEGATE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "media/capture/video/video_capture_device.h"

#if defined(OS_OPENBSD)
#include <sys/videoio.h>
#else
#include <linux/videodev2.h>
#endif

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace media {

// Class doing the actual Linux capture using V4L2 API. V4L2 SPLANE/MPLANE
// capture specifics are implemented in derived classes. Created and destroyed
// on the owner's thread, otherwise living and operating on |v4l2_task_runner_|.
class V4L2CaptureDelegate
    : public base::RefCountedThreadSafe<V4L2CaptureDelegate> {
 public:
  // Creates the appropiate VideoCaptureDelegate according to parameters.
  static scoped_refptr<V4L2CaptureDelegate> CreateV4L2CaptureDelegate(
      const VideoCaptureDevice::Name& device_name,
      const scoped_refptr<base::SingleThreadTaskRunner>& v4l2_task_runner,
      int power_line_frequency);

  // Retrieves the #planes for a given |fourcc|, or 0 if unknown.
  static size_t GetNumPlanesForFourCc(uint32_t fourcc);
  // Returns the Chrome pixel format for |v4l2_fourcc| or PIXEL_FORMAT_UNKNOWN.
  static VideoPixelFormat V4l2FourCcToChromiumPixelFormat(
      uint32_t v4l2_fourcc);

  // Composes a list of usable and supported pixel formats, in order of
  // preference, with MJPEG prioritised depending on |prefer_mjpeg|.
  static std::list<uint32_t> GetListOfUsableFourCcs(bool prefer_mjpeg);

  // Forward-to versions of VideoCaptureDevice virtual methods.
  void AllocateAndStart(int width,
                        int height,
                        float frame_rate,
                        scoped_ptr<VideoCaptureDevice::Client> client);
  void StopAndDeAllocate();

  void SetRotation(int rotation);

 protected:
  // Class keeping track of SPLANE/MPLANE V4L2 buffers, mmap()ed on construction
  // and munmap()ed on destruction. Destruction is syntactically equal for
  // S/MPLANE but not construction, so this is implemented in derived classes.
  // Internally it has a vector of planes, which for SPLANE will contain only
  // one element.
  class BufferTracker : public base::RefCounted<BufferTracker> {
   public:
    BufferTracker();
    // Abstract method to mmap() given |fd| according to |buffer|, planarity
    // specific.
    virtual bool Init(int fd, const v4l2_buffer& buffer) = 0;

    const uint8_t* GetPlaneStart(size_t plane) const {
      DCHECK_LT(plane, planes_.size());
      return planes_[plane].start;
    }

    size_t GetPlanePayloadSize(size_t plane) const {
      DCHECK_LT(plane, planes_.size());
      return planes_[plane].payload_size;
    }

    void SetPlanePayloadSize(size_t plane, size_t payload_size) {
      DCHECK_LT(plane, planes_.size());
      DCHECK_LE(payload_size, planes_[plane].length);
      planes_[plane].payload_size = payload_size;
    }

   protected:
    friend class base::RefCounted<BufferTracker>;
    virtual ~BufferTracker();
    // Adds a given mmap()ed plane to |planes_|.
    void AddMmapedPlane(uint8_t* const start, size_t length);

   private:
    struct Plane {
      uint8_t* start;
      size_t length;
      size_t payload_size;
    };
    std::vector<Plane> planes_;
  };

  V4L2CaptureDelegate(
      const VideoCaptureDevice::Name& device_name,
      const scoped_refptr<base::SingleThreadTaskRunner>& v4l2_task_runner,
      int power_line_frequency);
  virtual ~V4L2CaptureDelegate();

  // Creates the necessary, planarity-specific, internal tracking schemes,
  virtual scoped_refptr<BufferTracker> CreateBufferTracker() const = 0;

  // Fill in |format| with the given parameters, in a planarity dependent way.
  virtual bool FillV4L2Format(v4l2_format* format,
                              uint32_t width,
                              uint32_t height,
                              uint32_t pixelformat_fourcc) const = 0;

  // Finish filling |buffer| struct with planarity-dependent data.
  virtual void FinishFillingV4L2Buffer(v4l2_buffer* buffer) const = 0;

  // Fetch the number of bytes occupied by data in |buffer| and set to
  // |buffer_tracker|.
  virtual void SetPayloadSize(
      const scoped_refptr<BufferTracker>& buffer_tracker,
      const v4l2_buffer& buffer) const = 0;

  // Sends the captured |buffer| to the |client_|, synchronously.
  virtual void SendBuffer(const scoped_refptr<BufferTracker>& buffer_tracker,
                          const v4l2_format& format) const = 0;

  // A few accessors for SendBuffer()'s to access private member variables.
  VideoCaptureFormat capture_format() const { return capture_format_; }
  VideoCaptureDevice::Client* client() const { return client_.get(); }
  int rotation() const { return rotation_; }

 private:
  friend class base::RefCountedThreadSafe<V4L2CaptureDelegate>;

  // Returns the input |fourcc| as a std::string four char representation.
  static std::string FourccToString(uint32_t fourcc);
  // VIDIOC_QUERYBUFs a buffer from V4L2, creates a BufferTracker for it and
  // enqueues it (VIDIOC_QBUF) back into V4L2.
  bool MapAndQueueBuffer(int index);
  // Fills all common parts of |buffer|. Delegates to FinishFillingV4L2Buffer()
  // for filling in the planar-dependent parts.
  void FillV4L2Buffer(v4l2_buffer* buffer, int i) const;
  void DoCapture();
  void SetErrorState(const tracked_objects::Location& from_here,
                     const std::string& reason);

  const v4l2_buf_type capture_type_;
  const scoped_refptr<base::SingleThreadTaskRunner> v4l2_task_runner_;
  const VideoCaptureDevice::Name device_name_;
  const int power_line_frequency_;

  // The following members are only known on AllocateAndStart().
  VideoCaptureFormat capture_format_;
  v4l2_format video_fmt_;
  scoped_ptr<VideoCaptureDevice::Client> client_;
  base::ScopedFD device_fd_;

  // Vector of BufferTracker to keep track of mmap()ed pointers and their use.
  std::vector<scoped_refptr<BufferTracker>> buffer_tracker_pool_;

  bool is_capturing_;
  int timeout_count_;

  // Clockwise rotation in degrees. This value should be 0, 90, 180, or 270.
  int rotation_;

  DISALLOW_COPY_AND_ASSIGN(V4L2CaptureDelegate);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_LINUX_V4L2_VIDEO_CAPTURE_DELEGATE_H_
