// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
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

  // Registers a file system provided by an extension with the |extension_id|.
  // For success, it returns a numeric file system id, which is an
  // auto-incremented non-zero value. For failures, it returns zero.
  int RegisterFileSystem(const std::string& extension_id,
                         const std::string& file_system_name);

  // Unregisters a file system with the specified |file_system_id| for the
  // |extension_id|. For success returns true, otherwise false.
  bool UnregisterFileSystem(const std::string& extension_id,
                            int file_system_id);

  // Returns a list of currently registered file systems. All items are copied.
  std::vector<ProvidedFileSystem> GetRegisteredFileSystems();

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Gets the singleton instance for the |context|.
  static Service* Get(content::BrowserContext* context);

 private:
  typedef std::map<int, ProvidedFileSystem> FileSystemMap;

  Profile* profile_;
  ObserverList<Observer> observers_;
  FileSystemMap file_systems_;
  int next_id_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SERVICE_H_
