// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/service.h"

#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "webkit/browser/fileapi/external_mount_points.h"

namespace chromeos {
namespace file_system_provider {
namespace {

// Maximum number of file systems to be mounted in the same time, per profile.
const size_t kMaxFileSystems = 16;

// Default factory for provided file systems. The |event_router| nor the
// |request_manager| arguments must not be NULL.
ProvidedFileSystemInterface* CreateProvidedFileSystem(
    extensions::EventRouter* event_router,
    RequestManager* request_manager,
    const ProvidedFileSystemInfo& file_system_info) {
  DCHECK(event_router);
  DCHECK(request_manager);
  return new ProvidedFileSystem(
      event_router, request_manager, file_system_info);
}

}  // namespace

Service::Service(Profile* profile)
    : profile_(profile),
      file_system_factory_(base::Bind(CreateProvidedFileSystem)),
      next_id_(1),
      weak_ptr_factory_(this) {
  AddObserver(&request_manager_);
}

Service::~Service() { STLDeleteValues(&file_system_map_); }

// static
Service* Service::Get(content::BrowserContext* context) {
  return ServiceFactory::Get(context);
}

void Service::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void Service::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void Service::SetFileSystemFactoryForTests(
    const FileSystemFactoryCallback& factory_callback) {
  DCHECK(!factory_callback.is_null());
  file_system_factory_ = factory_callback;
}

int Service::MountFileSystem(const std::string& extension_id,
                             const std::string& file_system_name) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Restrict number of file systems to prevent system abusing.
  if (file_system_map_.size() + 1 > kMaxFileSystems) {
    FOR_EACH_OBSERVER(
        Observer,
        observers_,
        OnProvidedFileSystemMount(ProvidedFileSystemInfo(),
                                  base::File::FILE_ERROR_TOO_MANY_OPENED));
    return 0;
  }

  // The provided file system id is unique per service, so per profile.
  int file_system_id = next_id_;

  fileapi::ExternalMountPoints* const mount_points =
      fileapi::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);

  // The mount point path and name are unique per system, since they are system
  // wide. This is necessary for copying between profiles.
  const base::FilePath& mount_point_path =
      util::GetMountPointPath(profile_, extension_id, file_system_id);
  const std::string mount_point_name =
      mount_point_path.BaseName().AsUTF8Unsafe();

  if (!mount_points->RegisterFileSystem(mount_point_name,
                                        fileapi::kFileSystemTypeProvided,
                                        fileapi::FileSystemMountOption(),
                                        mount_point_path)) {
    FOR_EACH_OBSERVER(
        Observer,
        observers_,
        OnProvidedFileSystemMount(ProvidedFileSystemInfo(),
                                  base::File::FILE_ERROR_INVALID_OPERATION));
    return 0;
  }

  // Store the file system descriptor. Use the mount point name as the file
  // system provider file system id.
  // Examples:
  //   file_system_id = 41
  //   mount_point_name = file_system_id = b33f1337-41-5aa5
  //   mount_point_path = /provided/b33f1337-41-5aa5
  ProvidedFileSystemInfo file_system_info(
      extension_id, file_system_id, file_system_name, mount_point_path);

  // The event router may be NULL for unit tests.
  extensions::EventRouter* event_router =
      extensions::ExtensionSystem::Get(profile_)->event_router();

  ProvidedFileSystemInterface* file_system = file_system_factory_.Run(
      event_router, &request_manager_, file_system_info);
  DCHECK(file_system);
  file_system_map_[file_system_id] = file_system;

  FOR_EACH_OBSERVER(
      Observer,
      observers_,
      OnProvidedFileSystemMount(file_system_info, base::File::FILE_OK));

  next_id_++;
  return file_system_id;
}

bool Service::UnmountFileSystem(const std::string& extension_id,
                                int file_system_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ProvidedFileSystemMap::iterator file_system_it =
      file_system_map_.find(file_system_id);
  if (file_system_it == file_system_map_.end() ||
      file_system_it->second->GetFileSystemInfo().extension_id() !=
          extension_id) {
    const ProvidedFileSystemInfo empty_file_system_info;
    FOR_EACH_OBSERVER(
        Observer,
        observers_,
        OnProvidedFileSystemUnmount(empty_file_system_info,
                                    base::File::FILE_ERROR_NOT_FOUND));
    return false;
  }

  fileapi::ExternalMountPoints* const mount_points =
      fileapi::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);

  const ProvidedFileSystemInfo& file_system_info =
      file_system_it->second->GetFileSystemInfo();

  const std::string mount_point_name =
      file_system_info.mount_path().BaseName().value();
  if (!mount_points->RevokeFileSystem(mount_point_name)) {
    FOR_EACH_OBSERVER(
        Observer,
        observers_,
        OnProvidedFileSystemUnmount(file_system_info,
                                    base::File::FILE_ERROR_INVALID_OPERATION));
    return false;
  }

  FOR_EACH_OBSERVER(
      Observer,
      observers_,
      OnProvidedFileSystemUnmount(file_system_info, base::File::FILE_OK));

  delete file_system_it->second;
  file_system_map_.erase(file_system_it);
  return true;
}

bool Service::RequestUnmount(int file_system_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ProvidedFileSystemMap::iterator file_system_it =
      file_system_map_.find(file_system_id);
  if (file_system_it == file_system_map_.end())
    return false;

  return file_system_it->second->RequestUnmount(
      base::Bind(&Service::OnRequestUnmountStatus,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_system_it->second->GetFileSystemInfo()));
}

std::vector<ProvidedFileSystemInfo> Service::GetProvidedFileSystemInfoList() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::vector<ProvidedFileSystemInfo> result;
  for (ProvidedFileSystemMap::const_iterator it = file_system_map_.begin();
       it != file_system_map_.end();
       ++it) {
    result.push_back(it->second->GetFileSystemInfo());
  }
  return result;
}

ProvidedFileSystemInterface* Service::GetProvidedFileSystem(
    const std::string& extension_id,
    int file_system_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ProvidedFileSystemMap::iterator file_system_it =
      file_system_map_.find(file_system_id);
  if (file_system_it == file_system_map_.end() ||
      file_system_it->second->GetFileSystemInfo().extension_id() !=
          extension_id) {
    return NULL;
  }

  return file_system_it->second;
}

void Service::Shutdown() { RemoveObserver(&request_manager_); }

void Service::OnRequestUnmountStatus(
    const ProvidedFileSystemInfo& file_system_info,
    base::File::Error error) {
  // Notify observers about failure in unmounting, since mount() will not be
  // called by the provided file system. In case of success mount() will be
  // invoked, and observers notified, so there is no need to call them now.
  if (error != base::File::FILE_OK) {
    FOR_EACH_OBSERVER(Observer,
                      observers_,
                      OnProvidedFileSystemUnmount(file_system_info, error));
  }
}

}  // namespace file_system_provider
}  // namespace chromeos
