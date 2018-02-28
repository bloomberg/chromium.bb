// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/tracked_child_url_loader_factory_bundle.h"

#include "content/public/renderer/render_thread.h"

namespace content {

TrackedChildURLLoaderFactoryBundleInfo::
    TrackedChildURLLoaderFactoryBundleInfo() = default;

TrackedChildURLLoaderFactoryBundleInfo::TrackedChildURLLoaderFactoryBundleInfo(
    network::mojom::URLLoaderFactoryPtrInfo default_factory_info,
    std::map<std::string, network::mojom::URLLoaderFactoryPtrInfo>
        factories_info,
    PossiblyAssociatedURLLoaderFactoryPtrInfo direct_network_factory_info,
    std::unique_ptr<HostPtrAndTaskRunner> main_thread_host_bundle)
    : ChildURLLoaderFactoryBundleInfo(std::move(default_factory_info),
                                      std::move(factories_info),
                                      std::move(direct_network_factory_info)),
      main_thread_host_bundle_(std::move(main_thread_host_bundle)) {}

TrackedChildURLLoaderFactoryBundleInfo::
    ~TrackedChildURLLoaderFactoryBundleInfo() = default;

scoped_refptr<SharedURLLoaderFactory>
TrackedChildURLLoaderFactoryBundleInfo::CreateFactory() {
  auto other = std::make_unique<TrackedChildURLLoaderFactoryBundleInfo>();
  other->default_factory_info_ = std::move(default_factory_info_);
  other->factories_info_ = std::move(factories_info_);
  other->direct_network_factory_info_ = std::move(direct_network_factory_info_);
  other->main_thread_host_bundle_ = std::move(main_thread_host_bundle_);

  return base::MakeRefCounted<TrackedChildURLLoaderFactoryBundle>(
      std::move(other));
}

// -----------------------------------------------------------------------------

TrackedChildURLLoaderFactoryBundle::TrackedChildURLLoaderFactoryBundle(
    std::unique_ptr<TrackedChildURLLoaderFactoryBundleInfo> info) {
  DCHECK(info->main_thread_host_bundle());
  main_thread_host_bundle_ = std::move(info->main_thread_host_bundle());
  Update(std::move(info));
  AddObserverOnMainThread();
}

TrackedChildURLLoaderFactoryBundle::~TrackedChildURLLoaderFactoryBundle() {
  RemoveObserverOnMainThread();
};

std::unique_ptr<SharedURLLoaderFactoryInfo>
TrackedChildURLLoaderFactoryBundle::Clone() {
  auto info = base::WrapUnique(static_cast<ChildURLLoaderFactoryBundleInfo*>(
      ChildURLLoaderFactoryBundle::Clone().release()));

  DCHECK(main_thread_host_bundle_);

  auto main_thread_host_bundle_clone = std::make_unique<HostPtrAndTaskRunner>(
      main_thread_host_bundle_->first, main_thread_host_bundle_->second);

  return std::make_unique<TrackedChildURLLoaderFactoryBundleInfo>(
      std::move(info->default_factory_info()),
      std::move(info->factories_info()),
      std::move(info->direct_network_factory_info()),
      std::move(main_thread_host_bundle_clone));
}

void TrackedChildURLLoaderFactoryBundle::AddObserverOnMainThread() {
  DCHECK(main_thread_host_bundle_);

  // TODO(chongz): Change this to a DCHECK once all call sites have a
  // SequencedTaskRunnerHandle.
  if (!base::SequencedTaskRunnerHandle::IsSet())
    return;

  main_thread_host_bundle_->second->PostTask(
      FROM_HERE,
      base::BindOnce(
          &HostChildURLLoaderFactoryBundle::AddObserver,
          main_thread_host_bundle_->first, base::Unretained(this),
          std::make_unique<
              HostChildURLLoaderFactoryBundle::ObserverPtrAndTaskRunner>(
              AsWeakPtr(), base::SequencedTaskRunnerHandle::Get())));
}

void TrackedChildURLLoaderFactoryBundle::RemoveObserverOnMainThread() {
  DCHECK(main_thread_host_bundle_);

  main_thread_host_bundle_->second->PostTask(
      FROM_HERE,
      base::BindOnce(&HostChildURLLoaderFactoryBundle::RemoveObserver,
                     main_thread_host_bundle_->first, base::Unretained(this)));
}

void TrackedChildURLLoaderFactoryBundle::OnUpdate(
    std::unique_ptr<SharedURLLoaderFactoryInfo> info) {
  Update(base::WrapUnique(
      static_cast<ChildURLLoaderFactoryBundleInfo*>(info.release())));
}

// -----------------------------------------------------------------------------

HostChildURLLoaderFactoryBundle::HostChildURLLoaderFactoryBundle()
    : observer_list_(std::make_unique<ObserverList>()) {
  DCHECK(RenderThread::Get()) << "HostChildURLLoaderFactoryBundle should live "
                                 "on the main renderer thread";
}

HostChildURLLoaderFactoryBundle::~HostChildURLLoaderFactoryBundle() = default;

std::unique_ptr<SharedURLLoaderFactoryInfo>
HostChildURLLoaderFactoryBundle::Clone() {
  auto info = base::WrapUnique(static_cast<ChildURLLoaderFactoryBundleInfo*>(
      ChildURLLoaderFactoryBundle::Clone().release()));

  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  auto main_thread_host_bundle_clone = std::make_unique<
      TrackedChildURLLoaderFactoryBundle::HostPtrAndTaskRunner>(
      AsWeakPtr(), base::SequencedTaskRunnerHandle::Get());

  return std::make_unique<TrackedChildURLLoaderFactoryBundleInfo>(
      std::move(info->default_factory_info()),
      std::move(info->factories_info()),
      std::move(info->direct_network_factory_info()),
      std::move(main_thread_host_bundle_clone));
}

void HostChildURLLoaderFactoryBundle::UpdateThisAndAllClones(
    std::unique_ptr<URLLoaderFactoryBundleInfo> info) {
  DCHECK(RenderThread::Get()) << "Should run on the main renderer thread";
  DCHECK(observer_list_);

  auto partial_bundle = base::MakeRefCounted<ChildURLLoaderFactoryBundle>();
  static_cast<URLLoaderFactoryBundle*>(partial_bundle.get())
      ->Update(std::move(info));

  for (const auto& iter : *observer_list_) {
    NotifyUpdateOnMainOrWorkerThread(iter.second.get(),
                                     partial_bundle->Clone());
  }

  Update(partial_bundle->PassInterface());
}

bool HostChildURLLoaderFactoryBundle::IsHostChildURLLoaderFactoryBundle()
    const {
  return true;
}

void HostChildURLLoaderFactoryBundle::AddObserver(
    TrackedChildURLLoaderFactoryBundle* observer,
    std::unique_ptr<ObserverPtrAndTaskRunner> observer_info) {
  DCHECK(RenderThread::Get()) << "Should run in the main renderer thread";
  DCHECK(observer_list_);
  (*observer_list_)[observer] = std::move(observer_info);
}

void HostChildURLLoaderFactoryBundle::RemoveObserver(
    TrackedChildURLLoaderFactoryBundle* observer) {
  DCHECK(RenderThread::Get()) << "Should run in the main renderer thread";
  DCHECK(observer_list_);
  observer_list_->erase(observer);
}

void HostChildURLLoaderFactoryBundle::NotifyUpdateOnMainOrWorkerThread(
    ObserverPtrAndTaskRunner* observer_bundle,
    std::unique_ptr<SharedURLLoaderFactoryInfo> update_info) {
  observer_bundle->second->PostTask(
      FROM_HERE,
      base::BindOnce(&TrackedChildURLLoaderFactoryBundle::OnUpdate,
                     observer_bundle->first, std::move(update_info)));
}

}  // namespace content
