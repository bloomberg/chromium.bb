// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/arc/arc_process_task.h"

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/process.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/child_process_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

namespace task_management {

namespace {

base::string16 MakeTitle(const std::string& process_name,
                         arc::mojom::ProcessState process_state) {
  int name_template = IDS_TASK_MANAGER_ARC_PREFIX;
  switch (process_state) {
    case arc::mojom::ProcessState::PERSISTENT:
    case arc::mojom::ProcessState::PERSISTENT_UI:
    case arc::mojom::ProcessState::TOP:
      name_template = IDS_TASK_MANAGER_ARC_SYSTEM;
      break;
    case arc::mojom::ProcessState::BOUND_FOREGROUND_SERVICE:
    case arc::mojom::ProcessState::FOREGROUND_SERVICE:
    case arc::mojom::ProcessState::SERVICE:
    case arc::mojom::ProcessState::IMPORTANT_FOREGROUND:
    case arc::mojom::ProcessState::IMPORTANT_BACKGROUND:
      name_template = IDS_TASK_MANAGER_ARC_PREFIX_BACKGROUND_SERVICE;
      break;
    case arc::mojom::ProcessState::RECEIVER:
      name_template = IDS_TASK_MANAGER_ARC_PREFIX_RECEIVER;
      break;
    default:
      break;
  }
  base::string16 title =
      l10n_util::GetStringFUTF16(
          name_template,
          base::UTF8ToUTF16(process_name));
  base::i18n::AdjustStringForLocaleDirection(&title);
  return title;
}

scoped_refptr<arc::ActivityIconLoader> GetIconLoader() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc::ArcServiceManager* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return nullptr;
  return arc_service_manager->icon_loader();
}

// An activity name for retrieving the package's default icon without
// specifying an activity name.
constexpr char kEmptyActivityName[] = "";

}  // namespace

ArcProcessTask::ArcProcessTask(base::ProcessId pid,
                               base::ProcessId nspid,
                               const std::string& process_name,
                               arc::mojom::ProcessState process_state,
                               const std::vector<std::string>& packages)
    : Task(MakeTitle(process_name, process_state), process_name,
           nullptr /* icon */, pid),
      nspid_(nspid),
      process_name_(process_name),
      process_state_(process_state),
      // |packages| contains an alphabetically-sorted list of package names the
      // process has. Since the Task class can hold only one icon per process,
      // and there is no reliable way to pick the most important process from
      // the |packages| list, just use the first item in the list.
      package_name_(packages.empty() ? "" : packages.at(0)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  StartIconLoading();
}

void ArcProcessTask::StartIconLoading() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (package_name_.empty())
    return;

  scoped_refptr<arc::ActivityIconLoader> icon_loader = GetIconLoader();
  arc::ActivityIconLoader::GetResult result =
      arc::ActivityIconLoader::GetResult::FAILED_ARC_NOT_READY;
  if (icon_loader) {
    std::vector<arc::ActivityIconLoader::ActivityName> activities = {
        {package_name_, kEmptyActivityName}};
    result = icon_loader->GetActivityIcons(
        activities, base::Bind(&ArcProcessTask::OnIconLoaded,
                               weak_ptr_factory_.GetWeakPtr()));
  }

  if (result == arc::ActivityIconLoader::GetResult::FAILED_ARC_NOT_READY) {
    // Need to retry loading the icon.
    arc::ArcBridgeService::Get()->AddObserver(this);
  }
}

ArcProcessTask::~ArcProcessTask() {
  arc::ArcBridgeService::Get()->RemoveObserver(this);
}

Task::Type ArcProcessTask::GetType() const {
  return Task::ARC;
}

int ArcProcessTask::GetChildProcessUniqueID() const {
  // ARC process is not a child process of the browser.
  return content::ChildProcessHost::kInvalidUniqueID;
}

bool ArcProcessTask::IsKillable() {
  // Do not kill persistent processes.
  return process_state_ > arc::mojom::ProcessState::PERSISTENT_UI;
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

void ArcProcessTask::OnIntentHelperInstanceReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  VLOG(2) << "intent_helper instance is ready. Fetching the icon for "
          << package_name_;
  arc::ArcBridgeService::Get()->RemoveObserver(this);

  // Instead of calling into StartIconLoading() directly, return to the main
  // loop first to make sure other ArcBridgeService observers are notified.
  // Otherwise, arc::ActivityIconLoader::GetActivityIcon() may fail again.
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(&ArcProcessTask::StartIconLoading,
                                              weak_ptr_factory_.GetWeakPtr()));
}

void ArcProcessTask::SetProcessState(arc::mojom::ProcessState process_state) {
  process_state_ = process_state;
}

void ArcProcessTask::OnIconLoaded(
    std::unique_ptr<arc::ActivityIconLoader::ActivityToIconsMap> icons) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const auto& kv : *icons) {
    const gfx::Image& icon = kv.second.icon16;
    if (icon.IsEmpty())
      continue;
    set_icon(*icon.ToImageSkia());
    break;  // Since the parent class can hold only one icon, break here.
  }
}

}  // namespace task_management
