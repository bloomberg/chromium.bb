// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/service.h"

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "webkit/browser/fileapi/external_mount_points.h"

namespace chromeos {
namespace file_system_provider {
namespace {

// Root mount path for all of the provided file systems.
const base::FilePath::CharType kProvidedMountPointRoot[] =
    FILE_PATH_LITERAL("/provided");

// Maximum number of file systems to be mounted in the same time, per profile.
const size_t kMaxFileSystems = 16;

// Constructs a safe mount point path for the provided file system represented
// by |file_system_handle|. The handle is a numeric part of the file system id.
base::FilePath GetMountPointPath(Profile* profile,
                                 std::string extension_id,
                                 int file_system_id) {
  chromeos::User* const user =
      chromeos::UserManager::IsInitialized()
          ? chromeos::UserManager::Get()->GetUserByProfile(
                profile->GetOriginalProfile())
          : NULL;
  const std::string user_suffix = user ? "-" + user->username_hash() : "";
  return base::FilePath(kProvidedMountPointRoot).AppendASCII(
      extension_id + "-" + base::IntToString(file_system_id) + user_suffix);
}

// Creates values to be passed to request events. These values can be extended
// by additional fields.
scoped_ptr<base::ListValue> CreateRequestValues(int file_system_id,
                                                int request_id) {
  scoped_ptr<base::ListValue> values(new base::ListValue());
  values->AppendInteger(file_system_id);
  values->AppendInteger(request_id);
  return values.Pass();
}

}  // namespace

Service::Service(Profile* profile)
    : profile_(profile), next_id_(1), weak_ptr_factory_(this) {
  AddObserver(&request_manager_);
}

Service::~Service() {}

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

int Service::MountFileSystem(const std::string& extension_id,
                             const std::string& file_system_name) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Restrict number of file systems to prevent system abusing.
  if (file_systems_.size() + 1 > kMaxFileSystems) {
    FOR_EACH_OBSERVER(
        Observer,
        observers_,
        OnProvidedFileSystemMount(ProvidedFileSystem(),
                                  base::File::FILE_ERROR_TOO_MANY_OPENED));
    return 0;
  }

  // The file system id is unique per service, so per profile.
  int file_system_id = next_id_;

  fileapi::ExternalMountPoints* const mount_points =
      fileapi::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);

  // The mount point path and name are unique per system, since they are system
  // wide. This is necessary for copying between profiles.
  const base::FilePath& mount_point_path =
      GetMountPointPath(profile_, extension_id, file_system_id);
  const std::string mount_point_name =
      mount_point_path.BaseName().AsUTF8Unsafe();

  if (!mount_points->RegisterFileSystem(mount_point_name,
                                        fileapi::kFileSystemTypeProvided,
                                        fileapi::FileSystemMountOption(),
                                        mount_point_path)) {
    FOR_EACH_OBSERVER(
        Observer,
        observers_,
        OnProvidedFileSystemMount(ProvidedFileSystem(),
                                  base::File::FILE_ERROR_INVALID_OPERATION));
    return 0;
  }

  // Store the file system descriptor. Use the mount point name as the file
  // system provider file system id.
  // Examples:
  //   file_system_id = 41
  //   mount_point_name = file_system_id = b33f1337-41-5aa5
  //   mount_point_path = /provided/b33f1337-41-5aa5
  ProvidedFileSystem file_system(
      extension_id, file_system_id, file_system_name, mount_point_path);
  file_systems_[file_system_id] = file_system;

  FOR_EACH_OBSERVER(
      Observer,
      observers_,
      OnProvidedFileSystemMount(file_system, base::File::FILE_OK));

  next_id_++;
  return file_system_id;
}

bool Service::UnmountFileSystem(const std::string& extension_id,
                                int file_system_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  FileSystemMap::iterator file_system_it = file_systems_.find(file_system_id);
  if (file_system_it == file_systems_.end() ||
      file_system_it->second.extension_id() != extension_id) {
    const ProvidedFileSystem empty_file_system;
    FOR_EACH_OBSERVER(Observer,
                      observers_,
                      OnProvidedFileSystemUnmount(
                          empty_file_system, base::File::FILE_ERROR_NOT_FOUND));
    return false;
  }

  fileapi::ExternalMountPoints* const mount_points =
      fileapi::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);

  const std::string mount_point_name =
      file_system_it->second.mount_path().BaseName().value();
  if (!mount_points->RevokeFileSystem(mount_point_name)) {
    FOR_EACH_OBSERVER(
        Observer,
        observers_,
        OnProvidedFileSystemUnmount(file_system_it->second,
                                    base::File::FILE_ERROR_INVALID_OPERATION));
    return false;
  }

  FOR_EACH_OBSERVER(
      Observer,
      observers_,
      OnProvidedFileSystemUnmount(file_system_it->second, base::File::FILE_OK));

  file_systems_.erase(file_system_it);
  return true;
}

std::vector<ProvidedFileSystem> Service::GetMountedFileSystems() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::vector<ProvidedFileSystem> result;
  for (FileSystemMap::const_iterator it = file_systems_.begin();
       it != file_systems_.end();
       ++it) {
    result.push_back(it->second);
  }
  return result;
}

bool Service::FulfillRequest(const std::string& extension_id,
                             int file_system_id,
                             int request_id,
                             scoped_ptr<base::DictionaryValue> result,
                             bool has_next) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  FileSystemMap::iterator file_system_it = file_systems_.find(file_system_id);
  if (file_system_it == file_systems_.end() ||
      file_system_it->second.extension_id() != extension_id) {
    return false;
  }

  return request_manager_.FulfillRequest(
      file_system_it->second, request_id, result.Pass(), has_next);
}

bool Service::RejectRequest(const std::string& extension_id,
                            int file_system_id,
                            int request_id,
                            base::File::Error error) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  FileSystemMap::iterator file_system_it = file_systems_.find(file_system_id);
  if (file_system_it == file_systems_.end() ||
      file_system_it->second.extension_id() != extension_id) {
    return false;
  }

  return request_manager_.RejectRequest(
      file_system_it->second, request_id, error);
}

bool Service::RequestUnmount(int file_system_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  FileSystemMap::iterator file_system_it = file_systems_.find(file_system_id);
  if (file_system_it == file_systems_.end())
    return false;

  int request_id =
      request_manager_.CreateRequest(file_system_it->second,
                                     SuccessCallback(),
                                     base::Bind(&Service::OnRequestUnmountError,
                                                weak_ptr_factory_.GetWeakPtr(),
                                                file_system_it->second));

  if (!request_id)
    return false;

  scoped_ptr<base::ListValue> values(
      CreateRequestValues(file_system_id, request_id));

  extensions::EventRouter* event_router =
      extensions::ExtensionSystem::Get(profile_)->event_router();
  DCHECK(event_router);

  event_router->DispatchEventToExtension(
      file_system_it->second.extension_id(),
      make_scoped_ptr(new extensions::Event(
          extensions::api::file_system_provider::OnUnmountRequested::kEventName,
          values.Pass())));

  return true;
}

void Service::Shutdown() { RemoveObserver(&request_manager_); }

void Service::OnRequestUnmountError(const ProvidedFileSystem& file_system,
                                    base::File::Error error) {
  FOR_EACH_OBSERVER(
      Observer, observers_, OnProvidedFileSystemUnmount(file_system, error));
}

}  // namespace file_system_provider
}  // namespace chromeos
