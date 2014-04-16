// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace extensions {
class EventRouter;
}  // namespace extensions

namespace chromeos {
namespace file_system_provider {

class ProvidedFileSystemFactoryInterface;
class ProvidedFileSystemInfo;
class ProvidedFileSystemInterface;
class ServiceFactory;

// Manages and registers the file system provider service. Maintains provided
// file systems.
class Service : public KeyedService {
 public:
  typedef base::Callback<ProvidedFileSystemInterface*(
      extensions::EventRouter* event_router,
      RequestManager* request_manager,
      const ProvidedFileSystemInfo& file_system_info)>
      FileSystemFactoryCallback;

  explicit Service(Profile* profile);
  virtual ~Service();

  // Sets a custom ProvidedFileSystemInterface factory. Used by unit tests,
  // where an event router is not available.
  void SetFileSystemFactoryForTests(
      const FileSystemFactoryCallback& factory_callback);

  // Mounts a file system provided by an extension with the |extension_id|.
  // For success, it returns a numeric file system id, which is an
  // auto-incremented non-zero value. For failures, it returns zero.
  int MountFileSystem(const std::string& extension_id,
                      const std::string& file_system_name);

  // Unmounts a file system with the specified |file_system_id| for the
  // |extension_id|. For success returns true, otherwise false.
  bool UnmountFileSystem(const std::string& extension_id, int file_system_id);

  // Requests unmounting of the file system. The callback is called when the
  // request is accepted or rejected, with an error code. Returns false if the
  // request could not been created, true otherwise.
  bool RequestUnmount(int file_system_id);

  // Returns a list of information of all currently provided file systems. All
  // items are copied.
  std::vector<ProvidedFileSystemInfo> GetProvidedFileSystemInfoList();

  // Returns a provided file system with |file_system_id|, handled by
  // the extension with |extension_id|. If not found, then returns NULL.
  ProvidedFileSystemInterface* GetProvidedFileSystem(
      const std::string& extension_id,
      int file_system_id);

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Gets the singleton instance for the |context|.
  static Service* Get(content::BrowserContext* context);

  // BrowserContextKeyedService overrides.
  virtual void Shutdown() OVERRIDE;

  // Getter for the request manager. Used by the extension API to forward
  // replies. Valid as long as the service.
  RequestManager* request_manager() { return &request_manager_; }

 private:
  typedef std::map<int, ProvidedFileSystemInterface*> ProvidedFileSystemMap;

  // Called when the providing extension accepts or refuses a unmount request.
  // If |error| is equal to FILE_OK, then the request is accepted.
  void OnRequestUnmountStatus(const ProvidedFileSystemInfo& file_system_info,
                              base::File::Error error);

  RequestManager request_manager_;
  Profile* profile_;
  FileSystemFactoryCallback file_system_factory_;
  ObserverList<Observer> observers_;
  ProvidedFileSystemMap file_system_map_;  // Owns pointers.
  int next_id_;
  base::WeakPtrFactory<Service> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SERVICE_H_
