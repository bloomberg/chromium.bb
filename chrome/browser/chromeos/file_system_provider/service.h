// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SERVICE_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"

namespace extensions {
class ExtensionRegistry;
}  // namespace extensions

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace chromeos {
namespace file_system_provider {

// Key names for preferences.
extern const char kPrefKeyFileSystemId[];
extern const char kPrefKeyDisplayName[];
extern const char kPrefKeyWritable[];

class ProvidedFileSystemFactoryInterface;
class ProvidedFileSystemInfo;
class ProvidedFileSystemInterface;
class ServiceFactory;

// Registers preferences to remember registered file systems between reboots.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Manages and registers the file system provider service. Maintains provided
// file systems.
class Service : public KeyedService,
                public extensions::ExtensionRegistryObserver {
 public:
  typedef base::Callback<ProvidedFileSystemInterface*(
      Profile* profile,
      const ProvidedFileSystemInfo& file_system_info)>
      FileSystemFactoryCallback;

  // Reason for unmounting. In case of UNMOUNT_REASON_SHUTDOWN, the file system
  // will be remounted automatically after a reboot. In case of
  // UNMOUNT_REASON_USER it will be permanently unmounted.
  enum UnmountReason { UNMOUNT_REASON_USER, UNMOUNT_REASON_SHUTDOWN };

  Service(Profile* profile, extensions::ExtensionRegistry* extension_registry);
  virtual ~Service();

  // Sets a custom ProvidedFileSystemInterface factory. Used by unit tests,
  // where an event router is not available.
  void SetFileSystemFactoryForTesting(
      const FileSystemFactoryCallback& factory_callback);

  // Mounts a file system provided by an extension with the |extension_id|. If
  // |writable| is set to true, then the file system is mounted in a R/W mode.
  // Otherwise, only read-only operations are supported. For success, returns
  // true, otherwise false.
  bool MountFileSystem(const std::string& extension_id,
                       const std::string& file_system_id,
                       const std::string& display_name,
                       bool writable);

  // Unmounts a file system with the specified |file_system_id| for the
  // |extension_id|. For success returns true, otherwise false.
  bool UnmountFileSystem(const std::string& extension_id,
                         const std::string& file_system_id,
                         UnmountReason reason);

  // Requests unmounting of the file system. The callback is called when the
  // request is accepted or rejected, with an error code. Returns false if the
  // request could not been created, true otherwise.
  bool RequestUnmount(const std::string& extension_id,
                      const std::string& file_system_id);

  // Returns a list of information of all currently provided file systems. All
  // items are copied.
  std::vector<ProvidedFileSystemInfo> GetProvidedFileSystemInfoList();

  // Returns a provided file system with |file_system_id|, handled by
  // the extension with |extension_id|. If not found, then returns NULL.
  ProvidedFileSystemInterface* GetProvidedFileSystem(
      const std::string& extension_id,
      const std::string& file_system_id);

  // Returns a provided file system attached to the the passed
  // |mount_point_name|. If not found, then returns NULL.
  ProvidedFileSystemInterface* GetProvidedFileSystem(
      const std::string& mount_point_name);

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Gets the singleton instance for the |context|.
  static Service* Get(content::BrowserContext* context);

  // extensions::ExtensionRegistryObserver overrides.
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) OVERRIDE;
  virtual void OnExtensionLoaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension) OVERRIDE;

 private:
  // Key is a pair of an extension id and file system id, which makes it
  // unique among the entire service instance.
  typedef std::pair<std::string, std::string> FileSystemKey;

  typedef std::map<FileSystemKey, ProvidedFileSystemInterface*>
      ProvidedFileSystemMap;
  typedef std::map<std::string, FileSystemKey> MountPointNameToKeyMap;

  // Called when the providing extension accepts or refuses a unmount request.
  // If |error| is equal to FILE_OK, then the request is accepted.
  void OnRequestUnmountStatus(const ProvidedFileSystemInfo& file_system_info,
                              base::File::Error error);

  // Remembers the file system in preferences, in order to remount after a
  // reboot.
  void RememberFileSystem(const ProvidedFileSystemInfo& file_system_info);

  // Removes the file system from preferences, so it is not remounmted anymore
  // after a reboot.
  void ForgetFileSystem(const std::string& extension_id,
                        const std::string& file_system_id);

  // Restores from preferences file systems mounted previously by the
  // |extension_id| providing extension.
  void RestoreFileSystems(const std::string& extension_id);

  Profile* profile_;
  extensions::ExtensionRegistry* extension_registry_;  // Not owned.
  FileSystemFactoryCallback file_system_factory_;
  ObserverList<Observer> observers_;
  ProvidedFileSystemMap file_system_map_;  // Owns pointers.
  MountPointNameToKeyMap mount_point_name_to_key_map_;
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<Service> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SERVICE_H_
