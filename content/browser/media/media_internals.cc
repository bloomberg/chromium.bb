// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "media/audio/audio_parameters.h"
#include "media/base/media_log.h"
#include "media/base/media_log_event.h"

namespace content {

MediaInternals* MediaInternals::GetInstance() {
  return Singleton<MediaInternals>::get();
}

MediaInternals::~MediaInternals() {}

namespace {
std::string FormatAudioStreamName(void* host, int stream_id) {
  return base::StringPrintf("audio_streams.%p:%d", host, stream_id);
}
}

void MediaInternals::OnDeleteAudioStream(void* host, int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DeleteItem(FormatAudioStreamName(host, stream_id));
}

void MediaInternals::OnSetAudioStreamPlaying(
    void* host, int stream_id, bool playing) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  UpdateAudioStream(host, stream_id,
                    "playing", new base::FundamentalValue(playing));
}

void MediaInternals::OnAudioStreamCreated(void* host,
                                          int stream_id,
                                          const media::AudioParameters& params,
                                          const std::string& input_device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  StoreAudioStream(host,
                   stream_id,
                   "input_device_id",
                   Value::CreateStringValue(input_device_id));

  StoreAudioStream(
      host, stream_id, "status", Value::CreateStringValue("created"));

  StoreAudioStream(
      host, stream_id, "stream_id", Value::CreateIntegerValue(stream_id));

  StoreAudioStream(host,
                   stream_id,
                   "input_channels",
                   Value::CreateIntegerValue(params.input_channels()));

  StoreAudioStream(host,
                   stream_id,
                   "frames_per_buffer",
                   Value::CreateIntegerValue(params.frames_per_buffer()));

  StoreAudioStream(host,
                   stream_id,
                   "sample_rate",
                   Value::CreateIntegerValue(params.sample_rate()));

  StoreAudioStream(host,
                   stream_id,
                   "output_channels",
                   Value::CreateIntegerValue(params.channels()));

  StoreAudioStream(
      host,
      stream_id,
      "channel_layout",
      Value::CreateStringValue(ChannelLayoutToString(params.channel_layout())));

  SendEverything();
}

void MediaInternals::OnSetAudioStreamStatus(void* host,
                                            int stream_id,
                                            const std::string& status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  UpdateAudioStream(host, stream_id,
                    "status", new base::StringValue(status));
}

void MediaInternals::OnSetAudioStreamVolume(
    void* host, int stream_id, double volume) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  UpdateAudioStream(host, stream_id,
                    "volume", new base::FundamentalValue(volume));
}

void MediaInternals::OnMediaEvents(
    int render_process_id, const std::vector<media::MediaLogEvent>& events) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Notify observers that |event| has occured.
  for (std::vector<media::MediaLogEvent>::const_iterator event = events.begin();
      event != events.end(); ++event) {
    base::DictionaryValue dict;
    dict.SetInteger("renderer", render_process_id);
    dict.SetInteger("player", event->id);
    dict.SetString("type", media::MediaLog::EventTypeToString(event->type));

    int64 ticks = event->time.ToInternalValue();
    double ticks_millis =
        ticks / static_cast<double>(base::Time::kMicrosecondsPerMillisecond);

    dict.SetDouble("ticksMillis", ticks_millis);
    dict.Set("params", event->params.DeepCopy());
    SendUpdate("media.onMediaEvent", &dict);
  }
}

void MediaInternals::AddUpdateCallback(const UpdateCallback& callback) {
  update_callbacks_.push_back(callback);
}

void MediaInternals::RemoveUpdateCallback(const UpdateCallback& callback) {
  for (size_t i = 0; i < update_callbacks_.size(); ++i) {
    if (update_callbacks_[i].Equals(callback)) {
      update_callbacks_.erase(update_callbacks_.begin() + i);
      return;
    }
  }
  NOTREACHED();
}

void MediaInternals::SendEverything() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  SendUpdate("media.onReceiveEverything", &data_);
}

MediaInternals::MediaInternals() {
}

void MediaInternals::StoreAudioStream(void* host,
                                      int stream_id,
                                      const std::string& property,
                                      base::Value* value) {
  StoreItem(FormatAudioStreamName(host, stream_id), property, value);
}

void MediaInternals::UpdateAudioStream(void* host,
                                       int stream_id,
                                       const std::string& property,
                                       base::Value* value) {
  UpdateItem("media.updateAudioStream",
             FormatAudioStreamName(host, stream_id),
             property,
             value);
}

void MediaInternals::DeleteItem(const std::string& item) {
  data_.Remove(item, NULL);
  scoped_ptr<base::Value> value(new base::StringValue(item));
  SendUpdate("media.onItemDeleted", value.get());
}

base::DictionaryValue* MediaInternals::StoreItem(const std::string& id,
                                                 const std::string& property,
                                                 base::Value* value) {
  base::DictionaryValue* item_properties;
  if (!data_.GetDictionary(id, &item_properties)) {
    item_properties = new base::DictionaryValue();
    data_.Set(id, item_properties);
    item_properties->SetString("id", id);
  }
  item_properties->Set(property, value);
  return item_properties;
}

void MediaInternals::UpdateItem(const std::string& update_fn,
                                const std::string& id,
                                const std::string& property,
                                base::Value* value) {
  base::DictionaryValue* item_properties = StoreItem(id, property, value);
  SendUpdate(update_fn, item_properties);
}

void MediaInternals::SendUpdate(const std::string& function,
                                base::Value* value) {
  // Only bother serializing the update to JSON if someone is watching.
  if (update_callbacks_.empty())
    return;

  std::vector<const base::Value*> args;
  args.push_back(value);
  string16 update = WebUI::GetJavascriptCall(function, args);
  for (size_t i = 0; i < update_callbacks_.size(); i++)
    update_callbacks_[i].Run(update);
}

}  // namespace content
