// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_AVDA_CODEC_ALLOCATOR_H_
#define MEDIA_GPU_AVDA_CODEC_ALLOCATOR_H_

#include <stddef.h>

#include <memory>

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/sys_info.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "media/base/android/media_drm_bridge_cdm_context.h"
#include "media/base/android/sdk_media_codec_bridge.h"
#include "media/base/media.h"
#include "media/base/surface_manager.h"
#include "media/base/video_codecs.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace media {

// For TaskRunnerFor. These are used as vector indices, so please update
// AVDACodecAllocator's constructor if you add / change them.
// TODO(watk): Hide this from AVDA now that we manage codec creation.
enum TaskType {
  // Task for an autodetected MediaCodec instance.
  AUTO_CODEC = 0,

  // Task for a software-codec-required MediaCodec.
  SW_CODEC = 1,

  // Special value to indicate "none".  This is not used as an array index. It
  // must, however, be positive to keep the enum unsigned.
  FAILED_CODEC = 99,
};

// Configuration info for MediaCodec.
// This is used to shuttle configuration info between threads without needing
// to worry about the lifetime of the AVDA instance.
class CodecConfig : public base::RefCountedThreadSafe<CodecConfig> {
 public:
  CodecConfig();

  // Codec type. Used when we configure media codec.
  VideoCodec codec_ = kUnknownVideoCodec;

  // Whether encryption scheme requires to use protected surface.
  bool needs_protected_surface_ = false;

  // The surface that MediaCodec is configured to output to.
  gl::ScopedJavaSurface surface_;

  int surface_id_ = SurfaceManager::kNoSurfaceID;

  // The MediaCrypto object is used in the MediaCodec.configure() in case of
  // an encrypted stream.
  MediaDrmBridgeCdmContext::JavaObjectPtr media_crypto_;

  // Initial coded size.  The actual size might change at any time, so this
  // is only a hint.
  gfx::Size initial_expected_coded_size_;

  // The type of allocation to use for this.  We use this to select the right
  // thread for construction / destruction, and to decide if we should
  // restrict the codec to be software only.
  TaskType task_type_;

  // Codec specific data (SPS and PPS for H264).
  std::vector<uint8_t> csd0_;
  std::vector<uint8_t> csd1_;

 protected:
  friend class base::RefCountedThreadSafe<CodecConfig>;
  virtual ~CodecConfig();

 private:
  DISALLOW_COPY_AND_ASSIGN(CodecConfig);
};

class AVDACodecAllocatorClient {
 public:
  // Called when the requested SurfaceView becomes available after a call to
  // AllocateSurface()
  virtual void OnSurfaceAvailable(bool success) = 0;

  // Called when the allocated surface is being destroyed. This must either
  // replace the surface with MediaCodec#setSurface, or release the MediaCodec
  // it's attached to. The client no longer owns the surface and doesn't
  // need to call DeallocateSurface();
  virtual void OnSurfaceDestroyed() = 0;

  // Called on the main thread when a new MediaCodec is configured.
  // |media_codec| will be null if configuration failed.
  virtual void OnCodecConfigured(
      std::unique_ptr<VideoCodecBridge> media_codec) = 0;

 protected:
  ~AVDACodecAllocatorClient() {}
};

// AVDACodecAllocator manages threads for allocating and releasing MediaCodec
// instances.  These activities can hang, depending on android version, due
// to mediaserver bugs.  AVDACodecAllocator detects these cases, and reports
// on them to allow software fallback if the HW path is hung up.
class MEDIA_GPU_EXPORT AVDACodecAllocator {
 public:
  static AVDACodecAllocator* Instance();

  // Called synchronously when the given surface is being destroyed on the
  // browser UI thread.
  void OnSurfaceDestroyed(int surface_id);

  // Make sure the construction thread is started for |client|.
  bool StartThread(AVDACodecAllocatorClient* client);

  void StopThread(AVDACodecAllocatorClient* client);

  // Return the task runner for tasks of type |type|.  If that thread failed
  // to start, then fall back to the GPU main thread.
  scoped_refptr<base::SingleThreadTaskRunner> TaskRunnerFor(TaskType task_type);

  // Returns true if the caller now owns the surface, or false if someone else
  // owns the surface. |client| will be notified when the surface is available
  // via OnSurfaceAvailable().
  bool AllocateSurface(AVDACodecAllocatorClient* client, int surface_id);

  // Relinquish ownership of the surface or stop waiting for it to be available.
  // The caller must guarantee that when calling this the surface is either no
  // longer attached to a MediaCodec, or the MediaCodec it was attached to is
  // was released with ReleaseMediaCodec().
  void DeallocateSurface(AVDACodecAllocatorClient* client, int surface_id);

  // Create and configure a MediaCodec synchronously.
  std::unique_ptr<VideoCodecBridge> CreateMediaCodecSync(
      scoped_refptr<CodecConfig> codec_config);

  // Create and configure a MediaCodec asynchronously. The result is delivered
  // via OnCodecConfigured().
  void CreateMediaCodecAsync(base::WeakPtr<AVDACodecAllocatorClient> client,
                             scoped_refptr<CodecConfig> codec_config);

  // Asynchronously release |media_codec| with the attached surface.
  // TODO(watk): Bundle the MediaCodec and surface together so you can't get
  // this pairing wrong.
  void ReleaseMediaCodec(std::unique_ptr<VideoCodecBridge> media_codec,
                         TaskType task_type,
                         int surface_id);

  // Returns a hint about whether the construction thread has hung for
  // |task_type|.  Note that if a thread isn't started, then we'll just return
  // "not hung", since it'll run on the current thread anyway.  The hang
  // detector will see no pending jobs in that case, so it's automatic.
  bool IsThreadLikelyHung(TaskType task_type);

  // Return true if and only if there is any AVDA registered.
  bool IsAnyRegisteredAVDA();

  // Return the task type to use for a new codec allocation, or FAILED_CODEC if
  // there is no thread that can support it.
  TaskType TaskTypeForAllocation();

  // Return a reference to the thread for unit tests.
  base::Thread& GetThreadForTesting(TaskType task_type);

 private:
  friend struct base::DefaultLazyInstanceTraits<AVDACodecAllocator>;
  friend class AVDACodecAllocatorTest;

  // Things that our unit test needs.  We guarantee that we'll access none of
  // it, from any thread, after we are destructed.
  struct TestInformation {
    TestInformation();
    ~TestInformation();
    // Optional clock source.
    std::unique_ptr<base::TickClock> tick_clock_;

    // Optional event that we'll signal when stopping the AUTO_CODEC thread.
    std::unique_ptr<base::WaitableEvent> stop_event_;
  };

  struct OwnerRecord {
    AVDACodecAllocatorClient* owner = nullptr;
    AVDACodecAllocatorClient* waiter = nullptr;
  };

  class HangDetector : public base::MessageLoop::TaskObserver {
   public:
    HangDetector(base::TickClock* tick_clock);
    void WillProcessTask(const base::PendingTask& pending_task) override;
    void DidProcessTask(const base::PendingTask& pending_task) override;
    bool IsThreadLikelyHung();

   private:
    base::Lock lock_;

    // Non-null when a task is currently running.
    base::TimeTicks task_start_time_;

    base::TickClock* tick_clock_;

    DISALLOW_COPY_AND_ASSIGN(HangDetector);
  };

  // Handy combination of a thread and hang detector for it.
  struct ThreadAndHangDetector {
    ThreadAndHangDetector(const std::string& name, base::TickClock* tick_clock)
        : thread(name), hang_detector(tick_clock) {}
    base::Thread thread;
    HangDetector hang_detector;
  };

  // |test_info| is owned by the unit test.
  AVDACodecAllocator(TestInformation* test_info = nullptr);
  ~AVDACodecAllocator();

  void OnMediaCodecAndSurfaceReleased(int surface_id);

  // Stop the thread indicated by |index|, then signal |event| if provided.
  void StopThreadTask(size_t index, base::WaitableEvent* event = nullptr);

  // All registered AVDAs.
  std::set<AVDACodecAllocatorClient*> clients_;

  // Indexed by surface id.
  std::map<int32_t, OwnerRecord> surface_owners_;

  // Waitable events for ongoing release tasks indexed by surface id so we can
  // wait on the codec release if the surface attached to it is being destroyed.
  std::map<int32_t, base::WaitableEvent> pending_codec_releases_;

  // Threads for each of TaskType.  They are started / stopped as avda instances
  // show and and request them.  The vector indicies must match TaskType.
  std::vector<ThreadAndHangDetector*> threads_;

  base::ThreadChecker thread_checker_;

  // Optional, used for unit testing.  We do not own this.
  TestInformation* test_info_;

  // For canceling pending StopThreadTask()s.
  base::WeakPtrFactory<AVDACodecAllocator> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(AVDACodecAllocator);
};

}  // namespace media

#endif  // MEDIA_GPU_AVDA_CODEC_ALLOCATOR_H_
