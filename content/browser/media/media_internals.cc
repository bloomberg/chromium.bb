// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals.h"

#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_ui.h"
#include "media/audio/audio_parameters.h"
#include "media/base/media_log_event.h"
#include "media/filters/decrypting_video_decoder.h"
#include "media/filters/gpu_video_decoder.h"

namespace {

static base::LazyInstance<content::MediaInternals>::Leaky g_media_internals =
    LAZY_INSTANCE_INITIALIZER;

base::string16 SerializeUpdate(const std::string& function,
                               const base::Value* value) {
  return content::WebUI::GetJavascriptCall(
      function, std::vector<const base::Value*>(1, value));
}

std::string EffectsToString(int effects) {
  if (effects == media::AudioParameters::NO_EFFECTS)
    return "NO_EFFECTS";

  struct {
    int flag;
    const char* name;
  } flags[] = {
    { media::AudioParameters::ECHO_CANCELLER, "ECHO_CANCELLER" },
    { media::AudioParameters::DUCKING, "DUCKING" },
    { media::AudioParameters::KEYBOARD_MIC, "KEYBOARD_MIC" },
    { media::AudioParameters::HOTWORD, "HOTWORD" },
  };

  std::string ret;
  for (size_t i = 0; i < arraysize(flags); ++i) {
    if (effects & flags[i].flag) {
      if (!ret.empty())
        ret += " | ";
      ret += flags[i].name;
      effects &= ~flags[i].flag;
    }
  }

  if (effects) {
    if (!ret.empty())
      ret += " | ";
    ret += base::IntToString(effects);
  }

  return ret;
}

std::string FormatToString(media::AudioParameters::Format format) {
  switch (format) {
    case media::AudioParameters::AUDIO_PCM_LINEAR:
      return "pcm_linear";
    case media::AudioParameters::AUDIO_PCM_LOW_LATENCY:
      return "pcm_low_latency";
    case media::AudioParameters::AUDIO_FAKE:
      return "fake";
    case media::AudioParameters::AUDIO_LAST_FORMAT:
      break;
  }

  NOTREACHED();
  return "unknown";
}

const char kAudioLogStatusKey[] = "status";
const char kAudioLogUpdateFunction[] = "media.updateAudioComponent";

}  // namespace

namespace content {

class AudioLogImpl : public media::AudioLog {
 public:
  AudioLogImpl(int owner_id,
               media::AudioLogFactory::AudioComponent component,
               content::MediaInternals* media_internals);
  ~AudioLogImpl() override;

  void OnCreated(int component_id,
                 const media::AudioParameters& params,
                 const std::string& device_id) override;
  void OnStarted(int component_id) override;
  void OnStopped(int component_id) override;
  void OnClosed(int component_id) override;
  void OnError(int component_id) override;
  void OnSetVolume(int component_id, double volume) override;

 private:
  void SendSingleStringUpdate(int component_id,
                              const std::string& key,
                              const std::string& value);
  void StoreComponentMetadata(int component_id, base::DictionaryValue* dict);
  std::string FormatCacheKey(int component_id);

  const int owner_id_;
  const media::AudioLogFactory::AudioComponent component_;
  content::MediaInternals* const media_internals_;

  DISALLOW_COPY_AND_ASSIGN(AudioLogImpl);
};

AudioLogImpl::AudioLogImpl(int owner_id,
                           media::AudioLogFactory::AudioComponent component,
                           content::MediaInternals* media_internals)
    : owner_id_(owner_id),
      component_(component),
      media_internals_(media_internals) {}

AudioLogImpl::~AudioLogImpl() {}

void AudioLogImpl::OnCreated(int component_id,
                             const media::AudioParameters& params,
                             const std::string& device_id) {
  base::DictionaryValue dict;
  StoreComponentMetadata(component_id, &dict);

  dict.SetString(kAudioLogStatusKey, "created");
  dict.SetString("device_id", device_id);
  dict.SetString("device_type", FormatToString(params.format()));
  dict.SetInteger("frames_per_buffer", params.frames_per_buffer());
  dict.SetInteger("sample_rate", params.sample_rate());
  dict.SetInteger("channels", params.channels());
  dict.SetString("channel_layout",
                 ChannelLayoutToString(params.channel_layout()));
  dict.SetString("effects", EffectsToString(params.effects()));

  media_internals_->SendUpdateAndCacheAudioStreamKey(
      FormatCacheKey(component_id), kAudioLogUpdateFunction, &dict);
}

void AudioLogImpl::OnStarted(int component_id) {
  SendSingleStringUpdate(component_id, kAudioLogStatusKey, "started");
}

void AudioLogImpl::OnStopped(int component_id) {
  SendSingleStringUpdate(component_id, kAudioLogStatusKey, "stopped");
}

void AudioLogImpl::OnClosed(int component_id) {
  base::DictionaryValue dict;
  StoreComponentMetadata(component_id, &dict);
  dict.SetString(kAudioLogStatusKey, "closed");
  media_internals_->SendUpdateAndPurgeAudioStreamCache(
      FormatCacheKey(component_id), kAudioLogUpdateFunction, &dict);
}

void AudioLogImpl::OnError(int component_id) {
  SendSingleStringUpdate(component_id, "error_occurred", "true");
}

void AudioLogImpl::OnSetVolume(int component_id, double volume) {
  base::DictionaryValue dict;
  StoreComponentMetadata(component_id, &dict);
  dict.SetDouble("volume", volume);
  media_internals_->SendUpdateAndCacheAudioStreamKey(
      FormatCacheKey(component_id), kAudioLogUpdateFunction, &dict);
}

std::string AudioLogImpl::FormatCacheKey(int component_id) {
  return base::StringPrintf("%d:%d:%d", owner_id_, component_, component_id);
}

void AudioLogImpl::SendSingleStringUpdate(int component_id,
                                          const std::string& key,
                                          const std::string& value) {
  base::DictionaryValue dict;
  StoreComponentMetadata(component_id, &dict);
  dict.SetString(key, value);
  media_internals_->SendUpdateAndCacheAudioStreamKey(
      FormatCacheKey(component_id), kAudioLogUpdateFunction, &dict);
}

void AudioLogImpl::StoreComponentMetadata(int component_id,
                                          base::DictionaryValue* dict) {
  dict->SetInteger("owner_id", owner_id_);
  dict->SetInteger("component_id", component_id);
  dict->SetInteger("component_type", component_);
}

class MediaInternals::MediaInternalsUMAHandler : public NotificationObserver {
 public:
  MediaInternalsUMAHandler();

  // NotificationObserver implementation.
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  // Reports the pipeline status to UMA for every player
  // associated with the renderer process and then deletes the player state.
  void LogAndClearPlayersInRenderer(int render_process_id);

  // Helper function to save the event payload to RendererPlayerMap.
  void SavePlayerState(const media::MediaLogEvent& event,
                       int render_process_id);

 private:
  struct PipelineInfo {
    media::PipelineStatus last_pipeline_status;
    bool has_audio;
    bool has_video;
    bool video_dds;
    bool video_decoder_changed;
    std::string audio_codec_name;
    std::string video_codec_name;
    std::string video_decoder;
    PipelineInfo()
        : last_pipeline_status(media::PIPELINE_OK),
          has_audio(false),
          has_video(false),
          video_dds(false),
          video_decoder_changed(false) {}
  };

  // Helper function to report PipelineStatus associated with a player to UMA.
  void ReportUMAForPipelineStatus(const PipelineInfo& player_info);

  // Helper to generate PipelineStatus UMA name for AudioVideo streams.
  std::string GetUMANameForAVStream(const PipelineInfo& player_info);

  // Key is playerid
  typedef std::map<int, PipelineInfo> PlayerInfoMap;

  // Key is renderer id
  typedef std::map<int, PlayerInfoMap> RendererPlayerMap;

  // Stores player information per renderer
  RendererPlayerMap renderer_info_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(MediaInternalsUMAHandler);
};

MediaInternals::MediaInternalsUMAHandler::MediaInternalsUMAHandler() {
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllBrowserContextsAndSources());
}

void MediaInternals::MediaInternalsUMAHandler::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(type, NOTIFICATION_RENDERER_PROCESS_TERMINATED);
  RenderProcessHost* process = Source<RenderProcessHost>(source).ptr();

  // Post the task to the IO thread to avoid race in updating renderer_info_ map
  // by both SavePlayerState & LogAndClearPlayersInRenderer from different
  // threads.
  // Using base::Unretained() on MediaInternalsUMAHandler is safe since
  // it is owned by MediaInternals and share the same lifetime
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaInternalsUMAHandler::LogAndClearPlayersInRenderer,
                 base::Unretained(this), process->GetID()));
}

void MediaInternals::MediaInternalsUMAHandler::SavePlayerState(
    const media::MediaLogEvent& event,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  PlayerInfoMap& player_info = renderer_info_[render_process_id];
  switch (event.type) {
    case media::MediaLogEvent::PIPELINE_ERROR: {
      int status;
      event.params.GetInteger("pipeline_error", &status);
      player_info[event.id].last_pipeline_status =
          static_cast<media::PipelineStatus>(status);
      break;
    }
    case media::MediaLogEvent::PROPERTY_CHANGE:
      if (event.params.HasKey("found_audio_stream")) {
        event.params.GetBoolean("found_audio_stream",
                                &player_info[event.id].has_audio);
      }
      if (event.params.HasKey("found_video_stream")) {
        event.params.GetBoolean("found_video_stream",
                                &player_info[event.id].has_video);
      }
      if (event.params.HasKey("audio_codec_name")) {
        event.params.GetString("audio_codec_name",
                               &player_info[event.id].audio_codec_name);
      }
      if (event.params.HasKey("video_codec_name")) {
        event.params.GetString("video_codec_name",
                               &player_info[event.id].video_codec_name);
      }
      if (event.params.HasKey("video_decoder")) {
        std::string previous_video_decoder(player_info[event.id].video_decoder);
        event.params.GetString("video_decoder",
                               &player_info[event.id].video_decoder);
        if (!previous_video_decoder.empty() &&
            previous_video_decoder != player_info[event.id].video_decoder) {
          player_info[event.id].video_decoder_changed = true;
        }
      }
      if (event.params.HasKey("video_dds")) {
        event.params.GetBoolean("video_dds", &player_info[event.id].video_dds);
      }
      break;
    default:
      break;
  }
  return;
}

std::string MediaInternals::MediaInternalsUMAHandler::GetUMANameForAVStream(
    const PipelineInfo& player_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  static const char kPipelineUmaPrefix[] = "Media.PipelineStatus.AudioVideo.";
  std::string uma_name = kPipelineUmaPrefix;
  if (player_info.video_codec_name == "vp8") {
    uma_name += "VP8.";
  } else if (player_info.video_codec_name == "vp9") {
    uma_name += "VP9.";
  } else if (player_info.video_codec_name == "h264") {
    uma_name += "H264.";
  } else {
    return uma_name + "Other";
  }

  if (player_info.video_decoder ==
      media::DecryptingVideoDecoder::kDecoderName) {
    return uma_name + "DVD";
  }

  if (player_info.video_dds) {
    uma_name += "DDS.";
  }

  if (player_info.video_decoder == media::GpuVideoDecoder::kDecoderName) {
    uma_name += "HW";
  } else {
    uma_name += "SW";
  }
  return uma_name;
}

void MediaInternals::MediaInternalsUMAHandler::ReportUMAForPipelineStatus(
    const PipelineInfo& player_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (player_info.has_video && player_info.has_audio) {
    base::LinearHistogram::FactoryGet(
        GetUMANameForAVStream(player_info), 1, media::PIPELINE_STATUS_MAX,
        media::PIPELINE_STATUS_MAX + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(player_info.last_pipeline_status);
  } else if (player_info.has_audio) {
    UMA_HISTOGRAM_ENUMERATION("Media.PipelineStatus.AudioOnly",
                              player_info.last_pipeline_status,
                              media::PIPELINE_STATUS_MAX + 1);
  } else if (player_info.has_video) {
    UMA_HISTOGRAM_ENUMERATION("Media.PipelineStatus.VideoOnly",
                              player_info.last_pipeline_status,
                              media::PIPELINE_STATUS_MAX + 1);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Media.PipelineStatus.Unsupported",
                              player_info.last_pipeline_status,
                              media::PIPELINE_STATUS_MAX + 1);
  }
  // Report whether video decoder fallback happened, but only if a video decoder
  // was reported.
  if (!player_info.video_decoder.empty()) {
    UMA_HISTOGRAM_BOOLEAN("Media.VideoDecoderFallback",
                          player_info.video_decoder_changed);
  }
}

void MediaInternals::MediaInternalsUMAHandler::LogAndClearPlayersInRenderer(
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto players_it = renderer_info_.find(render_process_id);
  if (players_it == renderer_info_.end())
    return;
  auto it = players_it->second.begin();
  while (it != players_it->second.end()) {
    ReportUMAForPipelineStatus(it->second);
    players_it->second.erase(it++);
  }
}

MediaInternals* MediaInternals::GetInstance() {
  return g_media_internals.Pointer();
}

MediaInternals::MediaInternals()
    : owner_ids_(), uma_handler_(new MediaInternalsUMAHandler()) {
}

MediaInternals::~MediaInternals() {}

void MediaInternals::OnMediaEvents(
    int render_process_id, const std::vector<media::MediaLogEvent>& events) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Notify observers that |event| has occurred.
  for (auto event = events.begin(); event != events.end(); ++event) {
    base::DictionaryValue dict;
    dict.SetInteger("renderer", render_process_id);
    dict.SetInteger("player", event->id);
    dict.SetString("type", media::MediaLog::EventTypeToString(event->type));

    // TODO(dalecurtis): This is technically not correct.  TimeTicks "can't" be
    // converted to to a human readable time format.  See base/time/time.h.
    const double ticks = event->time.ToInternalValue();
    const double ticks_millis = ticks / base::Time::kMicrosecondsPerMillisecond;
    dict.SetDouble("ticksMillis", ticks_millis);

    // Convert PipelineStatus to human readable string
    if (event->type == media::MediaLogEvent::PIPELINE_ERROR) {
      int status;
      event->params.GetInteger("pipeline_error", &status);
      media::PipelineStatus error = static_cast<media::PipelineStatus>(status);
      dict.SetString("params.pipeline_error",
                     media::MediaLog::PipelineStatusToString(error));
    } else {
      dict.Set("params", event->params.DeepCopy());
    }

    SendUpdate(SerializeUpdate("media.onMediaEvent", &dict));
    uma_handler_->SavePlayerState(*event, render_process_id);
  }
}

void MediaInternals::AddUpdateCallback(const UpdateCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  update_callbacks_.push_back(callback);
}

void MediaInternals::RemoveUpdateCallback(const UpdateCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (size_t i = 0; i < update_callbacks_.size(); ++i) {
    if (update_callbacks_[i].Equals(callback)) {
      update_callbacks_.erase(update_callbacks_.begin() + i);
      return;
    }
  }
  NOTREACHED();
}

void MediaInternals::SendAudioStreamData() {
  base::string16 audio_stream_update;
  {
    base::AutoLock auto_lock(lock_);
    audio_stream_update = SerializeUpdate(
        "media.onReceiveAudioStreamData", &audio_streams_cached_data_);
  }
  SendUpdate(audio_stream_update);
}

void MediaInternals::SendVideoCaptureDeviceCapabilities() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendUpdate(SerializeUpdate("media.onReceiveVideoCaptureCapabilities",
                             &video_capture_capabilities_cached_data_));
}

void MediaInternals::UpdateVideoCaptureDeviceCapabilities(
    const media::VideoCaptureDeviceInfos& video_capture_device_infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  video_capture_capabilities_cached_data_.Clear();

  for (const auto& video_capture_device_info : video_capture_device_infos) {
    base::ListValue* format_list = new base::ListValue();
    for (const auto& format : video_capture_device_info.supported_formats)
      format_list->AppendString(format.ToString());

    base::DictionaryValue* device_dict = new base::DictionaryValue();
    device_dict->SetString("id", video_capture_device_info.name.id());
    device_dict->SetString(
        "name", video_capture_device_info.name.GetNameAndModel());
    device_dict->Set("formats", format_list);
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
    device_dict->SetString(
        "captureApi",
        video_capture_device_info.name.GetCaptureApiTypeString());
#endif
    video_capture_capabilities_cached_data_.Append(device_dict);
  }

  if (update_callbacks_.size() > 0)
    SendVideoCaptureDeviceCapabilities();
}

scoped_ptr<media::AudioLog> MediaInternals::CreateAudioLog(
    AudioComponent component) {
  base::AutoLock auto_lock(lock_);
  return scoped_ptr<media::AudioLog>(new AudioLogImpl(
      owner_ids_[component]++, component, this));
}

void MediaInternals::SendUpdate(const base::string16& update) {
  // SendUpdate() may be called from any thread, but must run on the IO thread.
  // TODO(dalecurtis): This is pretty silly since the update callbacks simply
  // forward the calls to the UI thread.  We should avoid the extra hop.
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
        &MediaInternals::SendUpdate, base::Unretained(this), update));
    return;
  }

  for (size_t i = 0; i < update_callbacks_.size(); i++)
    update_callbacks_[i].Run(update);
}

void MediaInternals::SendUpdateAndCacheAudioStreamKey(
    const std::string& cache_key,
    const std::string& function,
    const base::DictionaryValue* value) {
  SendUpdate(SerializeUpdate(function, value));

  base::AutoLock auto_lock(lock_);
  if (!audio_streams_cached_data_.HasKey(cache_key)) {
    audio_streams_cached_data_.Set(cache_key, value->DeepCopy());
    return;
  }

  base::DictionaryValue* existing_dict = NULL;
  CHECK(audio_streams_cached_data_.GetDictionary(cache_key, &existing_dict));
  existing_dict->MergeDictionary(value);
}

void MediaInternals::SendUpdateAndPurgeAudioStreamCache(
    const std::string& cache_key,
    const std::string& function,
    const base::DictionaryValue* value) {
  SendUpdate(SerializeUpdate(function, value));

  base::AutoLock auto_lock(lock_);
  scoped_ptr<base::Value> out_value;
  CHECK(audio_streams_cached_data_.Remove(cache_key, &out_value));
}

}  // namespace content
