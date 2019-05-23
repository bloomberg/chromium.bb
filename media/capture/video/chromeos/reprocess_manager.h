// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_REPROCESS_MANAGER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_REPROCESS_MANAGER_H_

#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "media/capture/capture_export.h"
#include "media/capture/mojom/image_capture.mojom.h"
#include "media/capture/video/chromeos/mojo/camera3.mojom.h"
#include "media/capture/video/chromeos/mojo/cros_image_capture.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace media {

struct ReprocessTask {
 public:
  ReprocessTask();
  ReprocessTask(ReprocessTask&& other);
  ~ReprocessTask();
  cros::mojom::Effect effect;
  cros::mojom::CrosImageCapture::SetReprocessOptionCallback callback;
  std::vector<cros::mojom::CameraMetadataEntryPtr> extra_metadata;
};

using ReprocessTaskQueue = base::queue<ReprocessTask>;

// TODO(shik): Get the keys from VendorTagOps by names instead (b/130774415).
constexpr uint32_t kPortraitModeVendorKey = 0x80000000;
constexpr uint32_t kPortraitModeSegmentationResultVendorKey = 0x80000001;
constexpr int32_t kReprocessSuccess = 0;

// ReprocessManager is used to communicate between the reprocess requester and
// the consumer. When reprocess is requested, the reprocess information will be
// wrapped as a ReprocessTask and stored in the queue. When consumption, all
// ReprocessTask in the queue will be dumped and a default NO_EFFECT task will
// be added on the top of the result queue. Note that all calls will be
// sequentialize to a single sequence.
class CAPTURE_EXPORT ReprocessManager {
 public:
  using GetSupportedEffectsCallback =
      base::OnceCallback<void(base::flat_set<cros::mojom::Effect>)>;

  class ReprocessManagerImpl {
   public:
    ReprocessManagerImpl();
    ~ReprocessManagerImpl();

    void SetReprocessOption(
        const std::string& device_id,
        cros::mojom::Effect effect,
        cros::mojom::CrosImageCapture::SetReprocessOptionCallback
            reprocess_result_callback);

    void ConsumeReprocessOptions(
        const std::string& device_id,
        media::mojom::ImageCapture::TakePhotoCallback take_photo_callback,
        base::OnceCallback<void(ReprocessTaskQueue)> consumption_callback);

    void FlushReprocessOptions(const std::string& device_id);

    void GetSupportedEffects(const std::string& device_id,
                             GetSupportedEffectsCallback callback);

    void UpdateSupportedEffects(const std::string& device_id,
                                const cros::mojom::CameraMetadataPtr metadata);

   private:
    base::flat_map<std::string, base::queue<ReprocessTask>>
        reprocess_task_queue_map_;
    base::flat_map<std::string, base::flat_set<cros::mojom::Effect>>
        supported_effects_map_;

    DISALLOW_COPY_AND_ASSIGN(ReprocessManagerImpl);
  };

  static int GetReprocessReturnCode(
      cros::mojom::Effect effect,
      const cros::mojom::CameraMetadataPtr* metadata);
  ReprocessManager();
  ~ReprocessManager();

  // Sets the reprocess option for given device id and effect. Each reprocess
  // option has a corressponding callback.
  void SetReprocessOption(
      const std::string& device_id,
      cros::mojom::Effect effect,
      cros::mojom::CrosImageCapture::SetReprocessOptionCallback
          reprocess_result_callback);

  // Consumes all ReprocessTasks in the queue. A default NO_EFFECT task will be
  // added on the top of the result queue.
  void ConsumeReprocessOptions(
      const std::string& device_id,
      media::mojom::ImageCapture::TakePhotoCallback take_photo_callback,
      base::OnceCallback<void(ReprocessTaskQueue)> consumption_callback);

  // Clears all remaining ReprocessTasks in the queue for given device id.
  void FlushReprocessOptions(const std::string& device_id);

  // Gets supported effects for current active device.
  void GetSupportedEffects(const std::string& device_id,
                           GetSupportedEffectsCallback callback);

  // Updates supported effects for given device. This method should be triggered
  // whenever the camera characteristics is updated.
  void UpdateSupportedEffects(const std::string& device_id,
                              const cros::mojom::CameraMetadataPtr& metadata);

 private:
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  std::unique_ptr<ReprocessManagerImpl> impl;

  DISALLOW_COPY_AND_ASSIGN(ReprocessManager);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_REPROCESS_MANAGER_H_
