// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals.h"

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "media/base/media_log.h"
#include "media/base/media_log_event.h"

namespace content {

MediaInternals* MediaInternals::GetInstance() {
  return Singleton<MediaInternals>::get();
}

MediaInternals::~MediaInternals() {}

void MediaInternals::OnDeleteAudioStream(void* host, int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  std::string stream = base::StringPrintf("audio_streams.%p:%d",
                                          host, stream_id);
  DeleteItem(stream);
}

void MediaInternals::OnSetAudioStreamPlaying(
    void* host, int stream_id, bool playing) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  UpdateAudioStream(host, stream_id,
                    "playing", new base::FundamentalValue(playing));
}

void MediaInternals::OnSetAudioStreamStatus(
    void* host, int stream_id, const std::string& status) {
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
    dict.SetDouble("time", event->time.ToDoubleT());
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

void MediaInternals::UpdateAudioStream(void* host,
                                       int stream_id,
                                       const std::string& property,
                                       base::Value* value) {
  std::string stream = base::StringPrintf("audio_streams.%p:%d",
                                          host, stream_id);
  UpdateItem("media.addAudioStream", stream, property, value);
}

void MediaInternals::DeleteItem(const std::string& item) {
  data_.Remove(item, NULL);
  scoped_ptr<base::Value> value(new base::StringValue(item));
  SendUpdate("media.onItemDeleted", value.get());
}

void MediaInternals::UpdateItem(
    const std::string& update_fn, const std::string& id,
    const std::string& property, base::Value* value) {
  base::DictionaryValue* item_properties;
  if (!data_.GetDictionary(id, &item_properties)) {
    item_properties = new base::DictionaryValue();
    data_.Set(id, item_properties);
    item_properties->SetString("id", id);
  }
  item_properties->Set(property, value);
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
