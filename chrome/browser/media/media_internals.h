// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_INTERNALS_H_
#define CHROME_BROWSER_MEDIA_MEDIA_INTERNALS_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "base/values.h"
#include "content/browser/renderer_host/media/media_observer.h"

class MediaInternalsObserver;

// This class stores information about currently active media.
class MediaInternals : public MediaObserver {
 public:
  virtual ~MediaInternals();

  // MediaObserver implementation. These are callable from any thread:
  virtual void OnSetAudioStreamPlaying(void* host, int32 render_view,
                                       int stream_id, bool playing);
  virtual void OnSetAudioStreamVolume(void* host, int32 render_view,
                                      int stream_id, double volume);

  // Methods for observers. Called from the observer's own thread:
  // UIs should add themselves on construction and remove themselves
  // on destruction.
  void AddUI(MediaInternalsObserver* ui);
  void RemoveUI(MediaInternalsObserver* ui);
  void SendEverything();

 private:
  friend class IOThread;
  friend class MediaInternalsTest;

  MediaInternals();

  // Sets |property| of an audio stream to |value| and notifies observers.
  // (host, render_view, stream_id) is a unique id for the audio stream.
  // |host| will never be dereferenced.
  void UpdateAudioStream(void* host, int32 render_view, int stream_id,
                         const std::string& property, Value* value);

  // Sets data_.id.property = value and notifies attached UIs using update_fn.
  // id may be any depth, e.g. "video.decoders.1.2.3"
  void UpdateItemOnIOThread(const std::string& update_fn, const std::string& id,
                            const std::string& property, Value* value);

  // Calls javascript |function|(|value|) on each attached UI.
  void SendUpdateOnIOThread(const std::string& function, Value* value);

  static MediaInternals* instance_;
  DictionaryValue data_;
  scoped_refptr<ObserverListThreadSafe<MediaInternalsObserver> > observers_;

  DISALLOW_COPY_AND_ASSIGN(MediaInternals);
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(MediaInternals);

#endif  // CHROME_BROWSER_MEDIA_MEDIA_INTERNALS_H_
