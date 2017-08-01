// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_MAIN_LOOP_H_
#define CONTENT_BROWSER_BROWSER_MAIN_LOOP_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/browser/browser_process_sub_thread.h"
#include "content/public/browser/browser_main_runner.h"

#if defined(USE_AURA)
namespace aura {
class Env;
}
#endif

namespace base {
class CommandLine;
class FilePath;
class HighResolutionTimerManager;
class MemoryPressureMonitor;
class MessageLoop;
class PowerMonitor;
class SingleThreadTaskRunner;
class SystemMonitor;
namespace trace_event {
class TraceEventSystemStatsMonitor;
}  // namespace trace_event
}  // namespace base

namespace discardable_memory {
class DiscardableSharedMemoryManager;
}

namespace gpu {
class GpuChannelEstablishFactory;
}

namespace media {
class AudioManager;
class AudioSystem;
#if defined(OS_WIN)
class SystemMessageWindowWin;
#elif defined(OS_LINUX) && defined(USE_UDEV)
class DeviceMonitorLinux;
#endif
class UserInputMonitor;
#if defined(OS_MACOSX)
class DeviceMonitorMac;
#endif
}  // namespace media

namespace memory_instrumentation {
class CoordinatorImpl;
}  // memory_instrumentation

namespace midi {
class MidiService;
}  // namespace midi

namespace mojo {
namespace edk {
class ScopedIPCSupport;
}  // namespace edk
}  // namespace mojo

namespace net {
class NetworkChangeNotifier;
}  // namespace net

#if defined(USE_OZONE)
namespace gfx {
class ClientNativePixmapFactory;
}  // namespace gfx
#endif

namespace viz {
class FrameSinkManagerImpl;
class HostFrameSinkManager;
}

namespace content {
class BrowserMainParts;
class BrowserOnlineStateObserver;
class BrowserThreadImpl;
class LoaderDelegateImpl;
class MediaStreamManager;
class ResourceDispatcherHostImpl;
class SaveFileManager;
class ServiceManagerContext;
class SpeechRecognitionManagerImpl;
class StartupTaskRunner;
class SwapMetricsDriver;
struct MainFunctionParams;

#if defined(OS_ANDROID)
class ScreenOrientationDelegate;
#endif

#if defined(USE_X11) && !defined(OS_CHROMEOS)
namespace internal {
class GpuDataManagerVisualProxy;
}
#endif

// Implements the main browser loop stages called from BrowserMainRunner.
// See comments in browser_main_parts.h for additional info.
class CONTENT_EXPORT BrowserMainLoop {
 public:
  // Returns the current instance. This is used to get access to the getters
  // that return objects which are owned by this class.
  static BrowserMainLoop* GetInstance();

  explicit BrowserMainLoop(const MainFunctionParams& parameters);
  virtual ~BrowserMainLoop();

  void Init();

  void EarlyInitialization();

  // Initializes the toolkit. Returns whether the toolkit initialization was
  // successful or not.
  bool InitializeToolkit();

  void PreMainMessageLoopStart();
  void MainMessageLoopStart();
  void PostMainMessageLoopStart();
  void PreShutdown();

  // Create and start running the tasks we need to complete startup. Note that
  // this can be called more than once (currently only on Android) if we get a
  // request for synchronous startup while the tasks created by asynchronous
  // startup are still running.
  void CreateStartupTasks();

  // Perform the default message loop run logic.
  void RunMainMessageLoopParts();

  // Performs the shutdown sequence, starting with PostMainMessageLoopRun
  // through stopping threads to PostDestroyThreads.
  void ShutdownThreadsAndCleanUp();

  int GetResultCode() const { return result_code_; }

  media::AudioManager* audio_manager() const { return audio_manager_.get(); }
  media::AudioSystem* audio_system() const { return audio_system_.get(); }
  MediaStreamManager* media_stream_manager() const {
    return media_stream_manager_.get();
  }
  media::UserInputMonitor* user_input_monitor() const {
    return user_input_monitor_.get();
  }
  discardable_memory::DiscardableSharedMemoryManager*
  discardable_shared_memory_manager() const {
    return discardable_shared_memory_manager_.get();
  }
  midi::MidiService* midi_service() const { return midi_service_.get(); }

  bool is_tracing_startup_for_duration() const {
    return is_tracing_startup_for_duration_;
  }

  const base::FilePath& startup_trace_file() const {
    return startup_trace_file_;
  }

  // Returns the task runner for tasks that that are critical to producing a new
  // CompositorFrame on resize. On Mac this will be the task runner provided by
  // WindowResizeHelperMac, on other platforms it will just be the thread task
  // runner.
  scoped_refptr<base::SingleThreadTaskRunner> GetResizeTaskRunner();

  gpu::GpuChannelEstablishFactory* gpu_channel_establish_factory() const;

#if defined(OS_ANDROID)
  void SynchronouslyFlushStartupTasks();
#endif  // OS_ANDROID

#if !defined(OS_ANDROID)
  // TODO(fsamuel): We should find an object to own HostFrameSinkManager on all
  // platforms including Android. See http://crbug.com/732507.
  viz::HostFrameSinkManager* host_frame_sink_manager() const {
    return host_frame_sink_manager_.get();
  }

  // TODO(crbug.com/657959): This will be removed once there are no users, as
  // SurfaceManager is being moved out of process.
  viz::FrameSinkManagerImpl* GetFrameSinkManager() const;
#endif

  void StopStartupTracingTimer();

#if defined(OS_MACOSX) && !defined(OS_IOS)
  media::DeviceMonitorMac* device_monitor_mac() const {
    return device_monitor_mac_.get();
  }
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserMainLoopTest, CreateThreadsInSingleProcess);

  void InitializeMainThread();

  // Called just before creating the threads
  int PreCreateThreads();

  // Create all secondary threads.
  int CreateThreads();

  // Called right after the browser threads have been started.
  int BrowserThreadsStarted();

  int PreMainMessageLoopRun();

  void MainMessageLoopRun();

  void InitializeMojo();
  base::FilePath GetStartupTraceFileName(
      const base::CommandLine& command_line) const;
  void InitStartupTracingForDuration(const base::CommandLine& command_line);
  void EndStartupTracing();

  void CreateAudioManager();

  bool UsingInProcessGpu() const;

  void InitializeMemoryManagementComponent();

  // Quick reference for initialization order:
  // Constructor
  // Init()
  // EarlyInitialization()
  // InitializeToolkit()
  // PreMainMessageLoopStart()
  // MainMessageLoopStart()
  //   InitializeMainThread()
  // PostMainMessageLoopStart()
  //   InitStartupTracingForDuration()
  // CreateStartupTasks()
  //   PreCreateThreads()
  //   CreateThreads()
  //   BrowserThreadsStarted()
  //   PreMainMessageLoopRun()

  // Members initialized on construction ---------------------------------------
  const MainFunctionParams& parameters_;
  const base::CommandLine& parsed_command_line_;
  int result_code_;
  bool created_threads_;  // True if the non-UI threads were created.
  bool is_tracing_startup_for_duration_;

  // Members initialized in |MainMessageLoopStart()| ---------------------------
  std::unique_ptr<base::MessageLoop> main_message_loop_;

  // Members initialized in |PostMainMessageLoopStart()| -----------------------
  std::unique_ptr<base::SystemMonitor> system_monitor_;
  std::unique_ptr<base::PowerMonitor> power_monitor_;
  std::unique_ptr<base::HighResolutionTimerManager> hi_res_timer_manager_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;

  // Per-process listener for online state changes.
  std::unique_ptr<BrowserOnlineStateObserver> online_state_observer_;

  std::unique_ptr<base::trace_event::TraceEventSystemStatsMonitor>
      system_stats_monitor_;

#if defined(USE_AURA)
  std::unique_ptr<aura::Env> env_;
#endif

#if defined(OS_ANDROID)
  // Android implementation of ScreenOrientationDelegate
  std::unique_ptr<ScreenOrientationDelegate> screen_orientation_delegate_;
#endif

  // Members initialized in |InitStartupTracingForDuration()| ------------------
  base::FilePath startup_trace_file_;

  // This timer initiates trace file saving.
  base::OneShotTimer startup_trace_timer_;

  // Members initialized in |Init()| -------------------------------------------
  // Destroy |parts_| before |main_message_loop_| (required) and before other
  // classes constructed in content (but after |main_thread_|).
  std::unique_ptr<BrowserMainParts> parts_;

  // Members initialized in |InitializeMainThread()| ---------------------------
  // This must get destroyed before other threads that are created in |parts_|.
  std::unique_ptr<BrowserThreadImpl> main_thread_;

  // Members initialized in |CreateStartupTasks()| -----------------------------
  std::unique_ptr<StartupTaskRunner> startup_task_runner_;

  // Members initialized in |PreCreateThreads()| -------------------------------
  // Torn down in ShutdownThreadsAndCleanUp.
  std::unique_ptr<base::MemoryPressureMonitor> memory_pressure_monitor_;
  std::unique_ptr<SwapMetricsDriver> swap_metrics_driver_;
#if defined(USE_X11) && !(OS_CHROMEOS)
  std::unique_ptr<internal::GpuDataManagerVisualProxy>
      gpu_data_manager_visual_proxy_;
#endif

  // Members initialized in |CreateThreads()| ----------------------------------
  // Only the IO thread is a real thread by default, other BrowserThreads are
  // redirected to TaskScheduler under the hood.
  std::unique_ptr<BrowserProcessSubThread> io_thread_;
#if defined(OS_ANDROID)
  // On Android, the PROCESS_LAUNCHER thread is handled by Java,
  // |process_launcher_thread_| is merely a proxy to the real message loop.
  std::unique_ptr<BrowserProcessSubThread> process_launcher_thread_;
#elif defined(OS_WIN)
  // TaskScheduler doesn't support async I/O on Windows as CACHE thread is
  // the only user and this use case is going away in
  // https://codereview.chromium.org/2216583003/.
  // TODO(gavinp): Remove this (and thus enable redirection of the CACHE thread
  // on Windows) once that CL lands.
  std::unique_ptr<BrowserProcessSubThread> cache_thread_;
#endif

  // Members initialized in |BrowserThreadsStarted()| --------------------------
  std::unique_ptr<ServiceManagerContext> service_manager_context_;
  std::unique_ptr<mojo::edk::ScopedIPCSupport> mojo_ipc_support_;

  // |user_input_monitor_| has to outlive |audio_manager_|, so declared first.
  std::unique_ptr<media::UserInputMonitor> user_input_monitor_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  // Calls to |audio_system_| must not be posted to the audio thread if it
  // differs from the UI one. See http://crbug.com/705455.
  std::unique_ptr<media::AudioSystem> audio_system_;

  std::unique_ptr<midi::MidiService> midi_service_;

  // Must be deleted on the IO thread.
  std::unique_ptr<SpeechRecognitionManagerImpl> speech_recognition_manager_;

#if defined(OS_WIN)
  std::unique_ptr<media::SystemMessageWindowWin> system_message_window_;
#elif defined(OS_LINUX) && defined(USE_UDEV)
  std::unique_ptr<media::DeviceMonitorLinux> device_monitor_linux_;
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  std::unique_ptr<media::DeviceMonitorMac> device_monitor_mac_;
#endif
#if defined(USE_OZONE)
  std::unique_ptr<gfx::ClientNativePixmapFactory> client_native_pixmap_factory_;
#endif

  std::unique_ptr<LoaderDelegateImpl> loader_delegate_;
  std::unique_ptr<ResourceDispatcherHostImpl> resource_dispatcher_host_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  std::unique_ptr<discardable_memory::DiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;
  scoped_refptr<SaveFileManager> save_file_manager_;
  std::unique_ptr<memory_instrumentation::CoordinatorImpl>
      memory_instrumentation_coordinator_;
#if !defined(OS_ANDROID)
  std::unique_ptr<viz::HostFrameSinkManager> host_frame_sink_manager_;
  // This is owned here so that SurfaceManager will be accessible in process
  // when display is in the same process. Other than using SurfaceManager,
  // access to |in_process_frame_sink_manager_| should happen via
  // |host_frame_sink_manager_| instead which uses Mojo. See
  // http://crbug.com/657959.
  std::unique_ptr<viz::FrameSinkManagerImpl> frame_sink_manager_impl_;
#endif

  // DO NOT add members here. Add them to the right categories above.

  DISALLOW_COPY_AND_ASSIGN(BrowserMainLoop);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_MAIN_LOOP_H_
