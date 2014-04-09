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

namespace chromeos {
namespace file_system_provider {

class ServiceFactory;

// Manages and registers the fileSystemProvider service.
class Service : public KeyedService {
 public:
  explicit Service(Profile* profile);
  virtual ~Service();

  // Mounts a file system provided by an extension with the |extension_id|.
  // For success, it returns a numeric file system id, which is an
  // auto-incremented non-zero value. For failures, it returns zero.
  int MountFileSystem(const std::string& extension_id,
                      const std::string& file_system_name);

  // Unmounts a file system with the specified |file_system_id| for the
  // |extension_id|. For success returns true, otherwise false.
  bool UnmountFileSystem(const std::string& extension_id, int file_system_id);

  // Returns a list of currently mounted file systems. All items are copied.
  std::vector<ProvidedFileSystem> GetMountedFileSystems();

  // Handles successful response for the |request_id|. If |has_next| is false,
  // then the request is disposed, after handling the |response|. On error,
  // returns false, and the request is disposed.
  bool FulfillRequest(const std::string& extension_id,
                      int file_system_id,
                      int request_id,
                      scoped_ptr<base::DictionaryValue> result,
                      bool has_next);

  // Handles error response for the |request_id|. If handling the error fails,
  // returns false. Always disposes the request.
  bool RejectRequest(const std::string& extension_id,
                     int file_system_id,
                     int request_id,
                     base::File::Error error);

  // Requests unmounting of a file system with the passed |file_system_id|.
  // Returns true is unmounting has been requested. False, if the request is
  // invalid (eg. already unmounted).
  bool RequestUnmount(int file_system_id);

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Gets the singleton instance for the |context|.
  static Service* Get(content::BrowserContext* context);

  // BrowserContextKeyedService overrides.
  virtual void Shutdown() OVERRIDE;

 private:
  typedef std::map<int, ProvidedFileSystem> FileSystemMap;

  // Called when the providing extension calls the success callback for the
  // onUnmountRequested event.
  void OnRequestUnmountError(const ProvidedFileSystem& file_system,
                             base::File::Error error);

  RequestManager request_manager_;
  Profile* profile_;
  ObserverList<Observer> observers_;
  FileSystemMap file_systems_;
  int next_id_;
  base::WeakPtrFactory<Service> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SERVICE_H_
