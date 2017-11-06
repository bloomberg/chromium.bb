// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/network_connection_tracker.h"

#include <utility>

#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/common/network_service.mojom.h"

namespace content {

namespace {

// Wraps a |user_callback| when GetConnectionType() is called on a different
// thread than NetworkConnectionTracker's thread.
void OnGetConnectionType(
    scoped_refptr<base::TaskRunner> task_runner,
    NetworkConnectionTracker::ConnectionTypeCallback user_callback,
    mojom::ConnectionType connection_type) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](NetworkConnectionTracker::ConnectionTypeCallback callback,
             mojom::ConnectionType type) { std::move(callback).Run(type); },
          std::move(user_callback), connection_type));
}

static const int32_t kConnectionTypeInvalid = -1;

}  // namespace

NetworkConnectionTracker::NetworkConnectionTracker(
    mojom::NetworkService* network_service)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      connection_type_(kConnectionTypeInvalid),
      network_change_observer_list_(
          new base::ObserverListThreadSafe<NetworkConnectionObserver>(
              base::ObserverListBase<
                  NetworkConnectionObserver>::NOTIFY_EXISTING_ONLY)),
      binding_(this) {
  DCHECK(network_service);
  // Get NetworkChangeManagerPtr.
  mojom::NetworkChangeManagerPtr manager_ptr;
  mojom::NetworkChangeManagerRequest request(mojo::MakeRequest(&manager_ptr));
  network_service->GetNetworkChangeManager(std::move(request));

  // Request notification from NetworkChangeManagerPtr.
  mojom::NetworkChangeManagerClientPtr client_ptr;
  mojom::NetworkChangeManagerClientRequest client_request(
      mojo::MakeRequest(&client_ptr));
  binding_.Bind(std::move(client_request));
  manager_ptr->RequestNotifications(std::move(client_ptr));
}

NetworkConnectionTracker::~NetworkConnectionTracker() {
  network_change_observer_list_->AssertEmpty();
}

bool NetworkConnectionTracker::GetConnectionType(
    mojom::ConnectionType* type,
    ConnectionTypeCallback callback) {
  // |connection_type_| is initialized when NetworkService starts up. In most
  // cases, it won't be kConnectionTypeInvalid and code will return early.
  base::subtle::Atomic32 type_value =
      base::subtle::NoBarrier_Load(&connection_type_);
  if (type_value != kConnectionTypeInvalid) {
    *type = static_cast<mojom::ConnectionType>(type_value);
    return true;
  }
  base::AutoLock lock(lock_);
  // Check again after getting the lock, and return early if
  // OnInitialConnectionType() is called after first NoBarrier_Load.
  type_value = base::subtle::NoBarrier_Load(&connection_type_);
  if (type_value != kConnectionTypeInvalid) {
    *type = static_cast<mojom::ConnectionType>(type_value);
    return true;
  }
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    connection_type_callbacks_.push_back(base::BindOnce(
        &OnGetConnectionType, base::ThreadTaskRunnerHandle::Get(),
        std::move(callback)));
  } else {
    connection_type_callbacks_.push_back(std::move(callback));
  }
  return false;
}

// static
bool NetworkConnectionTracker::IsConnectionCellular(
    mojom::ConnectionType type) {
  bool is_cellular = false;
  switch (type) {
    case mojom::ConnectionType::CONNECTION_2G:
    case mojom::ConnectionType::CONNECTION_3G:
    case mojom::ConnectionType::CONNECTION_4G:
      is_cellular = true;
      break;
    case mojom::ConnectionType::CONNECTION_UNKNOWN:
    case mojom::ConnectionType::CONNECTION_ETHERNET:
    case mojom::ConnectionType::CONNECTION_WIFI:
    case mojom::ConnectionType::CONNECTION_NONE:
    case mojom::ConnectionType::CONNECTION_BLUETOOTH:
      is_cellular = false;
      break;
  }
  return is_cellular;
}

void NetworkConnectionTracker::AddNetworkConnectionObserver(
    NetworkConnectionObserver* observer) {
  network_change_observer_list_->AddObserver(observer);
}

void NetworkConnectionTracker::RemoveNetworkConnectionObserver(
    NetworkConnectionObserver* observer) {
  network_change_observer_list_->RemoveObserver(observer);
}

void NetworkConnectionTracker::OnInitialConnectionType(
    mojom::ConnectionType type) {
  base::AutoLock lock(lock_);
  base::subtle::NoBarrier_Store(&connection_type_,
                                static_cast<base::subtle::Atomic32>(type));
  while (!connection_type_callbacks_.empty()) {
    std::move(connection_type_callbacks_.front()).Run(type);
    connection_type_callbacks_.pop_front();
  }
}

void NetworkConnectionTracker::OnNetworkChanged(mojom::ConnectionType type) {
  base::subtle::NoBarrier_Store(&connection_type_,
                                static_cast<base::subtle::Atomic32>(type));
  network_change_observer_list_->Notify(
      FROM_HERE, &NetworkConnectionObserver::OnConnectionChanged, type);
}

}  // namespace content
