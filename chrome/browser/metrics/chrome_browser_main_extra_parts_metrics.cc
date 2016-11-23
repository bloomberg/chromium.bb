// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_browser_main_extra_parts_metrics.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/sys_info.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/mac/bluetooth_utility.h"
#include "chrome/browser/shell_integration.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/touch/touch_device.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/screen.h"
#include "ui/events/event_switches.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/metrics/first_web_contents_profiler.h"
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID) && defined(__arm__)
#include <cpu-features.h>
#endif  // defined(OS_ANDROID) && defined(__arm__)

#if !defined(OS_ANDROID)
#include "chrome/browser/metrics/tab_usage_recorder.h"
#endif  // !defined(OS_ANDROID)

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include <gnu/libc-version.h>

#include "base/version.h"
#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

#if defined(USE_OZONE) || defined(USE_X11)
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/input_device_manager.h"
#endif  // defined(USE_OZONE) || defined(USE_X11)

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/installer/util/google_update_settings.h"
#endif  // defined(OS_WIN)

namespace {

enum UMALinuxGlibcVersion {
  UMA_LINUX_GLIBC_NOT_PARSEABLE,
  UMA_LINUX_GLIBC_UNKNOWN,
  UMA_LINUX_GLIBC_2_11,
  // To log newer versions, just update tools/metrics/histograms/histograms.xml.
};

enum UMALinuxWindowManager {
  UMA_LINUX_WINDOW_MANAGER_OTHER,
  UMA_LINUX_WINDOW_MANAGER_BLACKBOX,
  UMA_LINUX_WINDOW_MANAGER_CHROME_OS,  // Deprecated.
  UMA_LINUX_WINDOW_MANAGER_COMPIZ,
  UMA_LINUX_WINDOW_MANAGER_ENLIGHTENMENT,
  UMA_LINUX_WINDOW_MANAGER_ICE_WM,
  UMA_LINUX_WINDOW_MANAGER_KWIN,
  UMA_LINUX_WINDOW_MANAGER_METACITY,
  UMA_LINUX_WINDOW_MANAGER_MUFFIN,
  UMA_LINUX_WINDOW_MANAGER_MUTTER,
  UMA_LINUX_WINDOW_MANAGER_OPENBOX,
  UMA_LINUX_WINDOW_MANAGER_XFWM4,
  UMA_LINUX_WINDOW_MANAGER_AWESOME,
  UMA_LINUX_WINDOW_MANAGER_I3,
  UMA_LINUX_WINDOW_MANAGER_ION3,
  UMA_LINUX_WINDOW_MANAGER_MATCHBOX,
  UMA_LINUX_WINDOW_MANAGER_NOTION,
  UMA_LINUX_WINDOW_MANAGER_QTILE,
  UMA_LINUX_WINDOW_MANAGER_RATPOISON,
  UMA_LINUX_WINDOW_MANAGER_STUMPWM,
  UMA_LINUX_WINDOW_MANAGER_WMII,
  UMA_LINUX_WINDOW_MANAGER_FLUXBOX,
  UMA_LINUX_WINDOW_MANAGER_XMONAD,
  UMA_LINUX_WINDOW_MANAGER_UNNAMED,
  // NOTE: Append new window managers to the list above this line (i.e. don't
  // renumber) and update LinuxWindowManagerName in
  // tools/metrics/histograms/histograms.xml accordingly.
  UMA_LINUX_WINDOW_MANAGER_COUNT
};

enum UMATouchEventsState {
  UMA_TOUCH_EVENTS_ENABLED,
  UMA_TOUCH_EVENTS_AUTO_ENABLED,
  UMA_TOUCH_EVENTS_AUTO_DISABLED,
  UMA_TOUCH_EVENTS_DISABLED,
  // NOTE: Add states only immediately above this line. Make sure to
  // update the enum list in tools/metrics/histograms/histograms.xml
  // accordingly.
  UMA_TOUCH_EVENTS_STATE_COUNT
};

#if defined(OS_ANDROID) && defined(__arm__)
enum UMAAndroidArmFpu {
  UMA_ANDROID_ARM_FPU_VFPV3_D16,  // The ARM CPU only supports vfpv3-d16.
  UMA_ANDROID_ARM_FPU_NEON,       // The Arm CPU supports NEON.
  UMA_ANDROID_ARM_FPU_COUNT
};
#endif  // defined(OS_ANDROID) && defined(__arm__)

void RecordMicroArchitectureStats() {
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  base::CPU::IntelMicroArchitecture arch = cpu.GetIntelMicroArchitecture();
  UMA_HISTOGRAM_ENUMERATION("Platform.IntelMaxMicroArchitecture", arch,
                            base::CPU::MAX_INTEL_MICRO_ARCHITECTURE);
#endif  // defined(ARCH_CPU_X86_FAMILY)
#if defined(OS_ANDROID) && defined(__arm__)
  // Detect NEON support.
  // TODO(fdegans): Remove once non-NEON support has been removed.
  if (android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) {
    UMA_HISTOGRAM_ENUMERATION("Android.ArmFpu",
                              UMA_ANDROID_ARM_FPU_NEON,
                              UMA_ANDROID_ARM_FPU_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Android.ArmFpu",
                              UMA_ANDROID_ARM_FPU_VFPV3_D16,
                              UMA_ANDROID_ARM_FPU_COUNT);
  }
#endif  // defined(OS_ANDROID) && defined(__arm__)
  UMA_HISTOGRAM_SPARSE_SLOWLY("Platform.LogicalCpuCount",
                              base::SysInfo::NumberOfProcessors());
}

// Called on the blocking pool some time after startup to avoid slowing down
// startup with metrics that aren't trivial to compute.
void RecordStartupMetricsOnBlockingPool() {
#if defined(OS_WIN)
  GoogleUpdateSettings::RecordChromeUpdatePolicyHistograms();

  const base::win::OSInfo& os_info = *base::win::OSInfo::GetInstance();
  UMA_HISTOGRAM_ENUMERATION("Windows.GetVersionExVersion", os_info.version(),
                            base::win::VERSION_WIN_LAST);
  UMA_HISTOGRAM_ENUMERATION("Windows.Kernel32Version",
                            os_info.Kernel32Version(),
                            base::win::VERSION_WIN_LAST);
  UMA_HISTOGRAM_BOOLEAN("Windows.InCompatibilityMode",
                        os_info.version() != os_info.Kernel32Version());
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
  bluetooth_utility::BluetoothAvailability availability =
      bluetooth_utility::GetBluetoothAvailability();
  UMA_HISTOGRAM_ENUMERATION("OSX.BluetoothAvailability",
                            availability,
                            bluetooth_utility::BLUETOOTH_AVAILABILITY_COUNT);
#endif   // defined(OS_MACOSX)

  // Record whether Chrome is the default browser or not.
  shell_integration::DefaultWebClientState default_state =
      shell_integration::GetDefaultBrowser();
  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.State", default_state,
                            shell_integration::NUM_DEFAULT_STATES);
}

void RecordLinuxGlibcVersion() {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  base::Version version(gnu_get_libc_version());

  UMALinuxGlibcVersion glibc_version_result = UMA_LINUX_GLIBC_NOT_PARSEABLE;
  if (version.IsValid() && version.components().size() == 2) {
    glibc_version_result = UMA_LINUX_GLIBC_UNKNOWN;
    uint32_t glibc_major_version = version.components()[0];
    uint32_t glibc_minor_version = version.components()[1];
    if (glibc_major_version == 2) {
      // A constant to translate glibc 2.x minor versions to their
      // equivalent UMALinuxGlibcVersion values.
      const int kGlibcMinorVersionTranslationOffset = 11 - UMA_LINUX_GLIBC_2_11;
      uint32_t translated_glibc_minor_version =
          glibc_minor_version - kGlibcMinorVersionTranslationOffset;
      if (translated_glibc_minor_version >= UMA_LINUX_GLIBC_2_11) {
        glibc_version_result =
            static_cast<UMALinuxGlibcVersion>(translated_glibc_minor_version);
      }
    }
  }
  UMA_HISTOGRAM_SPARSE_SLOWLY("Linux.GlibcVersion", glibc_version_result);
#endif
}

#if defined(USE_X11) && !defined(OS_CHROMEOS)
UMALinuxWindowManager GetLinuxWindowManager() {
  switch (ui::GuessWindowManager()) {
    case ui::WM_OTHER:
      return UMA_LINUX_WINDOW_MANAGER_OTHER;
    case ui::WM_UNNAMED:
      return UMA_LINUX_WINDOW_MANAGER_UNNAMED;
    case ui::WM_AWESOME:
      return UMA_LINUX_WINDOW_MANAGER_AWESOME;
    case ui::WM_BLACKBOX:
      return UMA_LINUX_WINDOW_MANAGER_BLACKBOX;
    case ui::WM_COMPIZ:
      return UMA_LINUX_WINDOW_MANAGER_COMPIZ;
    case ui::WM_ENLIGHTENMENT:
      return UMA_LINUX_WINDOW_MANAGER_ENLIGHTENMENT;
    case ui::WM_FLUXBOX:
      return UMA_LINUX_WINDOW_MANAGER_FLUXBOX;
    case ui::WM_I3:
      return UMA_LINUX_WINDOW_MANAGER_I3;
    case ui::WM_ICE_WM:
      return UMA_LINUX_WINDOW_MANAGER_ICE_WM;
    case ui::WM_ION3:
      return UMA_LINUX_WINDOW_MANAGER_ION3;
    case ui::WM_KWIN:
      return UMA_LINUX_WINDOW_MANAGER_KWIN;
    case ui::WM_MATCHBOX:
      return UMA_LINUX_WINDOW_MANAGER_MATCHBOX;
    case ui::WM_METACITY:
      return UMA_LINUX_WINDOW_MANAGER_METACITY;
    case ui::WM_MUFFIN:
      return UMA_LINUX_WINDOW_MANAGER_MUFFIN;
    case ui::WM_MUTTER:
      return UMA_LINUX_WINDOW_MANAGER_MUTTER;
    case ui::WM_NOTION:
      return UMA_LINUX_WINDOW_MANAGER_NOTION;
    case ui::WM_OPENBOX:
      return UMA_LINUX_WINDOW_MANAGER_OPENBOX;
    case ui::WM_QTILE:
      return UMA_LINUX_WINDOW_MANAGER_QTILE;
    case ui::WM_RATPOISON:
      return UMA_LINUX_WINDOW_MANAGER_RATPOISON;
    case ui::WM_STUMPWM:
      return UMA_LINUX_WINDOW_MANAGER_STUMPWM;
    case ui::WM_WMII:
      return UMA_LINUX_WINDOW_MANAGER_WMII;
    case ui::WM_XFWM4:
      return UMA_LINUX_WINDOW_MANAGER_XFWM4;
    case ui::WM_XMONAD:
      return UMA_LINUX_WINDOW_MANAGER_XMONAD;
  }
  NOTREACHED();
  return UMA_LINUX_WINDOW_MANAGER_OTHER;
}
#endif

void RecordTouchEventState() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  const std::string touch_enabled_switch =
      command_line.HasSwitch(switches::kTouchEvents) ?
      command_line.GetSwitchValueASCII(switches::kTouchEvents) :
      switches::kTouchEventsAuto;

  UMATouchEventsState state;
  if (touch_enabled_switch.empty() ||
      touch_enabled_switch == switches::kTouchEventsEnabled) {
    state = UMA_TOUCH_EVENTS_ENABLED;
  } else if (touch_enabled_switch == switches::kTouchEventsAuto) {
    state = (ui::GetTouchScreensAvailability() ==
             ui::TouchScreensAvailability::ENABLED)
                ? UMA_TOUCH_EVENTS_AUTO_ENABLED
                : UMA_TOUCH_EVENTS_AUTO_DISABLED;
  } else if (touch_enabled_switch == switches::kTouchEventsDisabled) {
    state = UMA_TOUCH_EVENTS_DISABLED;
  } else {
    NOTREACHED();
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("Touchscreen.TouchEventsEnabled", state,
                            UMA_TOUCH_EVENTS_STATE_COUNT);
}

#if defined(USE_OZONE) || defined(USE_X11)

// Asynchronously records the touch event state when the ui::DeviceDataManager
// completes a device scan.
class AsynchronousTouchEventStateRecorder
    : public ui::InputDeviceEventObserver {
 public:
  AsynchronousTouchEventStateRecorder();
  ~AsynchronousTouchEventStateRecorder() override;

  // ui::InputDeviceEventObserver overrides.
  void OnDeviceListsComplete() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AsynchronousTouchEventStateRecorder);
};

AsynchronousTouchEventStateRecorder::AsynchronousTouchEventStateRecorder() {
  ui::InputDeviceManager::GetInstance()->AddObserver(this);
}

AsynchronousTouchEventStateRecorder::~AsynchronousTouchEventStateRecorder() {
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
}

void AsynchronousTouchEventStateRecorder::OnDeviceListsComplete() {
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
  RecordTouchEventState();
}

#endif  // defined(USE_OZONE) || defined(USE_X11)

#if defined(OS_WIN)
void RecordPinnedToTaskbarProcessError(bool error) {
  UMA_HISTOGRAM_BOOLEAN("Windows.IsPinnedToTaskbar.ProcessError", error);
}

void OnShellHandlerConnectionError() {
  RecordPinnedToTaskbarProcessError(true);
}

// Record the UMA histogram when a response is received.
void OnIsPinnedToTaskbarResult(bool succeeded, bool is_pinned_to_taskbar) {
  RecordPinnedToTaskbarProcessError(false);

  // Used for histograms; do not reorder.
  enum Result { NOT_PINNED = 0, PINNED = 1, FAILURE = 2, NUM_RESULTS };

  Result result = FAILURE;
  if (succeeded)
    result = is_pinned_to_taskbar ? PINNED : NOT_PINNED;
  UMA_HISTOGRAM_ENUMERATION("Windows.IsPinnedToTaskbar", result, NUM_RESULTS);
}

// Records the pinned state of the current executable into a histogram.
void RecordIsPinnedToTaskbarHistogram() {
  shell_integration::win::GetIsPinnedToTaskbarState(
      base::Bind(&OnShellHandlerConnectionError),
      base::Bind(&OnIsPinnedToTaskbarResult));
}
#endif  // defined(OS_WIN)

}  // namespace

ChromeBrowserMainExtraPartsMetrics::ChromeBrowserMainExtraPartsMetrics()
    : display_count_(0), is_screen_observer_(false) {
}

ChromeBrowserMainExtraPartsMetrics::~ChromeBrowserMainExtraPartsMetrics() {
  if (is_screen_observer_)
    display::Screen::GetScreen()->RemoveObserver(this);
}

void ChromeBrowserMainExtraPartsMetrics::PreProfileInit() {
  RecordMicroArchitectureStats();
}

void ChromeBrowserMainExtraPartsMetrics::PreBrowserStart() {
  flags_ui::PrefServiceFlagsStorage flags_storage(
      g_browser_process->local_state());
  about_flags::RecordUMAStatistics(&flags_storage);
}

void ChromeBrowserMainExtraPartsMetrics::PostBrowserStart() {
  RecordLinuxGlibcVersion();
#if defined(USE_X11) && !defined(OS_CHROMEOS)
  UMA_HISTOGRAM_ENUMERATION("Linux.WindowManager",
                            GetLinuxWindowManager(),
                            UMA_LINUX_WINDOW_MANAGER_COUNT);
#endif

#if defined(USE_OZONE) || defined(USE_X11)
  // The touch event state for X11 and Ozone based event sub-systems are based
  // on device scans that happen asynchronously. So we may need to attach an
  // observer to wait until these scans complete.
  if (ui::InputDeviceManager::GetInstance()->AreDeviceListsComplete()) {
    RecordTouchEventState();
  } else {
    input_device_event_observer_.reset(
        new AsynchronousTouchEventStateRecorder());
  }
#else
  RecordTouchEventState();
#endif  // defined(USE_OZONE) || defined(USE_X11)

#if defined(OS_MACOSX)
  RecordMacMetrics();
#endif  // defined(OS_MACOSX)

  constexpr base::TimeDelta kStartupMetricsGatheringDelay =
      base::TimeDelta::FromSeconds(45);
  content::BrowserThread::GetBlockingPool()->PostDelayedTask(
      FROM_HERE, base::Bind(&RecordStartupMetricsOnBlockingPool),
      kStartupMetricsGatheringDelay);
#if defined(OS_WIN)
  content::BrowserThread::PostDelayedTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&RecordIsPinnedToTaskbarHistogram),
      kStartupMetricsGatheringDelay);
#endif  // defined(OS_WIN)

  display_count_ = display::Screen::GetScreen()->GetNumDisplays();
  UMA_HISTOGRAM_COUNTS_100("Hardware.Display.Count.OnStartup", display_count_);
  display::Screen::GetScreen()->AddObserver(this);
  is_screen_observer_ = true;

#if !defined(OS_ANDROID)
  FirstWebContentsProfiler::Start();
  metrics::TabUsageRecorder::Initialize();
#endif  // !defined(OS_ANDROID)
}

void ChromeBrowserMainExtraPartsMetrics::OnDisplayAdded(
    const display::Display& new_display) {
  EmitDisplaysChangedMetric();
}

void ChromeBrowserMainExtraPartsMetrics::OnDisplayRemoved(
    const display::Display& old_display) {
  EmitDisplaysChangedMetric();
}

void ChromeBrowserMainExtraPartsMetrics::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {}

void ChromeBrowserMainExtraPartsMetrics::EmitDisplaysChangedMetric() {
  int display_count = display::Screen::GetScreen()->GetNumDisplays();
  if (display_count != display_count_) {
    display_count_ = display_count;
    UMA_HISTOGRAM_COUNTS_100("Hardware.Display.Count.OnChange", display_count_);
  }
}

namespace chrome {

void AddMetricsExtraParts(ChromeBrowserMainParts* main_parts) {
  main_parts->AddParts(new ChromeBrowserMainExtraPartsMetrics());
}

}  // namespace chrome
