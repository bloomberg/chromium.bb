// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_devices_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system.h"
#include "media/base/media_switches.h"

#if defined(OS_MACOSX)
#include "base/bind_helpers.h"
#include "base/single_thread_task_runner.h"
#include "content/browser/browser_main_loop.h"
#include "media/device_monitors/device_monitor_mac.h"
#endif

namespace content {

namespace {

// Resolutions used if the source doesn't support capability enumeration.
struct {
  uint16_t width;
  uint16_t height;
} const kFallbackVideoResolutions[] = {{1920, 1080}, {1280, 720}, {960, 720},
                                       {640, 480},   {640, 360},  {320, 240},
                                       {320, 180}};

// Frame rates for sources with no support for capability enumeration.
const uint16_t kFallbackVideoFrameRates[] = {30, 60};

// Private helper method to generate a string for the log message that lists the
// human readable names of |devices|.
std::string GetLogMessageString(MediaDeviceType device_type,
                                const MediaDeviceInfoArray& device_infos) {
  std::string output_string =
      base::StringPrintf("Getting devices of type %d:\n", device_type);
  if (device_infos.empty())
    return output_string + "No devices found.";
  for (const auto& device_info : device_infos)
    output_string += "  " + device_info.label + "\n";
  return output_string;
}

MediaDeviceInfoArray GetFakeAudioDevices(bool is_input) {
  MediaDeviceInfoArray result;
  if (is_input) {
    result.emplace_back(media::AudioDeviceDescription::kDefaultDeviceId,
                        "Fake Default Audio Input",
                        "fake_group_audio_input_default");
    result.emplace_back("fake_audio_input_1", "Fake Audio Input 1",
                        "fake_group_audio_input_1");
    result.emplace_back("fake_audio_input_2", "Fake Audio Input 2",
                        "fake_group_audio_input_2");
  } else {
    result.emplace_back(media::AudioDeviceDescription::kDefaultDeviceId,
                        "Fake Default Audio Output",
                        "fake_group_audio_output_default");
    result.emplace_back("fake_audio_output_1", "Fake Audio Output 1",
                        "fake_group_audio_output_1");
    result.emplace_back("fake_audio_output_2", "Fake Audio Output 2",
                        "fake_group_audio_output_2");
  }

  return result;
}

std::string VideoLabelWithoutModelID(const std::string& label) {
  if (label.rfind(")") != label.size() - 1)
    return label;

  auto idx = label.rfind(" (");
  if (idx == std::string::npos)
    return label;

  return label.substr(0, idx - 1);
}

}  // namespace

std::string GuessVideoGroupID(const MediaDeviceInfoArray& audio_infos,
                              const MediaDeviceInfo& video_info) {
  std::string video_label = VideoLabelWithoutModelID(video_info.label);

  // If |video_label| is very small, do not guess in order to avoid false
  // positives.
  if (video_label.size() <= 3)
    return video_info.device_id;

  auto equals_lambda = [&video_label](const MediaDeviceInfo& info) {
    return info.label.find(video_label) != std::string::npos;
  };

  auto it_first =
      std::find_if(audio_infos.begin(), audio_infos.end(), equals_lambda);
  if (it_first == audio_infos.end())
    return video_info.device_id;

  auto it = it_first;
  while ((it = std::find_if(it + 1, audio_infos.end(), equals_lambda)) !=
         audio_infos.end()) {
    // If the label appears with more than one group ID, it is impossible to
    // know which group ID is the correct one.
    if (it->group_id != it_first->group_id)
      return video_info.device_id;
  }

  return it_first->group_id;
}

struct MediaDevicesManager::EnumerationRequest {
  EnumerationRequest(const BoolDeviceTypes& requested_types,
                     const EnumerationCallback& callback)
      : callback(callback) {
    requested = requested_types;
    has_seen_result.fill(false);
  }

  BoolDeviceTypes requested;
  BoolDeviceTypes has_seen_result;
  EnumerationCallback callback;
};

// This class helps manage the consistency of cached enumeration results.
// It uses a sequence number for each invalidation and enumeration.
// A cache is considered valid if the sequence number for the last enumeration
// is greater than the sequence number for the last invalidation.
// The advantage of using invalidations over directly issuing enumerations upon
// each system notification is that some platforms issue multiple notifications
// on each device change. The cost of performing multiple redundant
// invalidations is significantly lower than the cost of issuing multiple
// redundant enumerations.
class MediaDevicesManager::CacheInfo {
 public:
  CacheInfo()
      : current_event_sequence_(0),
        seq_last_update_(0),
        seq_last_invalidation_(0),
        is_update_ongoing_(false) {}

  void InvalidateCache() {
    DCHECK(thread_checker_.CalledOnValidThread());
    seq_last_invalidation_ = NewEventSequence();
  }

  bool IsLastUpdateValid() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return seq_last_update_ > seq_last_invalidation_ && !is_update_ongoing_;
  }

  void UpdateStarted() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!is_update_ongoing_);
    seq_last_update_ = NewEventSequence();
    is_update_ongoing_ = true;
  }

  void UpdateCompleted() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(is_update_ongoing_);
    is_update_ongoing_ = false;
  }

  bool is_update_ongoing() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return is_update_ongoing_;
  }

 private:
  int64_t NewEventSequence() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return ++current_event_sequence_;
  }

  int64_t current_event_sequence_;
  int64_t seq_last_update_;
  int64_t seq_last_invalidation_;
  bool is_update_ongoing_;
  base::ThreadChecker thread_checker_;
};

MediaDevicesManager::SubscriptionRequest::SubscriptionRequest(
    int render_process_id,
    int render_frame_id,
    const std::string& group_id_salt_base,
    const BoolDeviceTypes& subscribe_types,
    blink::mojom::MediaDevicesListenerPtr listener)
    : render_process_id(render_process_id),
      render_frame_id(render_frame_id),
      group_id_salt_base(group_id_salt_base),
      subscribe_types(subscribe_types),
      listener(std::move(listener)) {}

MediaDevicesManager::SubscriptionRequest::SubscriptionRequest(
    SubscriptionRequest&&) = default;

MediaDevicesManager::SubscriptionRequest::~SubscriptionRequest() = default;

MediaDevicesManager::SubscriptionRequest&
MediaDevicesManager::SubscriptionRequest::operator=(SubscriptionRequest&&) =
    default;

MediaDevicesManager::MediaDevicesManager(
    media::AudioSystem* audio_system,
    const scoped_refptr<VideoCaptureManager>& video_capture_manager,
    MediaStreamManager* media_stream_manager)
    : use_fake_devices_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)),
      audio_system_(audio_system),
      video_capture_manager_(video_capture_manager),
      media_stream_manager_(media_stream_manager),
      permission_checker_(std::make_unique<MediaDevicesPermissionChecker>()),
      cache_infos_(NUM_MEDIA_DEVICE_TYPES),
      monitoring_started_(false),
      salt_and_origin_callback_(
          base::BindRepeating(&GetMediaDeviceSaltAndOrigin)),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(audio_system_);
  DCHECK(video_capture_manager_.get());
  cache_policies_.fill(CachePolicy::NO_CACHE);
  has_seen_result_.fill(false);
}

MediaDevicesManager::~MediaDevicesManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void MediaDevicesManager::EnumerateDevices(
    const BoolDeviceTypes& requested_types,
    const EnumerationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  StartMonitoring();

  requests_.emplace_back(requested_types, callback);
  bool all_results_cached = true;
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
    if (requested_types[i] && cache_policies_[i] == CachePolicy::NO_CACHE) {
      all_results_cached = false;
      DoEnumerateDevices(static_cast<MediaDeviceType>(i));
    }
  }

  if (all_results_cached)
    ProcessRequests();
}

void MediaDevicesManager::EnumerateDevices(
    int render_process_id,
    int render_frame_id,
    const std::string& group_id_salt_base,
    const BoolDeviceTypes& requested_types,
    bool request_video_input_capabilities,
    EnumerateDevicesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTaskAndReplyWithResult(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI).get(), FROM_HERE,
      base::BindOnce(salt_and_origin_callback_, render_process_id,
                     render_frame_id),
      base::BindOnce(&MediaDevicesManager::CheckPermissionsForEnumerateDevices,
                     weak_factory_.GetWeakPtr(), render_process_id,
                     render_frame_id, group_id_salt_base, requested_types,
                     request_video_input_capabilities, std::move(callback)));
}

uint32_t MediaDevicesManager::SubscribeDeviceChangeNotifications(
    int render_process_id,
    int render_frame_id,
    const std::string& group_id_salt_base,
    const BoolDeviceTypes& subscribe_types,
    blink::mojom::MediaDevicesListenerPtr listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  uint32_t subscription_id = ++last_subscription_id_;
  blink::mojom::MediaDevicesListenerPtr media_devices_listener =
      std::move(listener);
  media_devices_listener.set_connection_error_handler(
      base::BindOnce(&MediaDevicesManager::UnsubscribeDeviceChangeNotifications,
                     weak_factory_.GetWeakPtr(), subscription_id));
  subscriptions_.emplace(
      subscription_id, SubscriptionRequest(render_process_id, render_frame_id,
                                           group_id_salt_base, subscribe_types,
                                           std::move(media_devices_listener)));

  return subscription_id;
}

void MediaDevicesManager::UnsubscribeDeviceChangeNotifications(
    uint32_t subscription_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  subscriptions_.erase(subscription_id);
}

void MediaDevicesManager::SetCachePolicy(MediaDeviceType type,
                                         CachePolicy policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  if (cache_policies_[type] == policy)
    return;

  cache_policies_[type] = policy;
  // If the new policy is SYSTEM_MONITOR, issue an enumeration to populate the
  // cache.
  if (policy == CachePolicy::SYSTEM_MONITOR) {
    cache_infos_[type].InvalidateCache();
    DoEnumerateDevices(type);
  }
}

void MediaDevicesManager::StartMonitoring() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (monitoring_started_)
    return;

  if (!base::SystemMonitor::Get())
    return;

#if defined(OS_MACOSX)
  if (!base::FeatureList::IsEnabled(features::kDeviceMonitorMac))
    return;
#endif

  monitoring_started_ = true;
  base::SystemMonitor::Get()->AddDevicesChangedObserver(this);

  if (base::FeatureList::IsEnabled(features::kMediaDevicesSystemMonitorCache)) {
    for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
      DCHECK(cache_policies_[i] != CachePolicy::SYSTEM_MONITOR);
      SetCachePolicy(static_cast<MediaDeviceType>(i),
                     CachePolicy::SYSTEM_MONITOR);
    }
  }

#if defined(OS_MACOSX)
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MediaDevicesManager::StartMonitoringOnUIThread,
                 base::Unretained(this)));
#endif
}

#if defined(OS_MACOSX)
void MediaDevicesManager::StartMonitoringOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserMainLoop* browser_main_loop = content::BrowserMainLoop::GetInstance();
  if (!browser_main_loop)
    return;
  browser_main_loop->device_monitor_mac()->StartMonitoring();
}
#endif

void MediaDevicesManager::StopMonitoring() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!monitoring_started_)
    return;
  base::SystemMonitor::Get()->RemoveDevicesChangedObserver(this);
  monitoring_started_ = false;
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i)
    SetCachePolicy(static_cast<MediaDeviceType>(i), CachePolicy::NO_CACHE);
}

bool MediaDevicesManager::IsMonitoringStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return monitoring_started_;
}

void MediaDevicesManager::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  switch (device_type) {
    case base::SystemMonitor::DEVTYPE_AUDIO:
      HandleDevicesChanged(MEDIA_DEVICE_TYPE_AUDIO_INPUT);
      HandleDevicesChanged(MEDIA_DEVICE_TYPE_AUDIO_OUTPUT);
      break;
    case base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE:
      HandleDevicesChanged(MEDIA_DEVICE_TYPE_VIDEO_INPUT);
      break;
    default:
      break;  // Uninteresting device change.
  }
}

media::VideoCaptureFormats MediaDevicesManager::GetVideoInputFormats(
    const std::string& device_id,
    bool try_in_use_first) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  media::VideoCaptureFormats formats;

  if (try_in_use_first) {
    base::Optional<media::VideoCaptureFormat> format =
        video_capture_manager_->GetDeviceFormatInUse(MEDIA_DEVICE_VIDEO_CAPTURE,
                                                     device_id);
    if (format.has_value()) {
      formats.push_back(format.value());
      return formats;
    }
  }

  video_capture_manager_->GetDeviceSupportedFormats(device_id, &formats);
  // Remove formats that have zero resolution.
  formats.erase(std::remove_if(formats.begin(), formats.end(),
                               [](const media::VideoCaptureFormat& format) {
                                 return format.frame_size.GetArea() <= 0;
                               }),
                formats.end());

  // If the device does not report any valid format, use a fallback list of
  // standard formats.
  if (formats.empty()) {
    for (const auto& resolution : kFallbackVideoResolutions) {
      for (const auto frame_rate : kFallbackVideoFrameRates) {
        formats.push_back(media::VideoCaptureFormat(
            gfx::Size(resolution.width, resolution.height), frame_rate,
            media::PIXEL_FORMAT_I420));
      }
    }
  }

  return formats;
}

MediaDeviceInfoArray MediaDevicesManager::GetCachedDeviceInfo(
    MediaDeviceType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return current_snapshot_[type];
}

MediaDevicesPermissionChecker*
MediaDevicesManager::media_devices_permission_checker() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return permission_checker_.get();
}

void MediaDevicesManager::SetPermissionChecker(
    std::unique_ptr<MediaDevicesPermissionChecker> permission_checker) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(permission_checker);
  permission_checker_ = std::move(permission_checker);
}

void MediaDevicesManager::CheckPermissionsForEnumerateDevices(
    int render_process_id,
    int render_frame_id,
    const std::string& group_id_salt_base,
    const BoolDeviceTypes& requested_types,
    bool request_video_input_capabilities,
    EnumerateDevicesCallback callback,
    const std::pair<std::string, url::Origin>& salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  permission_checker_->CheckPermissions(
      requested_types, render_process_id, render_frame_id,
      base::BindOnce(&MediaDevicesManager::OnPermissionsCheckDone,
                     weak_factory_.GetWeakPtr(), group_id_salt_base,
                     requested_types, request_video_input_capabilities,
                     std::move(callback), salt_and_origin.first,
                     salt_and_origin.second));
}

void MediaDevicesManager::OnPermissionsCheckDone(
    const std::string& group_id_salt_base,
    const MediaDevicesManager::BoolDeviceTypes& requested_types,
    bool request_video_input_capabilities,
    EnumerateDevicesCallback callback,
    const std::string& device_id_salt,
    const url::Origin& security_origin,
    const MediaDevicesManager::BoolDeviceTypes& has_permissions) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  EnumerateDevices(
      requested_types,
      base::BindRepeating(&MediaDevicesManager::OnDevicesEnumerated,
                          weak_factory_.GetWeakPtr(), group_id_salt_base,
                          requested_types, request_video_input_capabilities,
                          base::Passed(&callback), device_id_salt,
                          security_origin, has_permissions));
}

void MediaDevicesManager::OnDevicesEnumerated(
    const std::string& group_id_salt_base,
    const MediaDevicesManager::BoolDeviceTypes& requested_types,
    bool request_video_input_capabilities,
    EnumerateDevicesCallback callback,
    const std::string& device_id_salt,
    const url::Origin& security_origin,
    const MediaDevicesManager::BoolDeviceTypes& has_permissions,
    const MediaDeviceEnumeration& enumeration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::string group_id_salt = group_id_salt_base + device_id_salt;
  const bool video_input_capabilities_requested =
      has_permissions[MEDIA_DEVICE_TYPE_VIDEO_INPUT] &&
      request_video_input_capabilities;

  MediaDeviceInfoArray video_device_infos =
      enumeration[MEDIA_DEVICE_TYPE_VIDEO_INPUT];
  for (auto& video_device_info : video_device_infos) {
    video_device_info.group_id = GuessVideoGroupID(
        enumeration[MEDIA_DEVICE_TYPE_AUDIO_INPUT], video_device_info);
  }

  std::vector<MediaDeviceInfoArray> result(NUM_MEDIA_DEVICE_TYPES);
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
    if (!requested_types[i])
      continue;

    if (i == MEDIA_DEVICE_TYPE_VIDEO_INPUT) {
      for (const auto& device_info : video_device_infos) {
        MediaDeviceInfo translated_device_info = TranslateMediaDeviceInfo(
            has_permissions[i], device_id_salt, group_id_salt, security_origin,
            device_info);
        if (video_input_capabilities_requested)
          translated_device_info.video_facing = device_info.video_facing;
        result[i].push_back(translated_device_info);
      }
    } else {
      for (const auto& device_info : enumeration[i]) {
        result[i].push_back(TranslateMediaDeviceInfo(
            has_permissions[i], device_id_salt, group_id_salt, security_origin,
            device_info));
      }
    }
  }

  std::move(callback).Run(
      std::move(result),
      video_input_capabilities_requested
          ? ComputeVideoInputCapabilities(result[MEDIA_DEVICE_TYPE_VIDEO_INPUT])
          : std::vector<VideoInputDeviceCapabilitiesPtr>());
}

std::vector<VideoInputDeviceCapabilitiesPtr>
MediaDevicesManager::ComputeVideoInputCapabilities(
    const MediaDeviceInfoArray& device_infos) {
  std::vector<VideoInputDeviceCapabilitiesPtr> video_input_capabilities;
  for (const auto& device_info : device_infos) {
    VideoInputDeviceCapabilitiesPtr capabilities =
        blink::mojom::VideoInputDeviceCapabilities::New();
    capabilities->device_id = device_info.device_id;
    capabilities->formats = GetVideoInputFormats(device_info.device_id,
                                                 false /* try_in_use_first */);
    capabilities->facing_mode = device_info.video_facing;
#if defined(OS_ANDROID)
    // On Android, the facing mode is not available in the |facing| field,
    // but is available as part of the label.
    // TODO(guidou): Remove this code once the |facing| field is supported
    // on Android. See http://crbug.com/672856.
    if (device_info.label.find("front") != std::string::npos)
      capabilities->facing_mode = media::MEDIA_VIDEO_FACING_USER;
    else if (device_info.label.find("back") != std::string::npos)
      capabilities->facing_mode = media::MEDIA_VIDEO_FACING_ENVIRONMENT;
#endif
    video_input_capabilities.push_back(std::move(capabilities));
  }
  return video_input_capabilities;
}

void MediaDevicesManager::DoEnumerateDevices(MediaDeviceType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  CacheInfo& cache_info = cache_infos_[type];
  if (cache_info.is_update_ongoing())
    return;

  cache_info.UpdateStarted();
  switch (type) {
    case MEDIA_DEVICE_TYPE_AUDIO_INPUT:
      EnumerateAudioDevices(true /* is_input */);
      break;
    case MEDIA_DEVICE_TYPE_VIDEO_INPUT:
      video_capture_manager_->EnumerateDevices(
          base::Bind(&MediaDevicesManager::VideoInputDevicesEnumerated,
                     weak_factory_.GetWeakPtr()));
      break;
    case MEDIA_DEVICE_TYPE_AUDIO_OUTPUT:
      EnumerateAudioDevices(false /* is_input */);
      break;
    default:
      NOTREACHED();
  }
}

void MediaDevicesManager::EnumerateAudioDevices(bool is_input) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaDeviceType type =
      is_input ? MEDIA_DEVICE_TYPE_AUDIO_INPUT : MEDIA_DEVICE_TYPE_AUDIO_OUTPUT;
  if (use_fake_devices_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&MediaDevicesManager::DevicesEnumerated,
                                  weak_factory_.GetWeakPtr(), type,
                                  GetFakeAudioDevices(is_input)));
    return;
  }

  audio_system_->GetDeviceDescriptions(
      is_input, base::BindOnce(&MediaDevicesManager::AudioDevicesEnumerated,
                               weak_factory_.GetWeakPtr(), type));
}

void MediaDevicesManager::VideoInputDevicesEnumerated(
    const media::VideoCaptureDeviceDescriptors& descriptors) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaDeviceInfoArray snapshot;
  for (const auto& descriptor : descriptors) {
    snapshot.emplace_back(descriptor);
  }
  DevicesEnumerated(MEDIA_DEVICE_TYPE_VIDEO_INPUT, snapshot);
}

void MediaDevicesManager::AudioDevicesEnumerated(
    MediaDeviceType type,
    media::AudioDeviceDescriptions device_descriptions) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  MediaDeviceInfoArray snapshot;
  for (const media::AudioDeviceDescription& description : device_descriptions) {
    snapshot.emplace_back(description);
  }
  DevicesEnumerated(type, snapshot);
}

void MediaDevicesManager::DevicesEnumerated(
    MediaDeviceType type,
    const MediaDeviceInfoArray& snapshot) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  UpdateSnapshot(type, snapshot);
  cache_infos_[type].UpdateCompleted();
  has_seen_result_[type] = true;

  std::string log_message =
      "New device enumeration result:\n" + GetLogMessageString(type, snapshot);
  MediaStreamManager::SendMessageToNativeLog(log_message);

  if (cache_policies_[type] == CachePolicy::NO_CACHE) {
    for (auto& request : requests_)
      request.has_seen_result[type] = true;
  }

  // Note that IsLastUpdateValid is always true when policy is NO_CACHE.
  if (cache_infos_[type].IsLastUpdateValid()) {
    ProcessRequests();
  } else {
    DoEnumerateDevices(type);
  }
}

void MediaDevicesManager::UpdateSnapshot(
    MediaDeviceType type,
    const MediaDeviceInfoArray& new_snapshot) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));

  // Only cache the device list when the device list has been changed.
  bool need_update_device_change_subscribers = false;
  MediaDeviceInfoArray& old_snapshot = current_snapshot_[type];

  if (old_snapshot.size() != new_snapshot.size() ||
      !std::equal(new_snapshot.begin(), new_snapshot.end(),
                  old_snapshot.begin())) {
    if (type == MEDIA_DEVICE_TYPE_AUDIO_INPUT ||
        type == MEDIA_DEVICE_TYPE_VIDEO_INPUT) {
      NotifyMediaStreamManager(type, new_snapshot);
    }

    // Do not notify device-change subscribers after the first enumeration
    // result, since it is not due to an actual device change.
    need_update_device_change_subscribers =
        has_seen_result_[type] &&
        (old_snapshot.size() != 0 || new_snapshot.size() != 0);
    current_snapshot_[type] = new_snapshot;
  }

  if (need_update_device_change_subscribers)
    NotifyDeviceChangeSubscribers(type, new_snapshot);
}

void MediaDevicesManager::ProcessRequests() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  requests_.erase(std::remove_if(requests_.begin(), requests_.end(),
                                 [this](const EnumerationRequest& request) {
                                   if (IsEnumerationRequestReady(request)) {
                                     request.callback.Run(current_snapshot_);
                                     return true;
                                   }
                                   return false;
                                 }),
                  requests_.end());
}

bool MediaDevicesManager::IsEnumerationRequestReady(
    const EnumerationRequest& request_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool is_ready = true;
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
    if (!request_info.requested[i])
      continue;
    switch (cache_policies_[i]) {
      case CachePolicy::SYSTEM_MONITOR:
        if (!cache_infos_[i].IsLastUpdateValid())
          is_ready = false;
        break;
      case CachePolicy::NO_CACHE:
        if (!request_info.has_seen_result[i])
          is_ready = false;
        break;
      default:
        NOTREACHED();
    }
  }
  return is_ready;
}

void MediaDevicesManager::HandleDevicesChanged(MediaDeviceType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  cache_infos_[type].InvalidateCache();
  DoEnumerateDevices(type);
}

void MediaDevicesManager::NotifyMediaStreamManager(
    MediaDeviceType type,
    const MediaDeviceInfoArray& new_snapshot) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(type == MEDIA_DEVICE_TYPE_AUDIO_INPUT ||
         type == MEDIA_DEVICE_TYPE_VIDEO_INPUT);

  if (!media_stream_manager_)
    return;

  for (const auto& old_device_info : current_snapshot_[type]) {
    auto it = std::find_if(new_snapshot.begin(), new_snapshot.end(),
                           [&old_device_info](const MediaDeviceInfo& info) {
                             return info.device_id == old_device_info.device_id;
                           });

    // If a device was removed, notify the MediaStreamManager to stop all
    // streams using that device.
    if (it == new_snapshot.end())
      media_stream_manager_->StopRemovedDevice(type, old_device_info);
  }

  media_stream_manager_->NotifyDevicesChanged(type, new_snapshot);
}

void MediaDevicesManager::NotifyDeviceChangeSubscribers(
    MediaDeviceType type,
    const MediaDeviceInfoArray& snapshot) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));

  for (auto& subscription : subscriptions_) {
    SubscriptionRequest* request = &subscription.second;
    if (request->subscribe_types[type]) {
      base::PostTaskAndReplyWithResult(
          BrowserThread::GetTaskRunnerForThread(BrowserThread::UI).get(),
          FROM_HERE,
          base::BindOnce(salt_and_origin_callback_, request->render_process_id,
                         request->render_frame_id),
          base::BindOnce(&MediaDevicesManager::CheckPermissionForDeviceChange,
                         weak_factory_.GetWeakPtr(), request, type, snapshot));
    }
  }
}

void MediaDevicesManager::CheckPermissionForDeviceChange(
    SubscriptionRequest* request,
    MediaDeviceType type,
    const MediaDeviceInfoArray& device_infos,
    const std::pair<std::string, url::Origin>& salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  permission_checker_->CheckPermission(
      type, request->render_process_id, request->render_frame_id,
      base::BindOnce(&MediaDevicesManager::NotifyDeviceChange,
                     weak_factory_.GetWeakPtr(), request, type, device_infos,
                     salt_and_origin.first, salt_and_origin.second));
}

void MediaDevicesManager::NotifyDeviceChange(
    SubscriptionRequest* request,
    MediaDeviceType type,
    const MediaDeviceInfoArray& device_infos,
    std::string device_id_salt,
    const url::Origin& security_origin,
    bool has_permission) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  std::string group_id_salt = request->group_id_salt_base + device_id_salt;
  request->listener->OnDevicesChanged(
      type, TranslateMediaDeviceInfoArray(has_permission, device_id_salt,
                                          group_id_salt, security_origin,
                                          device_infos));
}

}  // namespace content
