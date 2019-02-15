// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_export_import.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/crostini/crostini_manager_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_share_path.h"
#include "chrome/browser/chromeos/crostini/crostini_share_path_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace crostini {

class CrostiniExportImportFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CrostiniExportImport* GetForProfile(Profile* profile) {
    return static_cast<CrostiniExportImport*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static CrostiniExportImportFactory* GetInstance() {
    static base::NoDestructor<CrostiniExportImportFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<CrostiniExportImportFactory>;

  CrostiniExportImportFactory()
      : BrowserContextKeyedServiceFactory(
            "CrostiniExportImportService",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(CrostiniSharePathFactory::GetInstance());
    DependsOn(CrostiniManagerFactory::GetInstance());
  }

  ~CrostiniExportImportFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    return new CrostiniExportImport(profile);
  }
};

CrostiniExportImport* CrostiniExportImport::GetForProfile(Profile* profile) {
  return CrostiniExportImportFactory::GetForProfile(profile);
}

CrostiniExportImport::CrostiniExportImport(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {
  CrostiniManager* manager = CrostiniManager::GetForProfile(profile_);
  manager->AddExportContainerProgressObserver(this);
  manager->AddImportContainerProgressObserver(this);
}

CrostiniExportImport::~CrostiniExportImport() = default;

void CrostiniExportImport::Shutdown() {
  CrostiniManager* manager = CrostiniManager::GetForProfile(profile_);
  manager->RemoveExportContainerProgressObserver(this);
  manager->RemoveImportContainerProgressObserver(this);
}

void CrostiniExportImport::ExportContainer(content::WebContents* web_contents) {
  if (!IsCrostiniExportImportUIAllowedForProfile(profile_)) {
    return;
  }
  OpenFileDialog(ExportImportType::EXPORT, web_contents);
}

void CrostiniExportImport::ImportContainer(content::WebContents* web_contents) {
  if (!IsCrostiniExportImportUIAllowedForProfile(profile_)) {
    return;
  }
  OpenFileDialog(ExportImportType::IMPORT, web_contents);
}

void CrostiniExportImport::OpenFileDialog(ExportImportType type,
                                          content::WebContents* web_contents) {
  PrefService* pref_service = profile_->GetPrefs();
  ui::SelectFileDialog::Type file_selector_mode;
  unsigned title = 0;
  base::FilePath default_path;
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.allowed_paths =
      ui::SelectFileDialog::FileTypeInfo::NATIVE_OR_DRIVE_PATH;
  file_types.extensions = {{"tar.gz", "tgz"}};

  switch (type) {
    case ExportImportType::EXPORT:
      file_selector_mode = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
      title = IDS_CROSTINI_EXPORT_TITLE;
      base::Time::Exploded exploded;
      base::Time::Now().LocalExplode(&exploded);
      default_path = pref_service->GetFilePath(prefs::kDownloadDefaultDirectory)
                         .Append(base::StringPrintf(
                             "crostini-%04d-%02d-%02d.tar.gz", exploded.year,
                             exploded.month, exploded.day_of_month));
      break;
    case ExportImportType::IMPORT:
      file_selector_mode = ui::SelectFileDialog::SELECT_OPEN_FILE,
      title = IDS_CROSTINI_IMPORT_TITLE;
      default_path =
          pref_service->GetFilePath(prefs::kDownloadDefaultDirectory);
      break;
  }

  select_folder_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));
  select_folder_dialog_->SelectFile(
      file_selector_mode, l10n_util::GetStringUTF16(title), default_path,
      &file_types, 0, base::FilePath::StringType(),
      web_contents->GetTopLevelNativeWindow(), reinterpret_cast<void*>(type));
}

void CrostiniExportImport::FileSelected(const base::FilePath& path,
                                        int index,
                                        void* params) {
  ExportImportType type =
      static_cast<ExportImportType>(reinterpret_cast<uintptr_t>(params));
  ContainerId container_id(crostini::kCrostiniDefaultVmName,
                           crostini::kCrostiniDefaultContainerName);
  notifications_[container_id] =
      std::make_unique<CrostiniExportImportNotification>(
          profile_, type, GetUniqueNotificationId(), path);

  switch (type) {
    case ExportImportType::EXPORT:
      crostini::CrostiniSharePath::GetForProfile(profile_)->SharePath(
          crostini::kCrostiniDefaultVmName, path.DirName(), false,
          base::BindOnce(&CrostiniExportImport::ExportAfterSharing,
                         weak_ptr_factory_.GetWeakPtr(), container_id,
                         path.BaseName()));
      break;
    case ExportImportType::IMPORT:
      crostini::CrostiniSharePath::GetForProfile(profile_)->SharePath(
          crostini::kCrostiniDefaultVmName, path, false,
          base::BindOnce(&CrostiniExportImport::ImportAfterSharing,
                         weak_ptr_factory_.GetWeakPtr(), container_id));
      break;
  }
}

void CrostiniExportImport::ExportAfterSharing(
    const ContainerId& container_id,
    const base::FilePath& filename,
    const base::FilePath& container_path,
    bool result,
    const std::string failure_reason) {
  if (!result) {
    LOG(ERROR) << "Error sharing for export " << container_path.value() << ": "
               << failure_reason;
    auto it = notifications_.find(container_id);
    DCHECK(it != notifications_.end()) << ContainerIdToString(container_id)
                                       << " has no notification to update";
    it->second->UpdateStatus(CrostiniExportImportNotification::Status::FAILED,
                             0);
    return;
  }
  CrostiniManager::GetForProfile(profile_)->ExportLxdContainer(
      crostini::kCrostiniDefaultVmName, crostini::kCrostiniDefaultContainerName,
      container_path.Append(filename),
      base::BindOnce(&CrostiniExportImport::OnExportComplete,
                     weak_ptr_factory_.GetWeakPtr(), container_id));
}

void CrostiniExportImport::OnExportComplete(const ContainerId& container_id,
                                            CrostiniResult result) {
  auto it = notifications_.find(container_id);
  DCHECK(it != notifications_.end())
      << ContainerIdToString(container_id) << " has no notification to update";

  if (result != crostini::CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Error exporting " << int(result);
    it->second->UpdateStatus(CrostiniExportImportNotification::Status::FAILED,
                             0);
  } else {
    it->second->UpdateStatus(CrostiniExportImportNotification::Status::DONE, 0);
  }
  notifications_.erase(it);
}

void CrostiniExportImport::OnExportContainerProgress(
    const std::string& vm_name,
    const std::string& container_name,
    crostini::ExportContainerProgressStatus status,
    int progress_percent,
    uint64_t progress_speed) {
  ContainerId container_id(vm_name, container_name);
  auto it = notifications_.find(container_id);
  DCHECK(it != notifications_.end())
      << ContainerIdToString(container_id) << " has no notification to update";

  switch (status) {
    // Rescale PACK:1-100 => 0-50.
    case crostini::ExportContainerProgressStatus::PACK:
      it->second->UpdateStatus(
          CrostiniExportImportNotification::Status::RUNNING,
          progress_percent / 2);
      break;
    // Rescale DOWNLOAD:1-100 => 50-100.
    case crostini::ExportContainerProgressStatus::DOWNLOAD:
      it->second->UpdateStatus(
          CrostiniExportImportNotification::Status::RUNNING,
          50 + progress_percent / 2);
      break;
    default:
      LOG(WARNING) << "Unknown Export progress status " << int(status);
  }
}

void CrostiniExportImport::ImportAfterSharing(
    const ContainerId& container_id,
    const base::FilePath& container_path,
    bool result,
    const std::string failure_reason) {
  if (!result) {
    LOG(ERROR) << "Error sharing for import " << container_path.value() << ": "
               << failure_reason;
    auto it = notifications_.find(container_id);
    DCHECK(it != notifications_.end()) << ContainerIdToString(container_id)
                                       << " has no notification to update";
    it->second->UpdateStatus(CrostiniExportImportNotification::Status::FAILED,
                             0);
    return;
  }
  CrostiniManager::GetForProfile(profile_)->ImportLxdContainer(
      crostini::kCrostiniDefaultVmName, crostini::kCrostiniDefaultContainerName,
      container_path,
      base::BindOnce(&CrostiniExportImport::OnImportComplete,
                     weak_ptr_factory_.GetWeakPtr(), container_id));
}

void CrostiniExportImport::OnImportComplete(const ContainerId& container_id,
                                            CrostiniResult result) {
  auto it = notifications_.find(container_id);
  DCHECK(it != notifications_.end())
      << ContainerIdToString(container_id) << " has no notification to update";
  if (result != crostini::CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Error importing " << int(result);
    it->second->UpdateStatus(CrostiniExportImportNotification::Status::FAILED,
                             0);
  } else {
    it->second->UpdateStatus(CrostiniExportImportNotification::Status::DONE, 0);
  }
  notifications_.erase(it);
}

void CrostiniExportImport::OnImportContainerProgress(
    const std::string& vm_name,
    const std::string& container_name,
    crostini::ImportContainerProgressStatus status,
    int progress_percent,
    uint64_t progress_speed) {
  ContainerId container_id(vm_name, container_name);
  auto it = notifications_.find(container_id);
  DCHECK(it != notifications_.end())
      << ContainerIdToString(container_id) << " has no notification to update";

  switch (status) {
    // Rescale UPLOAD:1-100 => 0-50.
    case crostini::ImportContainerProgressStatus::UPLOAD:
      it->second->UpdateStatus(
          CrostiniExportImportNotification::Status::RUNNING,
          progress_percent / 2);
      break;
    // Rescale UNPACK:1-100 => 50-100.
    case crostini::ImportContainerProgressStatus::UNPACK:
      it->second->UpdateStatus(
          CrostiniExportImportNotification::Status::RUNNING,
          50 + progress_percent / 2);
      break;
    default:
      LOG(WARNING) << "Unknown Export progress status " << int(status);
  }
}

std::string CrostiniExportImport::GetUniqueNotificationId() {
  return base::StringPrintf("crostini_export_import_%d",
                            next_notification_id_++);
}

CrostiniExportImportNotification*
CrostiniExportImport::GetNotificationForTesting(ContainerId container_id) {
  auto it = notifications_.find(container_id);
  return it != notifications_.end() ? it->second.get() : nullptr;
}

}  // namespace crostini
