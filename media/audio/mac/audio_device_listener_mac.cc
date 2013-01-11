// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_device_listener_mac.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/mac/libdispatch_task_runner.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/message_loop.h"
#include "base/native_library.h"

namespace media {

// The following three variables are used for dynamically loading the required
// AudioObject(Add|Remove)PropertyListenerBlock functions on 10.7+ while keeping
// a 10.6 deployment target.  See http://crbug.com/158170 for details on why.
// TODO(dalecurtis): Remove once the deployment target is > 10.6.
static const char kCoreAudioFrameworkPath[] =
    "/System/Library/Frameworks/CoreAudio.framework/Versions/Current/CoreAudio";

typedef OSStatus(*AudioObjectPropertyListenerBlockT)(
    AudioObjectID inObjectID,
    const AudioObjectPropertyAddress* inAddress,
    dispatch_queue_t inDispatchQueue,
    CrAudioObjectPropertyListenerBlock inListener);

static AudioObjectPropertyListenerBlockT g_add_listener_block_func = NULL;
static AudioObjectPropertyListenerBlockT g_remove_listener_block_func = NULL;

// TaskObserver which guarantees that dispatch queue operations such as property
// listener callbacks are mutually exclusive to operations on the audio thread.
// TODO(dalecurtis): Instead we should replace the main thread with a dispatch
// queue.  See http://crbug.com/158170.
class ExclusiveDispatchQueueTaskObserver : public MessageLoop::TaskObserver {
 public:
  ExclusiveDispatchQueueTaskObserver()
      : property_listener_queue_(new base::mac::LibDispatchTaskRunner(
            "com.google.chrome.AudioPropertyListenerQueue")),
        queue_(property_listener_queue_->GetDispatchQueue()),
        message_loop_(MessageLoop::current()) {
    // If we're currently on the thread, fire the suspend operation so we don't
    // end up with an unbalanced resume.
    if (message_loop_->message_loop_proxy()->BelongsToCurrentThread())
      WillProcessTask(base::TimeTicks());

    message_loop_->AddTaskObserver(this);
  }

  virtual ~ExclusiveDispatchQueueTaskObserver() {
    message_loop_->RemoveTaskObserver(this);

    // If we're currently on the thread, fire the resume operation so we don't
    // end up with an unbalanced suspend.
    if (message_loop_->message_loop_proxy()->BelongsToCurrentThread())
      DidProcessTask(base::TimeTicks());

    // This will hang if any listeners are still registered with the queue.
    property_listener_queue_->Shutdown();
  }

  virtual void WillProcessTask(base::TimeTicks time_posted) OVERRIDE {
    // Issue a synchronous suspend operation.  Benchmarks on a retina 10.8.2
    // machine show this takes < 20us on average.  dispatch_suspend() is an
    // asynchronous operation so we need to issue it inside of a synchronous
    // block to ensure it completes before WillProccesTask() completes.
    dispatch_sync(queue_, ^{
        dispatch_suspend(queue_);
    });
  }

  virtual void DidProcessTask(base::TimeTicks time_posted) OVERRIDE {
    // Issue an asynchronous resume operation.  Benchmarks on a retina 10.8.2
    // machine show this takes < 10us on average.
    dispatch_resume(queue_);
  }

  dispatch_queue_t dispatch_queue() const {
    return queue_;
  }

 private:
  scoped_refptr<base::mac::LibDispatchTaskRunner> property_listener_queue_;
  const dispatch_queue_t queue_;
  MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ExclusiveDispatchQueueTaskObserver);
};

static bool LoadAudioObjectPropertyListenerBlockFunctions() {
  if (g_add_listener_block_func && g_remove_listener_block_func)
    return true;

  // Dynamically load required block functions.
  // TODO(dalecurtis): Remove once the deployment target is > 10.6.
  std::string error;
  base::NativeLibrary core_audio = base::LoadNativeLibrary(
      FilePath(kCoreAudioFrameworkPath), &error);
  if (!error.empty()) {
    LOG(ERROR) << "Could not open CoreAudio library: " << error;
    return false;
  }

  g_add_listener_block_func =
      reinterpret_cast<AudioObjectPropertyListenerBlockT>(
          base::GetFunctionPointerFromNativeLibrary(
              core_audio, "AudioObjectAddPropertyListenerBlock"));
  g_remove_listener_block_func =
      reinterpret_cast<AudioObjectPropertyListenerBlockT>(
          base::GetFunctionPointerFromNativeLibrary(
              core_audio, "AudioObjectRemovePropertyListenerBlock"));

  // If either function failed to load, skip listener registration.
  if (!g_add_listener_block_func || !g_remove_listener_block_func) {
    g_add_listener_block_func = NULL;
    g_remove_listener_block_func = NULL;
    return false;
  }

  return true;
}

// Property address to monitor for device changes.
const AudioObjectPropertyAddress
AudioDeviceListenerMac::kDeviceChangePropertyAddress = {
  kAudioHardwarePropertyDefaultOutputDevice,
  kAudioObjectPropertyScopeGlobal,
  kAudioObjectPropertyElementMaster
};

// Callback from the system when the default device changes; this must be called
// on the MessageLoop that created the AudioManager.
// static
OSStatus AudioDeviceListenerMac::OnDefaultDeviceChanged(
    AudioObjectID object, UInt32 num_addresses,
    const AudioObjectPropertyAddress addresses[], void* context) {
  if (object != kAudioObjectSystemObject)
    return noErr;

  for (UInt32 i = 0; i < num_addresses; ++i) {
    if (addresses[i].mSelector == kDeviceChangePropertyAddress.mSelector &&
        addresses[i].mScope == kDeviceChangePropertyAddress.mScope &&
        addresses[i].mElement == kDeviceChangePropertyAddress.mElement &&
        context) {
      static_cast<AudioDeviceListenerMac*>(context)->listener_cb_.Run();
      break;
    }
  }

  return noErr;
}

AudioDeviceListenerMac::AudioDeviceListenerMac(
    const base::Closure& listener_cb) {
  if (!LoadAudioObjectPropertyListenerBlockFunctions())
    return;

  // Device changes are hard, lets go shopping!  Sadly OSX does not handle
  // property listener callbacks in a thread safe manner.  On 10.6 we can set
  // kAudioHardwarePropertyRunLoop to account for this.  On 10.7 this is broken
  // and we need to create our dispatch queue to drive callbacks.  However code
  // running on the dispatch queue must be mutually exclusive with code running
  // on the audio thread.   ExclusiveDispatchQueueTaskObserver works around this
  // by pausing and resuming the dispatch queue before and after each pumped
  // task.  This is not ideal and long term we should replace the audio thread
  // on OSX with a dispatch queue.  See http://crbug.com/158170 for discussion.
  // TODO(dalecurtis): Does not fix the cases where GetAudioHardwareSampleRate()
  // and GetAudioInputHardwareSampleRate() are called by the browser process.
  // These are one time events due to renderer side cache and thus unlikely to
  // occur at the same time as a device callback.  Should be fixed along with
  // http://crbug.com/137326 using a forced PostTask.
  OSStatus result;
  if (base::mac::IsOSLionOrLater()) {
    task_observer_.reset(new ExclusiveDispatchQueueTaskObserver());
    listener_block_ = Block_copy(^(
        UInt32 num_addresses, const AudioObjectPropertyAddress addresses[]) {
            OnDefaultDeviceChanged(
                kAudioObjectSystemObject, num_addresses, addresses, this);
        });
    result = g_add_listener_block_func(
        kAudioObjectSystemObject, &kDeviceChangePropertyAddress,
        task_observer_->dispatch_queue(), listener_block_);
  } else {
    const AudioObjectPropertyAddress kRunLoopAddress = {
      kAudioHardwarePropertyRunLoop,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMaster
    };

    CFRunLoopRef run_loop = CFRunLoopGetCurrent();
    UInt32 size = sizeof(CFRunLoopRef);
    result = AudioObjectSetPropertyData(
        kAudioObjectSystemObject, &kRunLoopAddress, 0, 0, size, &run_loop);
    if (result != noErr) {
      OSSTATUS_DLOG(ERROR, result) << "Failed to set property listener thread.";
      return;
    }

    result = AudioObjectAddPropertyListener(
        kAudioObjectSystemObject, &kDeviceChangePropertyAddress,
        &AudioDeviceListenerMac::OnDefaultDeviceChanged, this);
  }

  if (result != noErr) {
    task_observer_.reset();
    OSSTATUS_DLOG(ERROR, result) << "AudioObjectAddPropertyListener() failed!";
    return;
  }

  listener_cb_ = listener_cb;
}

AudioDeviceListenerMac::~AudioDeviceListenerMac() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (listener_cb_.is_null())
    return;

  if (task_observer_) {
    OSStatus result = g_remove_listener_block_func(
        kAudioObjectSystemObject, &kDeviceChangePropertyAddress,
        task_observer_->dispatch_queue(), listener_block_);

    OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
        << "AudioObjectRemovePropertyListenerBlock() failed!";

    // Task observer will wait for all outstanding callbacks to complete.
    task_observer_.reset();
    Block_release(listener_block_);
    return;
  }

  // Since we're running on the same CFRunLoop, there can be no outstanding
  // callbacks in flight.
  OSStatus result = AudioObjectRemovePropertyListener(
      kAudioObjectSystemObject, &kDeviceChangePropertyAddress,
      &AudioDeviceListenerMac::OnDefaultDeviceChanged, this);
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "AudioObjectRemovePropertyListener() failed!";
}

}  // namespace media
