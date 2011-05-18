// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_MACOSX)
#include <signal.h>
#include <unistd.h>
#endif  // OS_MACOSX

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/i18n/rtl.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/system_monitor/system_monitor.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "content/common/content_counters.h"
#include "content/common/content_switches.h"
#include "content/common/main_function_params.h"
#include "content/common/hi_res_timer_manager.h"
#include "content/common/pepper_plugin_registry.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_thread.h"
#include "content/renderer/renderer_main_platform_delegate.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_MACOSX)
#include <Carbon/Carbon.h>  // TISCreateInputSourceList

#include "base/sys_info.h"
#include "third_party/mach_override/mach_override.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#endif  // OS_MACOSX

#if defined(OS_MACOSX)
namespace {

CFArrayRef ChromeTISCreateInputSourceList(
   CFDictionaryRef properties,
   Boolean includeAllInstalled) {
  CFTypeRef values[] = { CFSTR("") };
  return CFArrayCreate(
      kCFAllocatorDefault, values, arraysize(values), &kCFTypeArrayCallBacks);
}

void InstallFrameworkHacks() {
  int32 os_major, os_minor, os_bugfix;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major, &os_minor, &os_bugfix);

  // See http://crbug.com/31225
  // TODO: Don't do this on newer OS X revisions that have a fix for
  // http://openradar.appspot.com/radar?id=1156410
  if (os_major == 10 && os_minor >= 6) {
    // Chinese Handwriting was introduced in 10.6. Since doing this override
    // regresses page cycler memory usage on 10.5, don't do the unnecessary
    // override there.
    mach_error_t err = mach_override_ptr(
        (void*)&TISCreateInputSourceList,
        (void*)&ChromeTISCreateInputSourceList,
        NULL);
    CHECK_EQ(err_none, err);
  }
}

}  // namespace
#endif  // OS_MACOSX

// This function provides some ways to test crash and assertion handling
// behavior of the renderer.
static void HandleRendererErrorTestParameters(const CommandLine& command_line) {
  // This parameter causes an assertion.
  if (command_line.HasSwitch(switches::kRendererAssertTest)) {
    DCHECK(false);
  }


#if !defined(OFFICIAL_BUILD)
  // This parameter causes an assertion too.
  if (command_line.HasSwitch(switches::kRendererCheckFalseTest)) {
    CHECK(false);
  }
#endif  // !defined(OFFICIAL_BUILD)


  // This parameter causes a null pointer crash (crash reporter trigger).
  if (command_line.HasSwitch(switches::kRendererCrashTest)) {
    int* bad_pointer = NULL;
    *bad_pointer = 0;
  }

  if (command_line.HasSwitch(switches::kRendererStartupDialog)) {
    ChildProcess::WaitForDebugger("Renderer");
  }
}

// This is a simplified version of the browser Jankometer, which measures
// the processing time of tasks on the render thread.
class RendererMessageLoopObserver : public MessageLoop::TaskObserver {
 public:
  RendererMessageLoopObserver()
      : process_times_(base::Histogram::FactoryGet(
            "Chrome.ProcMsgL RenderThread",
            1, 3600000, 50, base::Histogram::kUmaTargetedHistogramFlag)) {}
  virtual ~RendererMessageLoopObserver() {}

  virtual void WillProcessTask(base::TimeTicks time_posted) {
    begin_process_message_ = base::TimeTicks::Now();
  }

  virtual void DidProcessTask(base::TimeTicks time_posted) {
    if (!begin_process_message_.is_null())
      process_times_->AddTime(base::TimeTicks::Now() - begin_process_message_);
  }

 private:
  base::TimeTicks begin_process_message_;
  base::Histogram* const process_times_;
  DISALLOW_COPY_AND_ASSIGN(RendererMessageLoopObserver);
};

// mainline routine for running as the Renderer process
int RendererMain(const MainFunctionParams& parameters) {
  TRACE_EVENT_BEGIN_ETW("RendererMain", 0, "");

  const CommandLine& parsed_command_line = parameters.command_line_;
  base::mac::ScopedNSAutoreleasePool* pool = parameters.autorelease_pool_;

#if defined(OS_MACOSX)
  InstallFrameworkHacks();
#endif  // OS_MACOSX

#if defined(OS_CHROMEOS)
  // As Zygote process starts up earlier than browser process gets its own
  // locale (at login time for Chrome OS), we have to set the ICU default
  // locale for renderer process here.
  // ICU locale will be used for fallback font selection etc.
  if (parsed_command_line.HasSwitch(switches::kLang)) {
    const std::string locale =
        parsed_command_line.GetSwitchValueASCII(switches::kLang);
    base::i18n::SetICUDefaultLocale(locale);
  }
#endif

  // This function allows pausing execution using the --renderer-startup-dialog
  // flag allowing us to attach a debugger.
  // Do not move this function down since that would mean we can't easily debug
  // whatever occurs before it.
  HandleRendererErrorTestParameters(parsed_command_line);

  RendererMainPlatformDelegate platform(parameters);

  base::StatsScope<base::StatsCounterTimer>
      startup_timer(content::Counters::renderer_main());

  RendererMessageLoopObserver task_observer;
#if defined(OS_MACOSX)
  // As long as we use Cocoa in the renderer (for the forseeable future as of
  // now; see http://crbug.com/13890 for info) we need to have a UI loop.
  MessageLoop main_message_loop(MessageLoop::TYPE_UI);
#else
  // The main message loop of the renderer services doesn't have IO or UI tasks,
  // unless in-process-plugins is used.
  MessageLoop main_message_loop(RenderProcessImpl::InProcessPlugins() ?
              MessageLoop::TYPE_UI : MessageLoop::TYPE_DEFAULT);
#endif
  main_message_loop.AddTaskObserver(&task_observer);

  base::PlatformThread::SetName("CrRendererMain");

  base::SystemMonitor system_monitor;
  HighResolutionTimerManager hi_res_timer_manager;

  platform.PlatformInitialize();

  bool no_sandbox = parsed_command_line.HasSwitch(switches::kNoSandbox);
  platform.InitSandboxTests(no_sandbox);

  // Initialize histogram statistics gathering system.
  // Don't create StatisticsRecorder in the single process mode.
  scoped_ptr<base::StatisticsRecorder> statistics;
  if (!base::StatisticsRecorder::IsActive()) {
    statistics.reset(new base::StatisticsRecorder());
  }

  // Initialize statistical testing infrastructure.  We set client_id to the
  // empty string to disallow the renderer process from creating its own
  // one-time randomized trials; they should be created in the browser process.
  base::FieldTrialList field_trial(EmptyString());
  // Ensure any field trials in browser are reflected into renderer.
  if (parsed_command_line.HasSwitch(switches::kForceFieldTestNameAndValue)) {
    std::string persistent = parsed_command_line.GetSwitchValueASCII(
        switches::kForceFieldTestNameAndValue);
    bool ret = field_trial.CreateTrialsInChildProcess(persistent);
    DCHECK(ret);
  }

  // Load pepper plugins before engaging the sandbox.
  PepperPluginRegistry::GetInstance();

  {
#if !defined(OS_LINUX)
    // TODO(markus): Check if it is OK to unconditionally move this
    // instruction down.
    RenderProcessImpl render_process;
    render_process.set_main_thread(new RenderThread());
#endif
    bool run_loop = true;
    if (!no_sandbox) {
      run_loop = platform.EnableSandbox();
    } else {
      LOG(ERROR) << "Running without renderer sandbox";
    }
#if defined(OS_LINUX)
    RenderProcessImpl render_process;
    render_process.set_main_thread(new RenderThread());
#endif

    platform.RunSandboxTests();

    startup_timer.Stop();  // End of Startup Time Measurement.

    if (run_loop) {
      if (pool)
        pool->Recycle();
      TRACE_EVENT_BEGIN_ETW("RendererMain.START_MSG_LOOP", 0, 0);
      MessageLoop::current()->Run();
      TRACE_EVENT_END_ETW("RendererMain.START_MSG_LOOP", 0, 0);
    }
  }
  platform.PlatformUninitialize();
  TRACE_EVENT_END_ETW("RendererMain", 0, "");
  return 0;
}
