// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_

#include <list>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/common/shared_worker/shared_worker.mojom.h"
#include "content/common/shared_worker/shared_worker_client.mojom.h"
#include "content/common/shared_worker/shared_worker_factory.mojom.h"
#include "content/common/shared_worker/shared_worker_host.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"

class GURL;

namespace blink {
class MessagePortChannel;
}

namespace content {

class SharedWorkerContentSettingsProxyImpl;
class SharedWorkerInstance;

// The SharedWorkerHost is the interface that represents the browser side of
// the browser <-> worker communication channel. This is owned by
// SharedWorkerServiceImpl and destructed when a worker context or worker's
// message filter is closed.
class SharedWorkerHost : public mojom::SharedWorkerHost,
                         public service_manager::mojom::InterfaceProvider {
 public:
  SharedWorkerHost(std::unique_ptr<SharedWorkerInstance> instance,
                   int process_id,
                   int route_id);
  ~SharedWorkerHost() override;

  // Starts the SharedWorker in the renderer process.
  void Start(mojom::SharedWorkerFactoryPtr factory, bool pause_on_start);

  void AllowFileSystem(const GURL& url,
                       base::OnceCallback<void(bool)> callback);
  void AllowIndexedDB(const GURL& url,
                      const base::string16& name,
                      base::OnceCallback<void(bool)> callback);

  // Terminates the given worker, i.e. based on a UI action.
  void TerminateWorker();

  void AddClient(mojom::SharedWorkerClientPtr client,
                 int process_id,
                 int frame_id,
                 const blink::MessagePortChannel& port);

  // Returns true if any clients live in a different process from this worker.
  bool ServesExternalClient();

  SharedWorkerInstance* instance() { return instance_.get(); }
  int process_id() const { return process_id_; }
  int route_id() const { return route_id_; }
  bool IsAvailable() const;

 private:
  struct ClientInfo {
    ClientInfo(mojom::SharedWorkerClientPtr client,
               int connection_request_id,
               int process_id,
               int frame_id);
    ~ClientInfo();
    mojom::SharedWorkerClientPtr client;
    const int connection_request_id;
    const int process_id;
    const int frame_id;
  };

  using ClientList = std::list<ClientInfo>;

  // mojom::SharedWorkerHost methods:
  void OnConnected(int connection_request_id) override;
  void OnContextClosed() override;
  void OnReadyForInspection() override;
  void OnScriptLoaded() override;
  void OnScriptLoadFailed() override;
  void OnFeatureUsed(blink::mojom::WebFeature feature) override;

  // Return a vector of all the render process/render frame IDs.
  std::vector<std::pair<int, int>> GetRenderFrameIDsForWorker();

  void AllowFileSystemResponse(base::OnceCallback<void(bool)> callback,
                               bool allowed);
  void OnClientConnectionLost();
  void OnWorkerConnectionLost();

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override;

  mojo::Binding<mojom::SharedWorkerHost> binding_;
  std::unique_ptr<SharedWorkerInstance> instance_;
  ClientList clients_;

  mojom::SharedWorkerPtr worker_;

  const int process_id_;
  const int route_id_;
  int next_connection_request_id_;
  bool termination_message_sent_ = false;
  bool closed_ = false;
  const base::TimeTicks creation_time_;

  // This is the set of features that this worker has used.
  std::set<blink::mojom::WebFeature> used_features_;

  std::unique_ptr<SharedWorkerContentSettingsProxyImpl> content_settings_;

  mojo::Binding<service_manager::mojom::InterfaceProvider>
      interface_provider_binding_;

  base::WeakPtrFactory<SharedWorkerHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_
