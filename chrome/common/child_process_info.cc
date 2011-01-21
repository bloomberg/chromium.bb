// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_info.h"

#include <limits>

#include "base/atomicops.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

ChildProcessInfo::ChildProcessInfo(ProcessType type, int id) :
    type_(type),
    renderer_type_(RENDERER_UNKNOWN) {
  if (id == -1)
    id_ = GenerateChildProcessUniqueId();
  else
    id_ = id;
}

ChildProcessInfo::ChildProcessInfo(const ChildProcessInfo& original)
    : type_(original.type_),
      renderer_type_(original.renderer_type_),
      name_(original.name_),
      version_(original.version_),
      id_(original.id_),
      process_(original.process_) {
}

ChildProcessInfo::~ChildProcessInfo() {
}

ChildProcessInfo& ChildProcessInfo::operator=(
    const ChildProcessInfo& original) {
  if (&original != this) {
    type_ = original.type_;
    renderer_type_ = original.renderer_type_;
    name_ = original.name_;
    version_ = original.version_;
    id_ = original.id_;
    process_ = original.process_;
  }
  return *this;
}

// static
std::string ChildProcessInfo::GetTypeNameInEnglish(
    ChildProcessInfo::ProcessType type) {
  switch (type) {
    case BROWSER_PROCESS:
      return "Browser";
    case RENDER_PROCESS:
      return "Tab";
    case PLUGIN_PROCESS:
      return "Plug-in";
    case WORKER_PROCESS:
      return "Web Worker";
    case UTILITY_PROCESS:
      return "Utility";
    case PROFILE_IMPORT_PROCESS:
      return "Profile Import helper";
    case ZYGOTE_PROCESS:
      return "Zygote";
    case SANDBOX_HELPER_PROCESS:
      return "Sandbox helper";
    case NACL_LOADER_PROCESS:
      return "Native Client module";
    case NACL_BROKER_PROCESS:
      return "Native Client broker";
    case GPU_PROCESS:
      return "GPU";
    case PPAPI_PLUGIN_PROCESS:
      return "Pepper Plugin";
    case UNKNOWN_PROCESS:
    default:
      DCHECK(false) << "Unknown child process type!";
      return "Unknown";
  }
}

// static
std::string ChildProcessInfo::GetRendererTypeNameInEnglish(
    ChildProcessInfo::RendererProcessType type) {
  switch (type) {
    case RENDERER_NORMAL:
      return "Tab";
    case RENDERER_CHROME:
      return "Tab (Chrome)";
    case RENDERER_EXTENSION:
      return "Extension";
    case RENDERER_DEVTOOLS:
      return "Devtools";
    case RENDERER_INTERSTITIAL:
      return "Interstitial";
    case RENDERER_NOTIFICATION:
      return "Notification";
    case RENDERER_BACKGROUND_APP:
      return "Background App";
    case RENDERER_UNKNOWN:
    default:
      NOTREACHED() << "Unknown renderer process type!";
      return "Unknown";
  }
}

// static
std::string ChildProcessInfo::GetFullTypeNameInEnglish(
    ChildProcessInfo::ProcessType type,
    ChildProcessInfo::RendererProcessType rtype) {
  if (type == RENDER_PROCESS)
    return GetRendererTypeNameInEnglish(rtype);
  return GetTypeNameInEnglish(type);
}


string16 ChildProcessInfo::GetLocalizedTitle() const {
  string16 title = WideToUTF16Hack(name_);
  if (type_ == ChildProcessInfo::PLUGIN_PROCESS && title.empty())
    title = l10n_util::GetStringUTF16(IDS_TASK_MANAGER_UNKNOWN_PLUGIN_NAME);

  // Explicitly mark name as LTR if there is no strong RTL character,
  // to avoid the wrong concatenation result similar to "!Yahoo! Mail: the
  // best web-based Email: NIGULP", in which "NIGULP" stands for the Hebrew
  // or Arabic word for "plugin".
  base::i18n::AdjustStringForLocaleDirection(&title);

  switch (type_) {
    case ChildProcessInfo::UTILITY_PROCESS:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_UTILITY_PREFIX);

    case ChildProcessInfo::PROFILE_IMPORT_PROCESS:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_UTILITY_PREFIX);

    case ChildProcessInfo::GPU_PROCESS:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_GPU_PREFIX);

    case ChildProcessInfo::NACL_BROKER_PROCESS:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NACL_BROKER_PREFIX);

    case ChildProcessInfo::PLUGIN_PROCESS:
    case ChildProcessInfo::PPAPI_PLUGIN_PROCESS:
      return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PLUGIN_PREFIX,
                                        title,
                                        WideToUTF16Hack(version_));

    case ChildProcessInfo::NACL_LOADER_PROCESS:
      return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_NACL_PREFIX, title);

    case ChildProcessInfo::WORKER_PROCESS:
      return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_WORKER_PREFIX, title);

    // These types don't need display names or get them from elsewhere.
    case BROWSER_PROCESS:
    case RENDER_PROCESS:
    case ZYGOTE_PROCESS:
    case SANDBOX_HELPER_PROCESS:
      NOTREACHED();
      break;

    case UNKNOWN_PROCESS:
      NOTREACHED() << "Need localized name for child process type.";
  }

  return title;
}

std::string ChildProcessInfo::GenerateRandomChannelID(void* instance) {
  // Note: the string must start with the current process id, this is how
  // child processes determine the pid of the parent.
  // Build the channel ID.  This is composed of a unique identifier for the
  // parent browser process, an identifier for the child instance, and a random
  // component. We use a random component so that a hacked child process can't
  // cause denial of service by causing future named pipe creation to fail.
  return base::StringPrintf("%d.%p.%d",
                            base::GetCurrentProcId(), instance,
                            base::RandInt(0, std::numeric_limits<int>::max()));
}

// static
int ChildProcessInfo::GenerateChildProcessUniqueId() {
  // This function must be threadsafe.
  static base::subtle::Atomic32 last_unique_child_id = 0;
  return base::subtle::NoBarrier_AtomicIncrement(&last_unique_child_id, 1);
}
