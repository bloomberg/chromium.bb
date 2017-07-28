// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_BRIDGE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/arc/common/file_system.mojom.h"
#include "components/arc/instance_holder.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "storage/browser/fileapi/watcher_manager.h"

class BrowserContextKeyedServiceFactory;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class handles file system related IPC from the ARC container.
class ArcFileSystemBridge
    : public KeyedService,
      public mojom::FileSystemHost,
      public InstanceHolder<mojom::FileSystemInstance>::Observer {
 public:
  class Observer {
   public:
    virtual void OnDocumentChanged(
        int64_t watcher_id,
        storage::WatcherManager::ChangeType type) = 0;

   protected:
    virtual ~Observer() {}
  };

  ArcFileSystemBridge(content::BrowserContext* context,
                      ArcBridgeService* bridge_service);
  ~ArcFileSystemBridge() override;

  // Returns the factory instance for this class.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Returns the instance for the given BrowserContext, or nullptr if the
  // browser |context| is not allowed to use ARC.
  static ArcFileSystemBridge* GetForBrowserContext(
      content::BrowserContext* context);

  // Adds an observer.
  void AddObserver(Observer* observer);

  // Removes an observer.
  void RemoveObserver(Observer* observer);

  // FileSystemHost overrides:
  void OnDocumentChanged(int64_t watcher_id,
                         storage::WatcherManager::ChangeType type) override;

  // InstanceHolder<mojom::FileSystemInstance>::Observer overrides:
  void OnInstanceReady() override;

 private:
  ArcBridgeService* const bridge_service_;  // Owned by ArcServiceManager
  mojo::Binding<mojom::FileSystemHost> binding_;
  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ArcFileSystemBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_BRIDGE_H_
