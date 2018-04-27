// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_remover.h"

#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace crostini {

CrostiniRemover::CrostiniRemover(Profile* profile,
                                 std::string vm_name,
                                 std::string container_name)
    : profile_(profile), vm_name_(vm_name), container_name_(container_name) {}

CrostiniRemover::~CrostiniRemover() = default;

void CrostiniRemover::RemoveCrostini() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  restart_id_ = crostini::CrostiniManager::GetInstance()->RestartCrostini(
      profile_, vm_name_, container_name_,
      base::BindOnce(&CrostiniRemover::OnRestartCrostini, this), this);
}

void CrostiniRemover::OnConciergeStarted(
    crostini::ConciergeClientResult result) {
  // Abort RestartCrostini after it has started the Concierge service, and
  // before it starts the VM.
  crostini::CrostiniManager::GetInstance()->AbortRestartCrostini(restart_id_);
  // Now that we have started the Concierge service, we can use it to stop the
  // VM.
  if (result == crostini::ConciergeClientResult::SUCCESS) {
    crostini::CrostiniManager::GetInstance()->StopVm(
        kCrostiniDefaultVmName,
        base::BindOnce(&CrostiniRemover::StopVmFinished, this));
  }
}

void CrostiniRemover::OnRestartCrostini(
    crostini::ConciergeClientResult result) {
  DCHECK_NE(result, crostini::ConciergeClientResult::SUCCESS)
      << "RestartCrostini failed to be aborted after starting the "
         "Concierge service.";
  LOG(ERROR) << "Failed to start container";
}

void CrostiniRemover::StopVmFinished(crostini::ConciergeClientResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result == crostini::ConciergeClientResult::SUCCESS) {
    crostini::CrostiniManager::GetInstance()->DestroyDiskImage(
        CryptohomeIdForProfile(profile_),
        base::FilePath(kCrostiniDefaultVmName),
        vm_tools::concierge::StorageLocation::STORAGE_CRYPTOHOME_ROOT,
        base::BindOnce(&CrostiniRemover::DestroyDiskImageFinished, this));
  }
}

void CrostiniRemover::DestroyDiskImageFinished(
    crostini::ConciergeClientResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Only set kCrostiniEnabled to false once cleanup is completely finished.
  if (result == crostini::ConciergeClientResult::SUCCESS) {
    profile_->GetPrefs()->SetBoolean(crostini::prefs::kCrostiniEnabled, false);
  }
}

}  // namespace crostini
