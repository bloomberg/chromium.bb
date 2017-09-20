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
#include "content/common/shared_worker/shared_worker_client.mojom.h"

class GURL;

namespace IPC {
class Message;
}

namespace content {

class MessagePort;
class SharedWorkerContentSettingsProxyImpl;
class SharedWorkerInstance;
class SharedWorkerMessageFilter;

// The SharedWorkerHost is the interface that represents the browser side of
// the browser <-> worker communication channel. This is owned by
// SharedWorkerServiceImpl and destructed when a worker context or worker's
// message filter is closed.
//
// NOTE: This class is intended to only be used on the IO thread.
//
class SharedWorkerHost {
 public:
  SharedWorkerHost(SharedWorkerInstance* instance,
                   SharedWorkerMessageFilter* filter,
                   int worker_route_id);
  ~SharedWorkerHost();

  // Starts the SharedWorker in the renderer process.
  void Start(bool pause_on_start);

  void CountFeature(blink::mojom::WebFeature feature);
  void WorkerContextClosed();
  void WorkerContextDestroyed();
  void WorkerReadyForInspection();
  void WorkerScriptLoaded();
  void WorkerScriptLoadFailed();
  void WorkerConnected(int connection_request_id);
  void AllowFileSystem(const GURL& url,
                       base::OnceCallback<void(bool)> callback);
  bool AllowIndexedDB(const GURL& url, const base::string16& name);

  // Terminates the given worker, i.e. based on a UI action.
  void TerminateWorker();

  void AddClient(mojom::SharedWorkerClientPtr client,
                 int process_id,
                 int frame_id,
                 const MessagePort& port);

  // Returns true if any clients live in a different process from this worker.
  bool ServesExternalClient();

  SharedWorkerInstance* instance() { return instance_.get(); }
  SharedWorkerMessageFilter* worker_render_filter() const {
    return worker_render_filter_;
  }
  int process_id() const { return worker_process_id_; }
  int worker_route_id() const { return worker_route_id_; }
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

  // Return a vector of all the render process/render frame IDs.
  std::vector<std::pair<int, int>> GetRenderFrameIDsForWorker();

  void AllowFileSystemResponse(base::OnceCallback<void(bool)> callback,
                               bool allowed);
  void OnClientConnectionLost();

  // Sends |message| to the SharedWorker.
  bool Send(IPC::Message* message);

  std::unique_ptr<SharedWorkerInstance> instance_;
  ClientList clients_;

  // A message filter for a renderer process that hosts a worker. This is always
  // valid because this host is destructed immediately after the filter is
  // closed (see SharedWorkerServiceImpl::OnSharedWorkerMessageFilterClosing).
  SharedWorkerMessageFilter* worker_render_filter_;

  const int worker_process_id_;
  const int worker_route_id_;
  int next_connection_request_id_;
  bool termination_message_sent_ = false;
  bool closed_ = false;
  const base::TimeTicks creation_time_;

  // This is the set of features that this worker has used.
  std::set<blink::mojom::WebFeature> used_features_;

  std::unique_ptr<SharedWorkerContentSettingsProxyImpl> content_settings_;
  base::WeakPtrFactory<SharedWorkerHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerHost);
};
}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_
