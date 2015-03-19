// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/linux/video_capture_device_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#if defined(OS_OPENBSD)
#include <sys/videoio.h>
#else
#include <linux/videodev2.h>
#endif
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <list>
#include <string>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"

namespace media {

#define GET_V4L2_FOURCC_CHAR(a, index) ((char)( ((a) >> (8 * index)) & 0xff))

// Desired number of video buffers to allocate. The actual number of allocated
// buffers by v4l2 driver can be higher or lower than this number.
// kNumVideoBuffers should not be too small, or Chrome may not return enough
// buffers back to driver in time.
const uint32 kNumVideoBuffers = 4;
// Timeout in milliseconds v4l2_thread_ blocks waiting for a frame from the hw.
enum { kCaptureTimeoutMs = 200 };
// The number of continuous timeouts tolerated before treated as error.
enum { kContinuousTimeoutLimit = 10 };
// MJPEG is preferred if the width or height is larger than this.
enum { kMjpegWidth = 640 };
enum { kMjpegHeight = 480 };
// Typical framerate, in fps
enum { kTypicalFramerate = 30 };

class VideoCaptureDeviceLinux::V4L2CaptureDelegate
    : public base::RefCountedThreadSafe<V4L2CaptureDelegate>{
 public:
  V4L2CaptureDelegate(
      const Name& device_name,
      const scoped_refptr<base::SingleThreadTaskRunner>  v4l2_task_runner,
      int power_line_frequency);

  void AllocateAndStart(int width,
                        int height,
                        float frame_rate,
                        scoped_ptr<Client> client);
  void StopAndDeAllocate();
  void SetRotation(int rotation);
  bool DeAllocateVideoBuffers();

 private:
  // Buffers used to receive captured frames from v4l2.
  struct Buffer {
    Buffer() : start(0), length(0) {}
    void* start;
    size_t length;
  };

  friend class base::RefCountedThreadSafe<V4L2CaptureDelegate>;
   ~V4L2CaptureDelegate();

  void DoCapture();
  bool AllocateVideoBuffers();
  void SetErrorState(const std::string& reason);

  const scoped_refptr<base::SingleThreadTaskRunner> v4l2_task_runner_;

  bool is_capturing_;
  scoped_ptr<VideoCaptureDevice::Client> client_;
  const Name device_name_;
  base::ScopedFD device_fd_;  // File descriptor for the opened camera device.
  Buffer* buffer_pool_;
  int buffer_pool_size_;  // Number of allocated buffers.
  int timeout_count_;
  VideoCaptureFormat capture_format_;
  const int power_line_frequency_;

  // Clockwise rotation in degrees.  This value should be 0, 90, 180, or 270.
  int rotation_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(V4L2CaptureDelegate);
};

// V4L2 color formats VideoCaptureDeviceLinux support.
static const int32 kV4l2RawFmts[] = {
  V4L2_PIX_FMT_YUV420,
  V4L2_PIX_FMT_YUYV,
  V4L2_PIX_FMT_UYVY
};

// USB VID and PID are both 4 bytes long.
static const size_t kVidPidSize = 4;

// /sys/class/video4linux/video{N}/device is a symlink to the corresponding
// USB device info directory.
static const char kVidPathTemplate[] =
    "/sys/class/video4linux/%s/device/../idVendor";
static const char kPidPathTemplate[] =
    "/sys/class/video4linux/%s/device/../idProduct";

static bool ReadIdFile(const std::string path, std::string* id) {
  char id_buf[kVidPidSize];
  FILE* file = fopen(path.c_str(), "rb");
  if (!file)
    return false;
  const bool success = fread(id_buf, kVidPidSize, 1, file) == 1;
  fclose(file);
  if (!success)
    return false;
  id->append(id_buf, kVidPidSize);
  return true;
}

// This function translates Video4Linux pixel formats to Chromium pixel formats,
// should only support those listed in GetListOfUsableFourCCs.
// static
VideoPixelFormat VideoCaptureDeviceLinux::V4l2FourCcToChromiumPixelFormat(
    uint32 v4l2_fourcc) {
  const struct {
    uint32 fourcc;
    VideoPixelFormat pixel_format;
  } kFourCcAndChromiumPixelFormat[] = {
    {V4L2_PIX_FMT_YUV420, PIXEL_FORMAT_I420},
    {V4L2_PIX_FMT_YUYV, PIXEL_FORMAT_YUY2},
    {V4L2_PIX_FMT_UYVY, PIXEL_FORMAT_UYVY},
    {V4L2_PIX_FMT_MJPEG, PIXEL_FORMAT_MJPEG},
    {V4L2_PIX_FMT_JPEG, PIXEL_FORMAT_MJPEG},
  };
  for (const auto& fourcc_and_pixel_format : kFourCcAndChromiumPixelFormat) {
    if (fourcc_and_pixel_format.fourcc == v4l2_fourcc)
      return fourcc_and_pixel_format.pixel_format;
  }
  DVLOG(1) << "Unsupported pixel format: "
           << GET_V4L2_FOURCC_CHAR(v4l2_fourcc, 0)
           << GET_V4L2_FOURCC_CHAR(v4l2_fourcc, 1)
           << GET_V4L2_FOURCC_CHAR(v4l2_fourcc, 2)
           << GET_V4L2_FOURCC_CHAR(v4l2_fourcc, 3);
  return PIXEL_FORMAT_UNKNOWN;
}

// static
std::list<int> VideoCaptureDeviceLinux::GetListOfUsableFourCCs(
    bool favour_mjpeg) {
  std::list<int> fourccs;
  for (size_t i = 0; i < arraysize(kV4l2RawFmts); ++i)
    fourccs.push_back(kV4l2RawFmts[i]);
  if (favour_mjpeg)
    fourccs.push_front(V4L2_PIX_FMT_MJPEG);
  else
    fourccs.push_back(V4L2_PIX_FMT_MJPEG);

  // JPEG works as MJPEG on some gspca webcams from field reports.
  // Put it as the least preferred format.
  fourccs.push_back(V4L2_PIX_FMT_JPEG);
  return fourccs;
}

const std::string VideoCaptureDevice::Name::GetModel() const {
  // |unique_id| is of the form "/dev/video2".  |file_name| is "video2".
  const std::string dev_dir = "/dev/";
  DCHECK_EQ(0, unique_id_.compare(0, dev_dir.length(), dev_dir));
  const std::string file_name =
      unique_id_.substr(dev_dir.length(), unique_id_.length());

  const std::string vidPath =
      base::StringPrintf(kVidPathTemplate, file_name.c_str());
  const std::string pidPath =
      base::StringPrintf(kPidPathTemplate, file_name.c_str());

  std::string usb_id;
  if (!ReadIdFile(vidPath, &usb_id))
    return "";
  usb_id.append(":");
  if (!ReadIdFile(pidPath, &usb_id))
    return "";

  return usb_id;
}

VideoCaptureDeviceLinux::VideoCaptureDeviceLinux(const Name& device_name)
    : v4l2_thread_("V4L2CaptureThread"),
      device_name_(device_name) {
}

VideoCaptureDeviceLinux::~VideoCaptureDeviceLinux() {
  // Check if the thread is running.
  // This means that the device has not been StopAndDeAllocate()d properly.
  DCHECK(!v4l2_thread_.IsRunning());
  v4l2_thread_.Stop();
}

void VideoCaptureDeviceLinux::AllocateAndStart(
    const VideoCaptureParams& params,
    scoped_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(!capture_impl_);
  if (v4l2_thread_.IsRunning())
    return;  // Wrong state.
  v4l2_thread_.Start();
  capture_impl_ = new V4L2CaptureDelegate(device_name_,
                                          v4l2_thread_.message_loop_proxy(),
                                          GetPowerLineFrequencyForLocation());
  v4l2_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(
          &VideoCaptureDeviceLinux::V4L2CaptureDelegate::AllocateAndStart,
          capture_impl_,
          params.requested_format.frame_size.width(),
          params.requested_format.frame_size.height(),
          params.requested_format.frame_rate,
          base::Passed(&client)));
}

void VideoCaptureDeviceLinux::StopAndDeAllocate() {
  if (!v4l2_thread_.IsRunning())
    return;  // Wrong state.
  v4l2_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(
          &VideoCaptureDeviceLinux::V4L2CaptureDelegate::StopAndDeAllocate,
          capture_impl_));
  v4l2_thread_.Stop();
  // TODO(mcasas): VCDLinux called DeAllocateVideoBuffers() a second time after
  // stopping |v4l2_thread_| to make sure buffers were completely deallocated.
  // Investigate if that's needed, otherwise remove the following line and make
  // V4L2CaptureDelegate::DeAllocateVideoBuffers() private.
  capture_impl_->DeAllocateVideoBuffers();
  capture_impl_ = NULL;
}

void VideoCaptureDeviceLinux::SetRotation(int rotation) {
  if (v4l2_thread_.IsRunning()) {
    v4l2_thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(
            &VideoCaptureDeviceLinux::V4L2CaptureDelegate::SetRotation,
            capture_impl_,
            rotation));
  }
}

VideoCaptureDeviceLinux::V4L2CaptureDelegate::V4L2CaptureDelegate(
        const Name& device_name,
        const scoped_refptr<base::SingleThreadTaskRunner> v4l2_task_runner,
        int power_line_frequency)
    : v4l2_task_runner_(v4l2_task_runner),
      is_capturing_(false),
      device_name_(device_name),
      buffer_pool_(NULL),
      buffer_pool_size_(0),
      timeout_count_(0),
      power_line_frequency_(power_line_frequency),
      rotation_(0) {
}

VideoCaptureDeviceLinux::V4L2CaptureDelegate::~V4L2CaptureDelegate() {
  DCHECK(!client_);
}

void VideoCaptureDeviceLinux::V4L2CaptureDelegate::AllocateAndStart(
    int width,
    int height,
    float frame_rate,
    scoped_ptr<Client> client) {
  DCHECK(v4l2_task_runner_->BelongsToCurrentThread());
  DCHECK(client);
  client_ = client.Pass();

  // Need to open camera with O_RDWR after Linux kernel 3.3.
  device_fd_.reset(HANDLE_EINTR(open(device_name_.id().c_str(), O_RDWR)));
  if (!device_fd_.is_valid()) {
    SetErrorState("Failed to open V4L2 device driver file.");
    return;
  }

  v4l2_capability cap = {};
  if (!((HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_QUERYCAP, &cap)) == 0) &&
      (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE &&
      !(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)))) {
    device_fd_.reset();
    SetErrorState("This is not a V4L2 video capture device");
    return;
  }

  // Get supported video formats in preferred order.
  // For large resolutions, favour mjpeg over raw formats.
  const std::list<int>& desired_v4l2_formats =
      GetListOfUsableFourCCs(width > kMjpegWidth || height > kMjpegHeight);
  std::list<int>::const_iterator best = desired_v4l2_formats.end();

  v4l2_fmtdesc fmtdesc = {};
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  for (; HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_ENUM_FMT, &fmtdesc)) == 0;
       ++fmtdesc.index) {
    best = std::find(desired_v4l2_formats.begin(), best, fmtdesc.pixelformat);
  }
  if (best == desired_v4l2_formats.end()) {
    SetErrorState("Failed to find a supported camera format.");
    return;
  }

  v4l2_format video_fmt = {};
  video_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  video_fmt.fmt.pix.sizeimage = 0;
  video_fmt.fmt.pix.width = width;
  video_fmt.fmt.pix.height = height;
  video_fmt.fmt.pix.pixelformat = *best;
  if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_S_FMT, &video_fmt)) < 0) {
    SetErrorState("Failed to set video capture format");
    return;
  }

  // Set capture framerate in the form of capture interval.
  v4l2_streamparm streamparm = {};
  streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  // The following line checks that the driver knows about framerate get/set.
  if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_G_PARM, &streamparm)) >= 0) {
    // Now check if the device is able to accept a capture framerate set.
    if (streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) {
      // |frame_rate| is float, approximate by a fraction.
      streamparm.parm.capture.timeperframe.numerator =
          media::kFrameRatePrecision;
      streamparm.parm.capture.timeperframe.denominator = (frame_rate) ?
          (frame_rate * media::kFrameRatePrecision) :
          (kTypicalFramerate * media::kFrameRatePrecision);

      if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_S_PARM, &streamparm)) <
          0) {
        SetErrorState("Failed to set camera framerate");
        return;
      }
      DVLOG(2) << "Actual camera driverframerate: "
               << streamparm.parm.capture.timeperframe.denominator << "/"
               << streamparm.parm.capture.timeperframe.numerator;
    }
  }
  // TODO(mcasas): what should be done if the camera driver does not allow
  // framerate configuration, or the actual one is different from the desired?

  // Set anti-banding/anti-flicker to 50/60Hz. May fail due to not supported
  // operation (|errno| == EINVAL in this case) or plain failure.
  if ((power_line_frequency_ == kPowerLine50Hz) ||
      (power_line_frequency_ == kPowerLine60Hz)) {
    struct v4l2_control control = {};
    control.id = V4L2_CID_POWER_LINE_FREQUENCY;
    control.value = (power_line_frequency_ == kPowerLine50Hz)
                        ? V4L2_CID_POWER_LINE_FREQUENCY_50HZ
                        : V4L2_CID_POWER_LINE_FREQUENCY_60HZ;
    HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_S_CTRL, &control));
  }

  capture_format_.frame_size.SetSize(video_fmt.fmt.pix.width,
                                     video_fmt.fmt.pix.height);
  capture_format_.frame_rate = frame_rate;
  capture_format_.pixel_format =
      V4l2FourCcToChromiumPixelFormat(video_fmt.fmt.pix.pixelformat);

  if (!AllocateVideoBuffers()) {
    SetErrorState("Allocate buffer failed (Cannot recover from this error)");
    return;
  }

  const v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_STREAMON, &type)) < 0) {
    SetErrorState("VIDIOC_STREAMON failed");
    return;
  }

  is_capturing_ = true;
  // Post task to start fetching frames from v4l2.
  v4l2_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureDeviceLinux::V4L2CaptureDelegate::DoCapture,
                 this));
}

void VideoCaptureDeviceLinux::V4L2CaptureDelegate::StopAndDeAllocate() {
  DCHECK(v4l2_task_runner_->BelongsToCurrentThread());

  const v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_STREAMOFF, &type)) < 0) {
    SetErrorState("VIDIOC_STREAMOFF failed");
    return;
  }
  // We don't dare to deallocate the buffers if we can't stop the capture
  // device.
  if (!DeAllocateVideoBuffers())
    SetErrorState("Failed to reset buffers");

  // We need to close and open the device if we want to change the settings.
  // Otherwise VIDIOC_S_FMT will return error. Sad but true.
  device_fd_.reset();
  is_capturing_ = false;
  client_.reset();
}

void VideoCaptureDeviceLinux::V4L2CaptureDelegate::SetRotation(int rotation) {
  DCHECK(v4l2_task_runner_->BelongsToCurrentThread());
  DCHECK(rotation >= 0 && rotation < 360 && rotation % 90 == 0);
  rotation_ = rotation;
}

void VideoCaptureDeviceLinux::V4L2CaptureDelegate::DoCapture() {
  DCHECK(v4l2_task_runner_->BelongsToCurrentThread());
  if (!is_capturing_)
    return;

  pollfd device_pfd = {};
  device_pfd.fd = device_fd_.get();
  device_pfd.events = POLLIN;
  const int result = HANDLE_EINTR(poll(&device_pfd, 1, kCaptureTimeoutMs));
  if (result < 0) {
    SetErrorState("Poll failed");
    return;
  }
  // Check if poll() timed out; track the amount of times it did in a row and
  // throw an error if it times out too many times.
  if (result == 0) {
    timeout_count_++;
    if (timeout_count_ >= kContinuousTimeoutLimit) {
      SetErrorState("Multiple continuous timeouts while read-polling.");
      timeout_count_ = 0;
      return;
    }
  } else {
    timeout_count_ = 0;
  }

  // Check if the driver has filled a buffer.
  if (device_pfd.revents & POLLIN) {
    v4l2_buffer buffer = {};
     buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     buffer.memory = V4L2_MEMORY_MMAP;
    if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_DQBUF, &buffer)) < 0) {
      SetErrorState("Failed to dequeue capture buffer");
      return;
    }
    client_->OnIncomingCapturedData(
        static_cast<uint8*>(buffer_pool_[buffer.index].start),
        buffer.bytesused,
        capture_format_,
        rotation_,
        base::TimeTicks::Now());

    if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_QBUF, &buffer)) < 0)
      SetErrorState("Failed to enqueue capture buffer");
  }

  v4l2_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureDeviceLinux::V4L2CaptureDelegate::DoCapture,
                 this));
}

bool VideoCaptureDeviceLinux::V4L2CaptureDelegate::AllocateVideoBuffers() {
  v4l2_requestbuffers r_buffer = {};
  r_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  r_buffer.memory = V4L2_MEMORY_MMAP;
  r_buffer.count = kNumVideoBuffers;
  if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_REQBUFS, &r_buffer)) < 0) {
    DLOG(ERROR) << "Error requesting MMAP buffers from V4L2";
    return false;
  }
  buffer_pool_size_ = r_buffer.count;
  buffer_pool_ = new Buffer[buffer_pool_size_];
  for (unsigned int i = 0; i < r_buffer.count; ++i) {
    v4l2_buffer buffer = {};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = i;
    buffer.length = 1;
    if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_QUERYBUF, &buffer)) < 0) {
      DLOG(ERROR) << "Error querying status of a MMAP V4L2 buffer";
      return false;
    }

    // Some devices require mmap() to be called with both READ and WRITE.
    // See http://crbug.com/178582.
    buffer_pool_[i].start = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
        MAP_SHARED, device_fd_.get(), buffer.m.offset);
    if (buffer_pool_[i].start == MAP_FAILED) {
      DLOG(ERROR) << "Error mmmap()ing a V4L2 buffer into userspace";
      return false;
    }

    buffer_pool_[i].length = buffer.length;
    // Enqueue the buffer in the drivers incoming queue.
    if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_QBUF, &buffer)) < 0) {
      DLOG(ERROR)
          << "Error enqueuing a V4L2 buffer back to the drivers incoming queue";
      return false;
    }
  }
  return true;
}

bool VideoCaptureDeviceLinux::V4L2CaptureDelegate::DeAllocateVideoBuffers() {
  if (!buffer_pool_)
    return true;

  for (int i = 0; i < buffer_pool_size_; ++i)
    munmap(buffer_pool_[i].start, buffer_pool_[i].length);

  v4l2_requestbuffers r_buffer = {};
  r_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  r_buffer.memory = V4L2_MEMORY_MMAP;
  r_buffer.count = 0;
  if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_REQBUFS, &r_buffer)) < 0)
    return false;

  delete [] buffer_pool_;
  buffer_pool_ = NULL;
  buffer_pool_size_ = 0;
  return true;
}

void VideoCaptureDeviceLinux::V4L2CaptureDelegate::SetErrorState(
    const std::string& reason) {
  DCHECK(v4l2_task_runner_->BelongsToCurrentThread());
  is_capturing_ = false;
  client_->OnError(reason);
}

}  // namespace media
