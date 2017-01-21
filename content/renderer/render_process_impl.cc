// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_process_impl.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <objidl.h>
#include <mlang.h>
#endif

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/sys_info.h"
#include "base/task_scheduler/initialization_util.h"
#include "base/task_scheduler/scheduler_worker_pool_params.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "content/child/site_isolation_stats_gatherer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "v8/include/v8.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

namespace {

enum WorkerPoolType : size_t {
  BACKGROUND = 0,
  BACKGROUND_FILE_IO,
  FOREGROUND,
  FOREGROUND_FILE_IO,
  WORKER_POOL_COUNT  // Always last.
};

const base::Feature kV8_ES2015_TailCalls_Feature {
  "V8_ES2015_TailCalls", base::FEATURE_DISABLED_BY_DEFAULT
};

const base::Feature kV8_ES2016_ExplicitTailCalls_Feature{
    "V8_ES2016_ExplicitTailCalls", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kV8SerializeEagerFeature{"V8_Serialize_Eager",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kV8SerializeAgeCodeFeature{
    "V8_Serialize_Age_Code", base::FEATURE_DISABLED_BY_DEFAULT};

void SetV8FlagIfFeature(const base::Feature& feature, const char* v8_flag) {
  if (base::FeatureList::IsEnabled(feature)) {
    v8::V8::SetFlagsFromString(v8_flag, strlen(v8_flag));
  }
}

void SetV8FlagIfNotFeature(const base::Feature& feature, const char* v8_flag) {
  if (!base::FeatureList::IsEnabled(feature)) {
    v8::V8::SetFlagsFromString(v8_flag, strlen(v8_flag));
  }
}

void SetV8FlagIfHasSwitch(const char* switch_name, const char* v8_flag) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switch_name)) {
    v8::V8::SetFlagsFromString(v8_flag, strlen(v8_flag));
  }
}

std::vector<base::SchedulerWorkerPoolParams>
GetDefaultSchedulerWorkerPoolParams() {
  using StandbyThreadPolicy =
      base::SchedulerWorkerPoolParams::StandbyThreadPolicy;
  using ThreadPriority = base::ThreadPriority;
  constexpr size_t kMaxNumThreadsInBackgroundPool = 1;
  constexpr size_t kMaxNumThreadsInBackgroundFileIOPool = 1;
  constexpr int kMaxNumThreadsInForegroundPoolLowerBound = 2;
  constexpr int kMaxNumThreadsInForegroundPoolUpperBound = 4;
  constexpr double kMaxNumThreadsInForegroundPoolCoresMultiplier = 1;
  constexpr int kMaxNumThreadsInForegroundPoolOffset = 0;
  constexpr size_t kMaxNumThreadsInForegroundFileIOPool = 1;
  constexpr auto kSuggestedReclaimTime = base::TimeDelta::FromSeconds(30);

  std::vector<base::SchedulerWorkerPoolParams> params_vector;
  params_vector.emplace_back("RendererBackground", ThreadPriority::BACKGROUND,
                             StandbyThreadPolicy::LAZY,
                             kMaxNumThreadsInBackgroundPool,
                             kSuggestedReclaimTime);
  params_vector.emplace_back(
      "RendererBackgroundFileIO", ThreadPriority::BACKGROUND,
      StandbyThreadPolicy::LAZY, kMaxNumThreadsInBackgroundFileIOPool,
      kSuggestedReclaimTime);
  params_vector.emplace_back("RendererForeground", ThreadPriority::NORMAL,
                             StandbyThreadPolicy::LAZY,
                             base::RecommendedMaxNumberOfThreadsInPool(
                                 kMaxNumThreadsInForegroundPoolLowerBound,
                                 kMaxNumThreadsInForegroundPoolUpperBound,
                                 kMaxNumThreadsInForegroundPoolCoresMultiplier,
                                 kMaxNumThreadsInForegroundPoolOffset),
                             kSuggestedReclaimTime);
  params_vector.emplace_back("RendererForegroundFileIO", ThreadPriority::NORMAL,
                             StandbyThreadPolicy::LAZY,
                             kMaxNumThreadsInForegroundFileIOPool,
                             kSuggestedReclaimTime);
  DCHECK_EQ(WORKER_POOL_COUNT, params_vector.size());
  return params_vector;
}

// Returns the worker pool index for |traits| defaulting to FOREGROUND or
// FOREGROUND_FILE_IO on any other priorities based off of worker pools defined
// in GetDefaultSchedulerWorkerPoolParams().
size_t DefaultRendererWorkerPoolIndexForTraits(const base::TaskTraits& traits) {
  const bool is_background =
      traits.priority() == base::TaskPriority::BACKGROUND;
  if (traits.may_block() || traits.with_base_sync_primitives())
    return is_background ? BACKGROUND_FILE_IO : FOREGROUND_FILE_IO;

  return is_background ? BACKGROUND : FOREGROUND;
}

}  // namespace

namespace content {

RenderProcessImpl::RenderProcessImpl()
    : enabled_bindings_(0) {
#if defined(OS_WIN)
  // Record whether the machine is domain joined in a crash key. This will be
  // used to better identify whether crashes are from enterprise users.
  // Note that this is done very early on so that crashes have the highest
  // chance of getting tagged.
  base::debug::SetCrashKeyValue("enrolled-to-domain",
                                base::win::IsEnrolledToDomain() ? "yes" : "no");

  // HACK:  See http://b/issue?id=1024307 for rationale.
  if (GetModuleHandle(L"LPK.DLL") == NULL) {
    // Makes sure lpk.dll is loaded by gdi32 to make sure ExtTextOut() works
    // when buffering into a EMF buffer for printing.
    typedef BOOL (__stdcall *GdiInitializeLanguagePack)(int LoadedShapingDLLs);
    GdiInitializeLanguagePack gdi_init_lpk =
        reinterpret_cast<GdiInitializeLanguagePack>(GetProcAddress(
            GetModuleHandle(L"GDI32.DLL"),
            "GdiInitializeLanguagePack"));
    DCHECK(gdi_init_lpk);
    if (gdi_init_lpk) {
      gdi_init_lpk(0);
    }
  }
#endif

  if (base::SysInfo::IsLowEndDevice()) {
    std::string optimize_flag("--optimize-for-size");
    v8::V8::SetFlagsFromString(optimize_flag.c_str(),
                               static_cast<int>(optimize_flag.size()));
  }

  SetV8FlagIfFeature(kV8_ES2015_TailCalls_Feature, "--harmony-tailcalls");
  SetV8FlagIfFeature(kV8_ES2016_ExplicitTailCalls_Feature,
                     "--harmony-explicit-tailcalls");
  SetV8FlagIfFeature(kV8SerializeEagerFeature, "--serialize_eager");
  SetV8FlagIfFeature(kV8SerializeAgeCodeFeature, "--serialize_age_code");
  SetV8FlagIfHasSwitch(switches::kDisableJavaScriptHarmonyShipping,
                       "--noharmony-shipping");
  SetV8FlagIfHasSwitch(switches::kJavaScriptHarmony, "--harmony");
  SetV8FlagIfFeature(features::kAsmJsToWebAssembly, "--validate-asm");
  SetV8FlagIfNotFeature(features::kWebAssembly,
                        "--wasm-disable-structured-cloning");
  SetV8FlagIfFeature(features::kSharedArrayBuffer,
                     "--harmony-sharedarraybuffer");

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kJavaScriptFlags)) {
    std::string flags(
        command_line.GetSwitchValueASCII(switches::kJavaScriptFlags));
    v8::V8::SetFlagsFromString(flags.c_str(), static_cast<int>(flags.size()));
  }

  SiteIsolationStatsGatherer::SetEnabled(
      GetContentClient()->renderer()->ShouldGatherSiteIsolationStats());
}

RenderProcessImpl::~RenderProcessImpl() {
#ifndef NDEBUG
  int count = blink::WebFrame::instanceCount();
  if (count)
    DLOG(ERROR) << "WebFrame LEAKED " << count << " TIMES";
#endif

  GetShutDownEvent()->Signal();
}

void RenderProcessImpl::AddBindings(int bindings) {
  enabled_bindings_ |= bindings;
}

int RenderProcessImpl::GetEnabledBindings() const {
  return enabled_bindings_;
}

void RenderProcessImpl::InitializeTaskScheduler() {
  std::vector<base::SchedulerWorkerPoolParams> params_vector;
  base::TaskScheduler::WorkerPoolIndexForTraitsCallback
      index_to_traits_callback;
  content::GetContentClient()->renderer()->GetTaskSchedulerInitializationParams(
      &params_vector, &index_to_traits_callback);

  if (params_vector.empty()) {
    params_vector = GetDefaultSchedulerWorkerPoolParams();
    index_to_traits_callback =
        base::Bind(&DefaultRendererWorkerPoolIndexForTraits);
  }
  DCHECK(index_to_traits_callback);

  base::TaskScheduler::CreateAndSetDefaultTaskScheduler(
      params_vector, index_to_traits_callback);
}

}  // namespace content
