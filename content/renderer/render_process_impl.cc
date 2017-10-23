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

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/crash_logging.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "base/task_scheduler/initialization_util.h"
#include "base/time/time.h"
#include "content/common/task_scheduler.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/loader/site_isolation_stats_gatherer.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "v8/include/v8.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

namespace {

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

std::unique_ptr<base::TaskScheduler::InitParams>
GetDefaultTaskSchedulerInitParams() {

  constexpr int kMaxNumThreadsInBackgroundPool = 1;
  constexpr int kMaxNumThreadsInBackgroundBlockingPool = 1;
  constexpr int kMaxNumThreadsInForegroundPoolLowerBound = 2;
  constexpr int kMaxNumThreadsInForegroundBlockingPool = 1;
  constexpr auto kSuggestedReclaimTime = base::TimeDelta::FromSeconds(30);

  return base::MakeUnique<base::TaskScheduler::InitParams>(
      base::SchedulerWorkerPoolParams(kMaxNumThreadsInBackgroundPool,
                                      kSuggestedReclaimTime),
      base::SchedulerWorkerPoolParams(kMaxNumThreadsInBackgroundBlockingPool,
                                      kSuggestedReclaimTime),
      base::SchedulerWorkerPoolParams(
          std::max(
              kMaxNumThreadsInForegroundPoolLowerBound,
              content::GetMinThreadsInRendererTaskSchedulerForegroundPool()),
          kSuggestedReclaimTime),
      base::SchedulerWorkerPoolParams(kMaxNumThreadsInForegroundBlockingPool,
                                      kSuggestedReclaimTime));
}

#if DCHECK_IS_ON() && defined(SYZYASAN)
void V8DcheckCallbackHandler(const char* file, int line, const char* message) {
  // TODO(siggi): Set a crash key or a breadcrumb so the fact that we hit a
  //     V8 DCHECK gets out in the crash report.
  ::logging::LogMessage(file, line, logging::LOG_DCHECK).stream() << message;
}
#endif  // DCHECK_IS_ON() && defined(SYZYASAN)

}  // namespace

namespace content {

RenderProcessImpl::RenderProcessImpl(
    std::unique_ptr<base::TaskScheduler::InitParams> task_scheduler_init_params)
    : RenderProcess("Renderer", std::move(task_scheduler_init_params)),
      enabled_bindings_(0) {
#if defined(OS_WIN)
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

#if DCHECK_IS_ON() && defined(SYZYASAN)
  // SyzyASAN official builds can ship with DCHECKs compiled in. Failing DCHECKs
  // then are either fatal or simply log the error, based on a feature flag.
  // Make sure V8 follows suit by setting a Dcheck handler that forwards to
  // the Chrome base logging implementation.
  v8::V8::SetDcheckErrorHandler(&V8DcheckCallbackHandler);

  if (!base::FeatureList::IsEnabled(base::kSyzyAsanDCheckIsFatalFeature)) {
    // These V8 flags default on in this build configuration. This triggers
    // additional verification and code generation, which both slows down V8,
    // and can lead to fatal CHECKs. Turn these flags down to get something
    // closer to V8s normal performance and behavior.
    constexpr char kDisabledFlags[] =
        "--noturbo_verify "
        "--noverify_csa "
        "--noturbo_verify_allocation "
        "--nodebug_code";

    v8::V8::SetFlagsFromString(kDisabledFlags, sizeof(kDisabledFlags));
  }
#endif  // DCHECK_IS_ON() && defined(SYZYASAN)

  if (base::SysInfo::IsLowEndDevice()) {
    std::string optimize_flag("--optimize-for-size");
    v8::V8::SetFlagsFromString(optimize_flag.c_str(),
                               static_cast<int>(optimize_flag.size()));
  }

  SetV8FlagIfHasSwitch(switches::kDisableJavaScriptHarmonyShipping,
                       "--noharmony-shipping");
  SetV8FlagIfHasSwitch(switches::kJavaScriptHarmony, "--harmony");
  SetV8FlagIfFeature(features::kModuleScriptsDynamicImport,
                     "--harmony-dynamic-import");
  SetV8FlagIfFeature(features::kModuleScriptsImportMetaUrl,
                     "--harmony-import-meta");
  SetV8FlagIfFeature(features::kAsmJsToWebAssembly, "--validate-asm");
  SetV8FlagIfNotFeature(features::kAsmJsToWebAssembly, "--no-validate-asm");
  SetV8FlagIfNotFeature(features::kWebAssembly,
                        "--wasm-disable-structured-cloning");

  SetV8FlagIfFeature(features::kV8VmFuture, "--future");
  SetV8FlagIfNotFeature(features::kV8VmFuture, "--no-future");

  SetV8FlagIfFeature(features::kWebAssemblyTrapHandler, "--wasm-trap-handler");
  SetV8FlagIfNotFeature(features::kWebAssemblyTrapHandler,
                        "--no-wasm-trap-handler");
#if defined(OS_LINUX) && defined(ARCH_CPU_X86_64) && !defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebAssemblyTrapHandler)) {
    base::debug::SetStackDumpFirstChanceCallback(v8::V8::TryHandleSignal);
  }
#endif

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kJavaScriptFlags)) {
    std::string flags(
        command_line.GetSwitchValueASCII(switches::kJavaScriptFlags));
    v8::V8::SetFlagsFromString(flags.c_str(), static_cast<int>(flags.size()));
  }

  SiteIsolationStatsGatherer::SetEnabled(
      GetContentClient()->renderer()->ShouldGatherSiteIsolationStats());

  if (command_line.HasSwitch(switches::kDomAutomationController))
    enabled_bindings_ |= BINDINGS_POLICY_DOM_AUTOMATION;
  if (command_line.HasSwitch(switches::kStatsCollectionController))
    enabled_bindings_ |= BINDINGS_POLICY_STATS_COLLECTION;
}

RenderProcessImpl::~RenderProcessImpl() {
#ifndef NDEBUG
  int count = blink::WebFrame::InstanceCount();
  if (count)
    DLOG(ERROR) << "WebFrame LEAKED " << count << " TIMES";
#endif

  GetShutDownEvent()->Signal();
}

std::unique_ptr<RenderProcess> RenderProcessImpl::Create() {
  auto task_scheduler_init_params =
      content::GetContentClient()->renderer()->GetTaskSchedulerInitParams();
  if (!task_scheduler_init_params)
    task_scheduler_init_params = GetDefaultTaskSchedulerInitParams();

  return base::WrapUnique(
      new RenderProcessImpl(std::move(task_scheduler_init_params)));
}

void RenderProcessImpl::AddBindings(int bindings) {
  enabled_bindings_ |= bindings;
}

int RenderProcessImpl::GetEnabledBindings() const {
  return enabled_bindings_;
}

}  // namespace content
