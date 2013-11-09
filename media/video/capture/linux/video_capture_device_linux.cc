// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/linux/video_capture_device_linux.h"

#include <errno.h>
#include <fcntl.h>
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
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/strings/stringprintf.h"

namespace media {

// Max number of video buffers VideoCaptureDeviceLinux can allocate.
enum { kMaxVideoBuffers = 2 };
// Timeout in microseconds v4l2_thread_ blocks waiting for a frame from the hw.
enum { kCaptureTimeoutUs = 200000 };
// The number of continuous timeouts tolerated before treated as error.
enum { kContinuousTimeoutLimit = 10 };
// Time to wait in milliseconds before v4l2_thread_ reschedules OnCaptureTask
// if an event is triggered (select) but no video frame is read.
enum { kCaptureSelectWaitMs = 10 };
// MJPEG is prefered if the width or height is larger than this.
enum { kMjpegWidth = 640 };
enum { kMjpegHeight = 480 };
// Typical framerate, in fps
enum { kTypicalFramerate = 30 };

// V4L2 color formats VideoCaptureDeviceLinux support.
static const int32 kV4l2RawFmts[] = {
  V4L2_PIX_FMT_YUV420,
  V4L2_PIX_FMT_YUYV
};

// USB VID and PID are both 4 bytes long.
static const size_t kVidPidSize = 4;

// /sys/class/video4linux/video{N}/device is a symlink to the corresponding
// USB device info directory.
static const char kVidPathTemplate[] =
    "/sys/class/video4linux/%s/device/../idVendor";
static const char kPidPathTemplate[] =
    "/sys/class/video4linux/%s/device/../idProduct";

// This function translates Video4Linux pixel formats to Chromium pixel formats,
// should only support those listed in GetListOfUsableFourCCs.
static VideoPixelFormat V4l2ColorToVideoCaptureColorFormat(
    int32 v4l2_fourcc) {
  VideoPixelFormat result = PIXEL_FORMAT_UNKNOWN;
  switch (v4l2_fourcc) {
    case V4L2_PIX_FMT_YUV420:
      result = PIXEL_FORMAT_I420;
      break;
    case V4L2_PIX_FMT_YUYV:
      result = PIXEL_FORMAT_YUY2;
      break;
    case V4L2_PIX_FMT_MJPEG:
    case V4L2_PIX_FMT_JPEG:
      result = PIXEL_FORMAT_MJPEG;
      break;
    default:
      DVLOG(1) << "Unsupported pixel format " << std::hex << v4l2_fourcc;
  }
  return result;
}

static void GetListOfUsableFourCCs(bool favour_mjpeg, std::list<int>* fourccs) {
  for (size_t i = 0; i < arraysize(kV4l2RawFmts); ++i)
    fourccs->push_back(kV4l2RawFmts[i]);
  if (favour_mjpeg)
    fourccs->push_front(V4L2_PIX_FMT_MJPEG);
  else
    fourccs->push_back(V4L2_PIX_FMT_MJPEG);

  // JPEG works as MJPEG on some gspca webcams from field reports.
  // Put it as the least preferred format.
  fourccs->push_back(V4L2_PIX_FMT_JPEG);
}

static bool HasUsableFormats(int fd) {
  v4l2_fmtdesc fmtdesc;
  std::list<int> usable_fourccs;

  GetListOfUsableFourCCs(false, &usable_fourccs);

  memset(&fmtdesc, 0, sizeof(v4l2_fmtdesc));
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
    if (std::find(usable_fourccs.begin(), usable_fourccs.end(),
                  fmtdesc.pixelformat) != usable_fourccs.end())
      return true;

    fmtdesc.index++;
  }
  return false;
}

void VideoCaptureDevice::GetDeviceNames(Names* device_names) {
  int fd = -1;

  // Empty the name list.
  device_names->clear();

  base::FilePath path("/dev/");
  base::FileEnumerator enumerator(
      path, false, base::FileEnumerator::FILES, "video*");

  while (!enumerator.Next().empty()) {
    base::FileEnumerator::FileInfo info = enumerator.GetInfo();

    std::string unique_id = path.value() + info.GetName().value();
    if ((fd = open(unique_id.c_str() , O_RDONLY)) < 0) {
      // Failed to open this device.
      continue;
    }
    // Test if this is a V4L2 capture device.
    v4l2_capability cap;
    if ((ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) &&
        (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
        !(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
      // This is a V4L2 video capture device
      if (HasUsableFormats(fd)) {
        Name device_name(base::StringPrintf("%s", cap.card), unique_id);
        device_names->push_back(device_name);
      } else {
        DVLOG(1) << "No usable formats reported by " << info.GetName().value();
      }
    }
    close(fd);
  }
}

void VideoCaptureDevice::GetDeviceSupportedFormats(
    const Name& device,
    VideoCaptureCapabilities* formats) {

  if (device.id().empty())
    return;
  int fd;
  VideoCaptureCapabilities capture_formats;
  if ((fd = open(device.id().c_str(), O_RDONLY)) < 0) {
    // Failed to open this device.
    return;
  }

  formats->clear();

  VideoCaptureCapability capture_format;
  // Retrieve the caps one by one, first get colorspace, then sizes, then
  // framerates. See http://linuxtv.org/downloads/v4l-dvb-apis for reference.
  v4l2_fmtdesc pixel_format = {};
  pixel_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  while (ioctl(fd, VIDIOC_ENUM_FMT, &pixel_format) == 0) {
    capture_format.color =
        V4l2ColorToVideoCaptureColorFormat((int32)pixel_format.pixelformat);
    if (capture_format.color == PIXEL_FORMAT_UNKNOWN) continue;

    v4l2_frmsizeenum frame_size = {};
    frame_size.pixel_format = pixel_format.pixelformat;
    while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frame_size) == 0) {
      if (frame_size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
        capture_format.width = frame_size.discrete.width;
        capture_format.height = frame_size.discrete.height;
      } else if (frame_size.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
        // TODO(mcasas): see http://crbug.com/249953, support these devices.
        NOTIMPLEMENTED();
      } else if (frame_size.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
        // TODO(mcasas): see http://crbug.com/249953, support these devices.
        NOTIMPLEMENTED();
      }
      v4l2_frmivalenum frame_interval = {};
      frame_interval.pixel_format = pixel_format.pixelformat;
      frame_interval.width = frame_size.discrete.width;
      frame_interval.height = frame_size.discrete.height;
      while (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frame_interval) == 0) {
        if (frame_interval.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
          if (frame_interval.discrete.numerator != 0) {
            capture_format.frame_rate =
                static_cast<float>(frame_interval.discrete.denominator) /
                static_cast<float>(frame_interval.discrete.numerator);
          } else {
            capture_format.frame_rate = 0;
          }
        } else if (frame_interval.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
          // TODO(mcasas): see http://crbug.com/249953, support these devices.
          NOTIMPLEMENTED();
          break;
        } else if (frame_interval.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
          // TODO(mcasas): see http://crbug.com/249953, support these devices.
          NOTIMPLEMENTED();
          break;
        }
        formats->push_back(capture_format);
        ++frame_interval.index;
      }
      ++frame_size.index;
    }
    ++pixel_format.index;
  }

  close(fd);
  return;
}

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

VideoCaptureDevice* VideoCaptureDevice::Create(const Name& device_name) {
  VideoCaptureDeviceLinux* self = new VideoCaptureDeviceLinux(device_name);
  if (!self)
    return NULL;
  // Test opening the device driver. This is to make sure it is available.
  // We will reopen it again in our worker thread when someone
  // allocates the camera.
  int fd = open(device_name.id().c_str(), O_RDONLY);
  if (fd < 0) {
    DVLOG(1) << "Cannot open device";
    delete self;
    return NULL;
  }
  close(fd);

  return self;
}

VideoCaptureDeviceLinux::VideoCaptureDeviceLinux(const Name& device_name)
    : state_(kIdle),
      device_name_(device_name),
      device_fd_(-1),
      v4l2_thread_("V4L2Thread"),
      buffer_pool_(NULL),
      buffer_pool_size_(0),
      timeout_count_(0) {}

VideoCaptureDeviceLinux::~VideoCaptureDeviceLinux() {
  state_ = kIdle;
  // Check if the thread is running.
  // This means that the device have not been DeAllocated properly.
  DCHECK(!v4l2_thread_.IsRunning());

  v4l2_thread_.Stop();
  if (device_fd_ >= 0) {
    close(device_fd_);
  }
}

void VideoCaptureDeviceLinux::AllocateAndStart(
    const VideoCaptureCapability& capture_format,
    scoped_ptr<VideoCaptureDevice::Client> client) {
  if (v4l2_thread_.IsRunning()) {
    return;  // Wrong state.
  }
  v4l2_thread_.Start();
  v4l2_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureDeviceLinux::OnAllocateAndStart,
                 base::Unretained(this),
                 capture_format.width,
                 capture_format.height,
                 capture_format.frame_rate,
                 base::Passed(&client)));
}

void VideoCaptureDeviceLinux::StopAndDeAllocate() {
  if (!v4l2_thread_.IsRunning()) {
    return;  // Wrong state.
  }
  v4l2_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureDeviceLinux::OnStopAndDeAllocate,
                 base::Unretained(this)));
  v4l2_thread_.Stop();
  // Make sure no buffers are still allocated.
  // This can happen (theoretically) if an error occurs when trying to stop
  // the camera.
  DeAllocateVideoBuffers();
}

void VideoCaptureDeviceLinux::OnAllocateAndStart(int width,
                                                 int height,
                                                 int frame_rate,
                                                 scoped_ptr<Client> client) {
  DCHECK_EQ(v4l2_thread_.message_loop(), base::MessageLoop::current());

  client_ = client.Pass();

  // Need to open camera with O_RDWR after Linux kernel 3.3.
  if ((device_fd_ = open(device_name_.id().c_str(), O_RDWR)) < 0) {
    SetErrorState("Failed to open V4L2 device driver.");
    return;
  }

  // Test if this is a V4L2 capture device.
  v4l2_capability cap;
  if (!((ioctl(device_fd_, VIDIOC_QUERYCAP, &cap) == 0) &&
      (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
      !(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT))) {
    // This is not a V4L2 video capture device.
    close(device_fd_);
    device_fd_ = -1;
    SetErrorState("This is not a V4L2 video capture device");
    return;
  }

  // Get supported video formats in preferred order.
  // For large resolutions, favour mjpeg over raw formats.
  std::list<int> v4l2_formats;
  GetListOfUsableFourCCs(width > kMjpegWidth || height > kMjpegHeight,
                         &v4l2_formats);

  v4l2_fmtdesc fmtdesc;
  memset(&fmtdesc, 0, sizeof(v4l2_fmtdesc));
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  // Enumerate image formats.
  std::list<int>::iterator best = v4l2_formats.end();
  while (ioctl(device_fd_, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
    best = std::find(v4l2_formats.begin(), best, fmtdesc.pixelformat);
    fmtdesc.index++;
  }

  if (best == v4l2_formats.end()) {
    SetErrorState("Failed to find a supported camera format.");
    return;
  }

  // Set format and frame size now.
  v4l2_format video_fmt;
  memset(&video_fmt, 0, sizeof(video_fmt));
  video_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  video_fmt.fmt.pix.sizeimage = 0;
  video_fmt.fmt.pix.width = width;
  video_fmt.fmt.pix.height = height;
  video_fmt.fmt.pix.pixelformat = *best;

  if (ioctl(device_fd_, VIDIOC_S_FMT, &video_fmt) < 0) {
    SetErrorState("Failed to set camera format");
    return;
  }

  // Set capture framerate in the form of capture interval.
  v4l2_streamparm streamparm;
  memset(&streamparm, 0, sizeof(v4l2_streamparm));
  streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  // The following line checks that the driver knows about framerate get/set.
  if (ioctl(device_fd_, VIDIOC_G_PARM, &streamparm) >= 0) {
    // Now check if the device is able to accept a capture framerate set.
    if (streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) {
      streamparm.parm.capture.timeperframe.numerator = 1;
      streamparm.parm.capture.timeperframe.denominator =
          (frame_rate) ? frame_rate : kTypicalFramerate;

      if (ioctl(device_fd_, VIDIOC_S_PARM, &streamparm) < 0) {
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

  // Store our current width and height.
  frame_info_.color = V4l2ColorToVideoCaptureColorFormat(
      video_fmt.fmt.pix.pixelformat);
  frame_info_.width  = video_fmt.fmt.pix.width;
  frame_info_.height = video_fmt.fmt.pix.height;
  frame_info_.frame_rate = frame_rate;
  frame_info_.frame_size_type = VariableResolutionVideoCaptureDevice;

  // Start capturing.
  if (!AllocateVideoBuffers()) {
    // Error, We can not recover.
    SetErrorState("Allocate buffer failed");
    return;
  }

  // Start UVC camera.
  v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(device_fd_, VIDIOC_STREAMON, &type) == -1) {
    SetErrorState("VIDIOC_STREAMON failed");
    return;
  }

  state_ = kCapturing;
  // Post task to start fetching frames from v4l2.
  v4l2_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureDeviceLinux::OnCaptureTask,
                 base::Unretained(this)));
}

void VideoCaptureDeviceLinux::OnStopAndDeAllocate() {
  DCHECK_EQ(v4l2_thread_.message_loop(), base::MessageLoop::current());

  v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(device_fd_, VIDIOC_STREAMOFF, &type) < 0) {
    SetErrorState("VIDIOC_STREAMOFF failed");
    return;
  }
  // We don't dare to deallocate the buffers if we can't stop
  // the capture device.
  DeAllocateVideoBuffers();

  // We need to close and open the device if we want to change the settings
  // Otherwise VIDIOC_S_FMT will return error
  // Sad but true.
  close(device_fd_);
  device_fd_ = -1;
  state_ = kIdle;
  client_.reset();
}

void VideoCaptureDeviceLinux::OnCaptureTask() {
  DCHECK_EQ(v4l2_thread_.message_loop(), base::MessageLoop::current());

  if (state_ != kCapturing) {
    return;
  }

  fd_set r_set;
  FD_ZERO(&r_set);
  FD_SET(device_fd_, &r_set);
  timeval timeout;

  timeout.tv_sec = 0;
  timeout.tv_usec = kCaptureTimeoutUs;

  // First argument to select is the highest numbered file descriptor +1.
  // Refer to http://linux.die.net/man/2/select for more information.
  int result = select(device_fd_ + 1, &r_set, NULL, NULL, &timeout);
  // Check if select have failed.
  if (result < 0) {
    // EINTR is a signal. This is not really an error.
    if (errno != EINTR) {
      SetErrorState("Select failed");
      return;
    }
    v4l2_thread_.message_loop()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&VideoCaptureDeviceLinux::OnCaptureTask,
                   base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(kCaptureSelectWaitMs));
  }

  // Check if select timeout.
  if (result == 0) {
    timeout_count_++;
    if (timeout_count_ >= kContinuousTimeoutLimit) {
      SetErrorState(base::StringPrintf(
          "Continuous timeout %d times", timeout_count_));
      timeout_count_ = 0;
      return;
    }
  } else {
    timeout_count_ = 0;
  }

  // Check if the driver have filled a buffer.
  if (FD_ISSET(device_fd_, &r_set)) {
    v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    // Dequeue a buffer.
    if (ioctl(device_fd_, VIDIOC_DQBUF, &buffer) == 0) {
      client_->OnIncomingCapturedFrame(
          static_cast<uint8*> (buffer_pool_[buffer.index].start),
          buffer.bytesused, base::Time::Now(), 0, false, false, frame_info_);

      // Enqueue the buffer again.
      if (ioctl(device_fd_, VIDIOC_QBUF, &buffer) == -1) {
        SetErrorState(base::StringPrintf(
            "Failed to enqueue capture buffer errno %d", errno));
      }
    } else {
      SetErrorState(base::StringPrintf(
          "Failed to dequeue capture buffer errno %d", errno));
      return;
    }
  }

  v4l2_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureDeviceLinux::OnCaptureTask,
                 base::Unretained(this)));
}

bool VideoCaptureDeviceLinux::AllocateVideoBuffers() {
  v4l2_requestbuffers r_buffer;
  memset(&r_buffer, 0, sizeof(r_buffer));

  r_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  r_buffer.memory = V4L2_MEMORY_MMAP;
  r_buffer.count = kMaxVideoBuffers;

  if (ioctl(device_fd_, VIDIOC_REQBUFS, &r_buffer) < 0) {
    return false;
  }

  if (r_buffer.count > kMaxVideoBuffers) {
    r_buffer.count = kMaxVideoBuffers;
  }

  buffer_pool_size_ = r_buffer.count;

  // Map the buffers.
  buffer_pool_ = new Buffer[r_buffer.count];
  for (unsigned int i = 0; i < r_buffer.count; i++) {
    v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = i;

    if (ioctl(device_fd_, VIDIOC_QUERYBUF, &buffer) < 0) {
      return false;
    }

    // Some devices require mmap() to be called with both READ and WRITE.
    // See crbug.com/178582.
    buffer_pool_[i].start = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
                                 MAP_SHARED, device_fd_, buffer.m.offset);
    if (buffer_pool_[i].start == MAP_FAILED) {
      return false;
    }
    buffer_pool_[i].length = buffer.length;
    // Enqueue the buffer in the drivers incoming queue.
    if (ioctl(device_fd_, VIDIOC_QBUF, &buffer) < 0) {
      return false;
    }
  }
  return true;
}

void VideoCaptureDeviceLinux::DeAllocateVideoBuffers() {
  if (!buffer_pool_)
    return;

  // Unmaps buffers.
  for (int i = 0; i < buffer_pool_size_; i++) {
    munmap(buffer_pool_[i].start, buffer_pool_[i].length);
  }
  v4l2_requestbuffers r_buffer;
  memset(&r_buffer, 0, sizeof(r_buffer));
  r_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  r_buffer.memory = V4L2_MEMORY_MMAP;
  r_buffer.count = 0;

  if (ioctl(device_fd_, VIDIOC_REQBUFS, &r_buffer) < 0) {
    SetErrorState("Failed to reset buf.");
  }

  delete [] buffer_pool_;
  buffer_pool_ = NULL;
  buffer_pool_size_ = 0;
}

void VideoCaptureDeviceLinux::SetErrorState(const std::string& reason) {
  DCHECK(!v4l2_thread_.IsRunning() ||
         v4l2_thread_.message_loop() == base::MessageLoop::current());
  DVLOG(1) << reason;
  state_ = kError;
  client_->OnError();
}

}  // namespace media
