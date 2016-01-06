// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/cma/backend/alsa/media_pipeline_backend_alsa.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/media_codec_support_shlib.h"
#include "chromecast/public/video_plane.h"
#include "media/base/media.h"

namespace chromecast {
namespace media {
namespace {

const char kPepperoniAlsaDevName[] = "/dev/snd/hwC0D0";

// 1 MHz reference allows easy translation between frequency and PPM.
const double kOneMhzReference = 1e6;
const double kMaxAdjustmentHz = 500;
const double kGranularityHz = 1.0;

enum {
  SNDRV_BERLIN_GET_CLOCK_PPM = 0x1003,
  SNDRV_BERLIN_SET_CLOCK_PPM,
};

class DefaultVideoPlane : public VideoPlane {
 public:
  ~DefaultVideoPlane() override {}

  void SetGeometry(const RectF& display_rect,
                   Transform transform) override {}
};

DefaultVideoPlane* g_video_plane = nullptr;

base::AtExitManager g_at_exit_manager;

scoped_ptr<base::ThreadTaskRunnerHandle> g_thread_task_runner_handle;

}  // namespace

void CastMediaShlib::Initialize(const std::vector<std::string>& argv) {
  base::CommandLine::Init(0, nullptr);
  base::CommandLine::ForCurrentProcess()->InitFromArgv(argv);

  g_video_plane = new DefaultVideoPlane();

  ::media::InitializeMediaLibrary();
}

void CastMediaShlib::Finalize() {
  base::CommandLine::Reset();

  delete g_video_plane;
  g_video_plane = nullptr;

  g_thread_task_runner_handle.reset();
}

VideoPlane* CastMediaShlib::GetVideoPlane() {
  return g_video_plane;
}

MediaPipelineBackend* CastMediaShlib::CreateMediaPipelineBackend(
    const MediaPipelineDeviceParams& params) {
  // Set up the static reference in base::ThreadTaskRunnerHandle::Get
  // for the media thread in this shared library.  We can extract the
  // SingleThreadTaskRunner passed in from cast_shell for this.
  if (!base::ThreadTaskRunnerHandle::IsSet()) {
    DCHECK(!g_thread_task_runner_handle);
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        static_cast<TaskRunnerImpl*>(params.task_runner)->runner();
    DCHECK(task_runner->BelongsToCurrentThread());
    g_thread_task_runner_handle.reset(
        new base::ThreadTaskRunnerHandle(task_runner));
  }

  // TODO(cleichner): Implement MediaSyncType in MediaPipelineDeviceAlsa
  return new MediaPipelineBackendAlsa(params);
}

double CastMediaShlib::GetMediaClockRate() {
  base::ScopedFD alsa_device(
      TEMP_FAILURE_RETRY(open(kPepperoniAlsaDevName, O_RDONLY)));
  DCHECK(alsa_device.is_valid()) << "Failed to open ALSA device";
  double ppm;
  int ret = ioctl(alsa_device.get(), SNDRV_BERLIN_GET_CLOCK_PPM, &ppm);
  DCHECK(ret != -1) << "ioctl(SNDRV_BERLIN_GET_CLOCK_PPM) failed: "
                    << strerror(errno);
  return kOneMhzReference + ppm;
}

double CastMediaShlib::MediaClockRatePrecision() {
  return kGranularityHz;
}

void CastMediaShlib::MediaClockRateRange(double* minimum_rate,
                                         double* maximum_rate) {
  DCHECK(minimum_rate);
  DCHECK(maximum_rate);

  *minimum_rate = kOneMhzReference - kMaxAdjustmentHz;
  *maximum_rate = kOneMhzReference + kMaxAdjustmentHz;
}

bool CastMediaShlib::SetMediaClockRate(double new_rate) {
  new_rate -= kOneMhzReference;
  base::ScopedFD alsa_device(
      TEMP_FAILURE_RETRY(open(kPepperoniAlsaDevName, O_WRONLY)));
  DCHECK(alsa_device.is_valid()) << "Failed to open ALSA device";

  int ret = ioctl(alsa_device.get(), SNDRV_BERLIN_SET_CLOCK_PPM, &new_rate);
  if (ret) {
    LOG(ERROR) << "ioctl(SNDRV_BERLIN_SET_CLOCK_PPM) failed: "
               << strerror(errno);
    return false;
  }
  return true;
}

bool CastMediaShlib::SupportsMediaClockRateChange() {
  // Simply test the 'Get PPM' IOCTL to determine support.
  base::ScopedFD alsa_device(
      TEMP_FAILURE_RETRY(open(kPepperoniAlsaDevName, O_RDONLY)));
  if (!alsa_device.is_valid())
    return false;

  double ppm;
  int ret = ioctl(alsa_device.get(), SNDRV_BERLIN_GET_CLOCK_PPM, &ppm);
  if (ret == -1)
    return false;

  return true;
}

}  // namespace media
}  // namespace chromecast
