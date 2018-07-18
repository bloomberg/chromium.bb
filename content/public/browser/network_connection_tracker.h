// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NETWORK_CONNECTION_TRACKER_H_
#define CONTENT_PUBLIC_BROWSER_NETWORK_CONNECTION_TRACKER_H_

#include <list>
#include <memory>

#include "base/atomicops.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list_threadsafe.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace content {

// This class subscribes to network change events from
// network::mojom::NetworkChangeManager and propogates these notifications to
// its NetworkConnectionObservers registered through
// AddNetworkConnectionObserver()/RemoveNetworkConnectionObserver().
class CONTENT_EXPORT NetworkConnectionTracker
    : public network::mojom::NetworkChangeManagerClient {
 public:
  using ConnectionTypeCallback =
      base::OnceCallback<void(network::mojom::ConnectionType)>;

  class CONTENT_EXPORT NetworkConnectionObserver {
   public:
    // Please refer to NetworkChangeManagerClient::OnNetworkChanged for when
    // this method is invoked.
    virtual void OnConnectionChanged(network::mojom::ConnectionType type) = 0;

   protected:
    virtual ~NetworkConnectionObserver() {}
  };

  // Constructs a NetworkConnectionTracker. |callback| should return the network
  // service that is in use. NetworkConnectionTracker does not need to be
  // destroyed before the network service.
  explicit NetworkConnectionTracker(
      base::RepeatingCallback<network::mojom::NetworkService*()> callback);

  ~NetworkConnectionTracker() override;

  // If connection type can be retrieved synchronously, returns true and |type|
  // will contain the current connection type; Otherwise, returns false and
  // does not modify |type|, in which case, |callback| will be called on the
  // calling thread when connection type is ready. This method is thread safe.
  // Please also refer to net::NetworkChangeNotifier::GetConnectionType() for
  // documentation.
  virtual bool GetConnectionType(network::mojom::ConnectionType* type,
                                 ConnectionTypeCallback callback);

  // Returns true if |type| is a cellular connection.
  // Returns false if |type| is CONNECTION_UNKNOWN, and thus, depending on the
  // implementation of GetConnectionType(), it is possible that
  // IsConnectionCellular(GetConnectionType()) returns false even if the
  // current connection is cellular.
  static bool IsConnectionCellular(network::mojom::ConnectionType type);

  // Registers |observer| to receive notifications of network changes. The
  // thread on which this is called is the thread on which |observer| will be
  // called back with notifications.
  void AddNetworkConnectionObserver(NetworkConnectionObserver* observer);

  // Unregisters |observer| from receiving notifications.  This must be called
  // on the same thread on which AddNetworkConnectionObserver() was called.
  // All observers must be unregistered before |this| is destroyed.
  void RemoveNetworkConnectionObserver(NetworkConnectionObserver* observer);

 protected:
  // Constructor used in testing to mock out network service.
  NetworkConnectionTracker();

  // NetworkChangeManagerClient implementation. Protected for testing.
  void OnInitialConnectionType(network::mojom::ConnectionType type) override;
  void OnNetworkChanged(network::mojom::ConnectionType type) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(NetworkGetConnectionTest,
                           GetConnectionTypeOnDifferentThread);

  // Starts listening for connection change notifications from
  // |network_service|. Observers may be added and GetConnectionType called, but
  // no network information will be provided until this method is called. For
  // unit tests, this class can be subclassed, and OnInitialConnectionType /
  // OnNetworkChanged may be called directly, instead of providing a
  // NetworkService.
  void Initialize();

  // Serves as a connection error handler, and is invoked when network service
  // restarts.
  void HandleNetworkServicePipeBroken();

  // Callback to get the current network service raw mojo pointer. This is to
  // ensure that |this| can survive crashes and restarts of network service.
  const base::RepeatingCallback<network::mojom::NetworkService*()>
      get_network_service_callback_;

  // The task runner that |this| lives on.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Protect access to |connection_type_callbacks_|.
  base::Lock lock_;

  // Saves user callback if GetConnectionType() cannot complete synchronously.
  std::list<ConnectionTypeCallback> connection_type_callbacks_;

  // |connection_type_| is set on one thread but read on many threads.
  // The default value is -1 before OnInitialConnectionType().
  base::subtle::Atomic32 connection_type_;

  const scoped_refptr<base::ObserverListThreadSafe<NetworkConnectionObserver>>
      network_change_observer_list_;

  mojo::Binding<network::mojom::NetworkChangeManagerClient> binding_;

  // Only the initialization and re-initialization of |this| are required to
  // be bound to the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(NetworkConnectionTracker);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NETWORK_CONNECTION_TRACKER_H_
