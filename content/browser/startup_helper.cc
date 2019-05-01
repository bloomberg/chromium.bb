// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/startup_helper.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool/initialization_util.h"
#include "base/task/thread_pool/thread_pool.h"
#include "build/build_config.h"
#include "content/common/thread_pool_util.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_switches.h"

namespace content {

namespace {

std::unique_ptr<base::ThreadPool::InitParams> GetDefaultThreadPoolInitParams() {
#if defined(OS_ANDROID)
  // Mobile config, for iOS see ios/web/app/web_main_loop.cc.
  return std::make_unique<base::ThreadPool::InitParams>(
      base::ThreadGroupParams(
          base::RecommendedMaxNumberOfThreadsInPool(4, 8, 0.2, 0),
          base::TimeDelta::FromSeconds(30)),
      base::ThreadGroupParams(
          base::RecommendedMaxNumberOfThreadsInPool(6, 8, 0.6, 0),
          base::TimeDelta::FromSeconds(30)));
#else
  // Desktop config.
  return std::make_unique<base::ThreadPool::InitParams>(
      base::ThreadGroupParams(
          base::RecommendedMaxNumberOfThreadsInPool(6, 8, 0.2, 0),
          base::TimeDelta::FromSeconds(30)),
      base::ThreadGroupParams(
          base::RecommendedMaxNumberOfThreadsInPool(16, 32, 0.6, 0),
          base::TimeDelta::FromSeconds(30))
#if defined(OS_WIN)
          ,
      base::ThreadPool::InitParams::CommonThreadPoolEnvironment::COM_MTA
#endif  // defined(OS_WIN)
  );
#endif
}

}  // namespace

std::unique_ptr<base::FieldTrialList> SetUpFieldTrialsAndFeatureList() {
  auto field_trial_list = std::make_unique<base::FieldTrialList>(nullptr);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // Ensure any field trials specified on the command line are initialized.
  if (command_line->HasSwitch(::switches::kForceFieldTrials)) {
    // Create field trials without activating them, so that this behaves in a
    // consistent manner with field trials created from the server.
    bool result = base::FieldTrialList::CreateTrialsFromString(
        command_line->GetSwitchValueASCII(::switches::kForceFieldTrials),
        std::set<std::string>());
    CHECK(result) << "Invalid --" << ::switches::kForceFieldTrials
                  << " list specified.";
  }

  base::FeatureList::InitializeInstance(
      command_line->GetSwitchValueASCII(switches::kEnableFeatures),
      command_line->GetSwitchValueASCII(switches::kDisableFeatures));
  return field_trial_list;
}

void StartBrowserThreadPool() {
  auto thread_pool_init_params =
      GetContentClient()->browser()->GetThreadPoolInitParams();
  if (!thread_pool_init_params)
    thread_pool_init_params = GetDefaultThreadPoolInitParams();
  DCHECK(thread_pool_init_params);

  // If a renderer lives in the browser process, adjust the number of
  // threads in the foreground pool.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    const base::ThreadGroupParams& current_foreground_thread_group_params(
        thread_pool_init_params->foreground_thread_group_params);
    thread_pool_init_params->foreground_thread_group_params =
        base::ThreadGroupParams(
            std::max(GetMinForegroundThreadsInRendererThreadPool(),
                     current_foreground_thread_group_params.max_tasks()),
            current_foreground_thread_group_params.suggested_reclaim_time(),
            current_foreground_thread_group_params.backward_compatibility());
  }

  base::ThreadPool::GetInstance()->Start(*thread_pool_init_params.get());
}

}  // namespace content
