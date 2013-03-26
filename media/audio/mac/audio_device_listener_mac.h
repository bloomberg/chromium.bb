// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_

#include <CoreAudio/AudioHardware.h>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/scoped_native_library.h"
#include "base/threading/thread_checker.h"
#include "media/base/media_export.h"

namespace media {

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

  // Attempts to dynamically load the property listener block functions.  Sets
  // |core_audio_lib_|, |add_listener_block_func_|, and
  // |remove_listener_block_func_|.  Returns false on failure.  If successful,
  // the unload function must be called to avoid memory leaks.
  bool LoadAudioObjectPropertyListenerBlockFunctions();

  base::Closure listener_cb_;
  scoped_ptr<ExclusiveDispatchQueueTaskObserver> task_observer_;
  base::ScopedNativeLibrary core_audio_lib_;

  // TODO(dalecurtis): Use the standard AudioObjectPropertyListenerBlock when
  // the deployment target is > 10.6.
  typedef void (^CrAudioObjectPropertyListenerBlock)(
      UInt32 inNumberAddresses, const AudioObjectPropertyAddress inAddresses[]);
  CrAudioObjectPropertyListenerBlock listener_block_;

  // Variables for storing dynamically loaded block functions.
  typedef OSStatus(*AudioObjectPropertyListenerBlockT)(
      AudioObjectID inObjectID,
      const AudioObjectPropertyAddress* inAddress,
      dispatch_queue_t inDispatchQueue,
      CrAudioObjectPropertyListenerBlock inListener);
  AudioObjectPropertyListenerBlockT add_listener_block_func_;
  AudioObjectPropertyListenerBlockT remove_listener_block_func_;

  // AudioDeviceListenerMac must be constructed and destructed on the same
  // thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AudioDeviceListenerMac);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_
