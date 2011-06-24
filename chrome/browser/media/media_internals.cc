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

// The names of the javascript functions to call with updates.
static const char kAudioUpdateFunction[] = "onAudioUpdate";
static const char kSendEverythingFunction[] = "onReceiveEverything";

MediaInternals::~MediaInternals() {}

void MediaInternals::OnSetAudioStreamPlaying(
    void* host, int32 render_view, int stream_id, bool playing) {
  UpdateAudioStream(host, render_view, stream_id,
                    "playing", Value::CreateBooleanValue(playing));
}

void MediaInternals::OnSetAudioStreamVolume(
    void* host, int32 render_view, int stream_id, double volume) {
  UpdateAudioStream(host, render_view, stream_id,
                    "volume", Value::CreateDoubleValue(volume));
}

void MediaInternals::AddUI(MediaInternalsObserver* ui) {
  observers_->AddObserver(ui);
}

void MediaInternals::RemoveUI(MediaInternalsObserver* ui) {
  observers_->RemoveObserver(ui);
}

void MediaInternals::SendEverything() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &MediaInternals::SendUpdateOnIOThread,
          std::string(kSendEverythingFunction), &data_));
}

MediaInternals::MediaInternals()
    : observers_(new ObserverListThreadSafe<MediaInternalsObserver>()) {}

void MediaInternals::UpdateAudioStream(
    void* host, int32 render_view, int stream_id,
    const std::string& property, Value* value) {
  std::string stream = base::StringPrintf("audio_streams.%p.%d.%d",
                                          host, render_view, stream_id);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &MediaInternals::UpdateItemOnIOThread,
          std::string(kAudioUpdateFunction), stream, property, value));
}

void MediaInternals::UpdateItemOnIOThread(
    const std::string& update_fn, const std::string& id,
    const std::string& property, Value* value) {
  DictionaryValue* item_properties;
  if (!data_.GetDictionary(id, &item_properties)) {
    item_properties = new DictionaryValue();
    data_.Set(id, item_properties);
    item_properties->SetString("id", id);
  }
  item_properties->Set(property, value);
  SendUpdateOnIOThread(update_fn, item_properties);
}

void MediaInternals::SendUpdateOnIOThread(const std::string& function,
                                          Value* value) {
  scoped_ptr<std::vector<const Value*> > args(new std::vector<const Value*>());
  args->push_back(value);
  string16 update = WebUI::GetJavascriptCall(function, *args.get());
  observers_->Notify(&MediaInternalsObserver::OnUpdate, update);
}
