// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/compositor_util.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "gpu/config/gpu_feature_type.h"

namespace content {

namespace {

bool CanDoAcceleratedCompositing() {
  const GpuDataManager* manager = GpuDataManager::GetInstance();

  // Don't run the field trial if gpu access has been blocked or
  // accelerated compositing is blacklisted.
  if (!manager->GpuAccessAllowed(NULL) ||
      manager->IsFeatureBlacklisted(
          gpu::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING))
    return false;

  // Check for SwiftShader.
  if (manager->ShouldUseSwiftShader())
    return false;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisableAcceleratedCompositing))
    return false;

  return true;
}

}  // namespace

bool IsThreadedCompositingEnabled() {
#if defined(USE_AURA)
  // We always want threaded compositing on Aura.
  return true;
#endif

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // Command line switches take precedence over blacklist and field trials.
  if (command_line.HasSwitch(switches::kDisableForceCompositingMode) ||
      command_line.HasSwitch(switches::kDisableThreadedCompositing)) {
    return false;
  } else if (command_line.HasSwitch(switches::kEnableThreadedCompositing)) {
    return true;
  }

  if (!CanDoAcceleratedCompositing())
    return false;

  base::FieldTrial* trial =
      base::FieldTrialList::Find(kGpuCompositingFieldTrialName);
  return trial &&
         trial->group_name() == kGpuCompositingFieldTrialThreadEnabledName;
}

bool IsForceCompositingModeEnabled() {
#if defined(USE_AURA)
  // We always want compositing on Aura.
  return true;
#endif

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // Command line switches take precedence over blacklisting and field trials.
  if (command_line.HasSwitch(switches::kDisableForceCompositingMode))
    return false;
  else if (command_line.HasSwitch(switches::kForceCompositingMode))
    return true;

  if (!CanDoAcceleratedCompositing())
    return false;

  base::FieldTrial* trial =
      base::FieldTrialList::Find(kGpuCompositingFieldTrialName);

  // Force compositing is enabled in both the force compositing
  // and threaded compositing mode field trials.
  return trial &&
        (trial->group_name() ==
            kGpuCompositingFieldTrialForceCompositingEnabledName ||
         trial->group_name() == kGpuCompositingFieldTrialThreadEnabledName);
}

bool IsDelegatedRendererEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool enabled = false;

#if defined(USE_AURA)
  // Default to true on Aura, but allow override via flags.
  enabled = true;
#endif

  // Flags override.
  enabled |= command_line.HasSwitch(switches::kEnableDelegatedRenderer);
  enabled &= !command_line.HasSwitch(switches::kDisableDelegatedRenderer);

  // Needs compositing, and thread.
  if (enabled &&
      (!IsForceCompositingModeEnabled() || !IsThreadedCompositingEnabled())) {
    enabled = false;
    LOG(ERROR) << "Disabling delegated-rendering because it needs "
               << "force-compositing-mode and threaded-compositing.";
  }

  return enabled;
}

}  // namespace content
