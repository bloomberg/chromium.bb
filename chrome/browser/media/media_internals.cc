// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_internals.h"

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "chrome/browser/media/media_internals_observer.h"
#include "content/browser/browser_thread.h"
#include "content/browser/webui/web_ui.h"
#include "media/base/media_log.h"
#include "media/base/media_log_event.h"

MediaInternals::~MediaInternals() {}

void MediaInternals::OnDeleteAudioStream(void* host, int stream_id) {
  DCHECK(CalledOnValidThread());
  std::string stream = base::StringPrintf("audio_streams.%p:%d",
                                          host, stream_id);
  DeleteItem(stream);
}

void MediaInternals::OnSetAudioStreamPlaying(
    void* host, int stream_id, bool playing) {
  DCHECK(CalledOnValidThread());
  UpdateAudioStream(host, stream_id,
                    "playing", Value::CreateBooleanValue(playing));
}

void MediaInternals::OnSetAudioStreamStatus(
    void* host, int stream_id, const std::string& status) {
  DCHECK(CalledOnValidThread());
  UpdateAudioStream(host, stream_id,
                    "status", Value::CreateStringValue(status));
}

void MediaInternals::OnSetAudioStreamVolume(
    void* host, int stream_id, double volume) {
  DCHECK(CalledOnValidThread());
  UpdateAudioStream(host, stream_id,
                    "volume", Value::CreateDoubleValue(volume));
}

void MediaInternals::OnMediaEvent(
    int render_process_id, const media::MediaLogEvent& event) {
  DCHECK(CalledOnValidThread());

  // Notify observers that |event| has occured.
  DictionaryValue dict;
  dict.SetInteger("renderer", render_process_id);
  dict.SetInteger("player", event.id);
  dict.SetString("type", media::MediaLog::EventTypeToString(event.type));
  dict.SetDouble("time", event.time.ToDoubleT());
  dict.Set("params", event.params.DeepCopy());
  SendUpdate("media.onMediaEvent", &dict);
}

void MediaInternals::AddObserver(MediaInternalsObserver* observer) {
  DCHECK(CalledOnValidThread());
  observers_.AddObserver(observer);
}

void MediaInternals::RemoveObserver(MediaInternalsObserver* observer) {
  DCHECK(CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void MediaInternals::SendEverything() {
  DCHECK(CalledOnValidThread());
  SendUpdate("media.onReceiveEverything", &data_);
}

MediaInternals::MediaInternals() {}

void MediaInternals::UpdateAudioStream(
    void* host, int stream_id, const std::string& property, Value* value) {
  std::string stream = base::StringPrintf("audio_streams.%p:%d",
                                          host, stream_id);
  UpdateItem("media.addAudioStream", stream, property, value);
}

void MediaInternals::DeleteItem(const std::string& item) {
  data_.Remove(item, NULL);
  scoped_ptr<Value> value(Value::CreateStringValue(item));
  SendUpdate("media.onItemDeleted", value.get());
}

void MediaInternals::UpdateItem(
    const std::string& update_fn, const std::string& id,
    const std::string& property, Value* value) {
  DictionaryValue* item_properties;
  if (!data_.GetDictionary(id, &item_properties)) {
    item_properties = new DictionaryValue();
    data_.Set(id, item_properties);
    item_properties->SetString("id", id);
  }
  item_properties->Set(property, value);
  SendUpdate(update_fn, item_properties);
}

void MediaInternals::SendUpdate(const std::string& function, Value* value) {
  // Only bother serializing the update to JSON if someone is watching.
  if (observers_.size()) {
    std::vector<const Value*> args;
    args.push_back(value);
    string16 update = WebUI::GetJavascriptCall(function, args);
    FOR_EACH_OBSERVER(MediaInternalsObserver, observers_, OnUpdate(update));
  }
}
