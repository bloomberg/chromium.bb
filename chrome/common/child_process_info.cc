// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_info.h"

#include <limits>

#include "app/l10n_util.h"
#include "base/atomicops.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "grit/generated_resources.h"

ChildProcessInfo::ChildProcessInfo(const ChildProcessInfo& original)
    : type_(original.type_),
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
    name_ = original.name_;
    version_ = original.version_;
    id_ = original.id_;
    process_ = original.process_;
  }
  return *this;
}

std::wstring ChildProcessInfo::GetTypeNameInEnglish(
    ChildProcessInfo::ProcessType type) {
  switch (type) {
    case BROWSER_PROCESS:
      return L"Browser";
    case RENDER_PROCESS:
      return L"Tab";
    case PLUGIN_PROCESS:
      return L"Plug-in";
    case WORKER_PROCESS:
      return L"Web Worker";
    case UTILITY_PROCESS:
      return L"Utility";
    case PROFILE_IMPORT_PROCESS:
      return L"Profile Import helper";
    case ZYGOTE_PROCESS:
      return L"Zygote";
    case SANDBOX_HELPER_PROCESS:
      return L"Sandbox helper";
    case NACL_LOADER_PROCESS:
      return L"Native Client module";
    case NACL_BROKER_PROCESS:
      return L"Native Client broker";
    case GPU_PROCESS:
      return L"GPU";
    case UNKNOWN_PROCESS:
      default:
      DCHECK(false) << "Unknown child process type!";
      return L"Unknown";
    }
}

std::wstring ChildProcessInfo::GetLocalizedTitle() const {
  std::wstring title = name_;
  if (type_ == ChildProcessInfo::PLUGIN_PROCESS && title.empty())
    title = l10n_util::GetString(IDS_TASK_MANAGER_UNKNOWN_PLUGIN_NAME);

  // Explicitly mark name as LTR if there is no strong RTL character,
  // to avoid the wrong concatenation result similar to "!Yahoo! Mail: the
  // best web-based Email: NIGULP", in which "NIGULP" stands for the Hebrew
  // or Arabic word for "plugin".
  base::i18n::AdjustStringForLocaleDirection(title, &title);

  int message_id;
  if (type_ == ChildProcessInfo::PLUGIN_PROCESS) {
    message_id = IDS_TASK_MANAGER_PLUGIN_PREFIX;
    return l10n_util::GetStringF(message_id, title, version_.c_str());
  } else if (type_ == ChildProcessInfo::WORKER_PROCESS) {
    message_id = IDS_TASK_MANAGER_WORKER_PREFIX;
  } else if (type_ == ChildProcessInfo::UTILITY_PROCESS) {
    message_id = IDS_TASK_MANAGER_UTILITY_PREFIX;
  } else if (type_ == ChildProcessInfo::PROFILE_IMPORT_PROCESS) {
    message_id = IDS_TASK_MANAGER_PROFILE_IMPORT_PREFIX;
  } else if (type_ == ChildProcessInfo::NACL_LOADER_PROCESS) {
    message_id = IDS_TASK_MANAGER_NACL_PREFIX;
  } else if (type_ == ChildProcessInfo::NACL_BROKER_PROCESS) {
    message_id = IDS_TASK_MANAGER_NACL_BROKER_PREFIX;
  } else {
    DCHECK(false) << "Need localized name for child process type.";
    return title;
  }

  return l10n_util::GetStringF(message_id, title);
}

ChildProcessInfo::ChildProcessInfo(ProcessType type, int id) : type_(type) {
  if (id == -1)
    id_ = GenerateChildProcessUniqueId();
  else
    id_ = id;
}

std::string ChildProcessInfo::GenerateRandomChannelID(void* instance) {
  // Note: the string must start with the current process id, this is how
  // child processes determine the pid of the parent.
  // Build the channel ID.  This is composed of a unique identifier for the
  // parent browser process, an identifier for the child instance, and a random
  // component. We use a random component so that a hacked child process can't
  // cause denial of service by causing future named pipe creation to fail.
  return StringPrintf("%d.%p.%d",
                      base::GetCurrentProcId(), instance,
                      base::RandInt(0, std::numeric_limits<int>::max()));
}

// static
int ChildProcessInfo::GenerateChildProcessUniqueId() {
  // This function must be threadsafe.
  static base::subtle::Atomic32 last_unique_child_id = 0;
  return base::subtle::NoBarrier_AtomicIncrement(&last_unique_child_id, 1);
}
