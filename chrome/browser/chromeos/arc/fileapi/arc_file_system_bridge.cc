// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_bridge.h"

#include "base/memory/singleton.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "content/public/browser/browser_thread.h"

namespace arc {

namespace {

// Factory of ArcFileSystemBridge.
class ArcFileSystemBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcFileSystemBridge,
          ArcFileSystemBridgeFactory> {
 public:
  static constexpr const char* kName = "ArcFileSystemBridgeFactory";

  static ArcFileSystemBridgeFactory* GetInstance() {
    return base::Singleton<ArcFileSystemBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcFileSystemBridgeFactory>;
  ArcFileSystemBridgeFactory() = default;
  ~ArcFileSystemBridgeFactory() override = default;
};

}  // namespace

ArcFileSystemBridge::ArcFileSystemBridge(content::BrowserContext* context,
                                         ArcBridgeService* bridge_service)
    : bridge_service_(bridge_service), binding_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bridge_service_->file_system()->AddObserver(this);
}

ArcFileSystemBridge::~ArcFileSystemBridge() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bridge_service_->file_system()->RemoveObserver(this);
}

// static
BrowserContextKeyedServiceFactory* ArcFileSystemBridge::GetFactory() {
  return ArcFileSystemBridgeFactory::GetInstance();
}

// static
ArcFileSystemBridge* ArcFileSystemBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ArcFileSystemBridgeFactory::GetForBrowserContext(context);
}

void ArcFileSystemBridge::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.AddObserver(observer);
}

void ArcFileSystemBridge::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.RemoveObserver(observer);
}

void ArcFileSystemBridge::OnDocumentChanged(
    int64_t watcher_id,
    storage::WatcherManager::ChangeType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto& observer : observer_list_)
    observer.OnDocumentChanged(watcher_id, type);
}

void ArcFileSystemBridge::OnInstanceReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* file_system_instance =
      ARC_GET_INSTANCE_FOR_METHOD(bridge_service_->file_system(), Init);
  if (file_system_instance) {
    mojom::FileSystemHostPtr host_proxy;
    binding_.Bind(mojo::MakeRequest(&host_proxy));
    file_system_instance->Init(std::move(host_proxy));
  }
}

}  // namespace arc
