// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/arc/arc_process_task.h"

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_bridge_service.h"
#include "content/public/common/child_process_host.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_management {

namespace {

base::string16 MakeTitle(const std::string& process_name) {
  base::string16 title =
      l10n_util::GetStringFUTF16(
          IDS_TASK_MANAGER_ARC_PREFIX, base::UTF8ToUTF16(process_name));
  base::i18n::AdjustStringForLocaleDirection(&title);
  return title;
}

}  // namespace

ArcProcessTask::ArcProcessTask(
    base::ProcessId pid,
    base::ProcessId nspid,
    const std::string& process_name)
    : Task(MakeTitle(process_name), process_name, nullptr, pid),
      nspid_(nspid),
      process_name_(process_name) {
}

ArcProcessTask::~ArcProcessTask() {
}

Task::Type ArcProcessTask::GetType() const {
  return Task::ARC;
}

int ArcProcessTask::GetChildProcessUniqueID() const {
  // ARC process is not a child process of the browser.
  return content::ChildProcessHost::kInvalidUniqueID;
}

void ArcProcessTask::Kill() {
  arc::mojom::ProcessInstance* arc_process_instance =
      arc::ArcBridgeService::Get()->process_instance();
  if (!arc_process_instance) {
    LOG(ERROR) << "ARC process instance is not ready.";
    return;
  }
  if (arc::ArcBridgeService::Get()->process_version() < 1) {
    LOG(ERROR) << "ARC KillProcess IPC is unavailable.";
    return;
  }
  arc_process_instance->KillProcess(
      nspid_, "Killed manually from Task Manager");
}

}  // namespace task_management
