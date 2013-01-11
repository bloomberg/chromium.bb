// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_

#include <CoreAudio/AudioHardware.h>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "media/base/media_export.h"

namespace media {

// TODO(dalecurtis): Use the standard AudioObjectPropertyListenerBlock when
// the deployment target is > 10.6.
typedef void (^CrAudioObjectPropertyListenerBlock)(
    UInt32 num_addresses, const AudioObjectPropertyAddress addresses[]);

class ExclusiveDispatchQueueTaskObserver;

// AudioDeviceListenerMac facilitates thread safe execution of device listener
// callbacks issued via CoreAudio.  On 10.6 this means correctly binding the
// CFRunLoop driving callbacks to the CFRunLoop AudioDeviceListenerMac is
// constructed on.  On 10.7 this means creating a dispatch queue to drive
// callbacks and modifying the message loop AudioDeviceListenerMac to ensure
// message loop tasks and dispatch queue tasks are executed in a mutually
// exclusive manner.  See http://crbug.com/158170 for more discussion.
class MEDIA_EXPORT AudioDeviceListenerMac {
 public:
  // |listener_cb| will be called when a device change occurs; it's a permanent
  // callback and must outlive AudioDeviceListenerMac.
  explicit AudioDeviceListenerMac(const base::Closure& listener_cb);
  ~AudioDeviceListenerMac();

 private:
  friend class AudioDeviceListenerMacTest;
  static const AudioObjectPropertyAddress kDeviceChangePropertyAddress;

  static OSStatus OnDefaultDeviceChanged(
      AudioObjectID object, UInt32 num_addresses,
      const AudioObjectPropertyAddress addresses[], void* context);

  base::Closure listener_cb_;
  CrAudioObjectPropertyListenerBlock listener_block_;
  scoped_ptr<ExclusiveDispatchQueueTaskObserver> task_observer_;

  // AudioDeviceListenerMac must be constructed and destructed on the same
  // thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AudioDeviceListenerMac);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_
