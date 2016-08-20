// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_BLIMP_CLIENT_CONTEXT_IMPL_H_
#define BLIMP_CLIENT_CORE_BLIMP_CLIENT_CONTEXT_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "blimp/client/core/session/client_network_components.h"
#include "blimp/client/core/session/identity_source.h"
#include "blimp/client/core/session/network_event_observer.h"
#include "blimp/client/public/blimp_client_context.h"
#include "blimp/client/public/contents/blimp_contents.h"
#include "blimp/client/public/session/assignment.h"
#include "blimp/net/thread_pipe_manager.h"
#include "url/gurl.h"

namespace blimp {
namespace client {

class BlimpContentsManager;
class TabControlFeature;

// BlimpClientContextImpl is the implementation of the main context-class for
// the blimp client.
class BlimpClientContextImpl : public BlimpClientContext,
                               public NetworkEventObserver {
 public:
  // The |io_thread_task_runner| must be the task runner to use for IO
  // operations.
  // The |file_thread_task_runner| must be the task runner to use for file
  // operations.
  BlimpClientContextImpl(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner);
  ~BlimpClientContextImpl() override;

  // BlimpClientContext implementation.
  void SetDelegate(BlimpClientContextDelegate* delegate) override;
  std::unique_ptr<BlimpContents> CreateBlimpContents() override;
  void Connect() override;

  // NetworkEventObserver implementation.
  void OnConnected() override;
  void OnDisconnected(int result) override;

  TabControlFeature* GetTabControlFeature() const;

 protected:
  // Returns the URL to use for connections to the assigner. Used to construct
  // the AssignmentSource.
  virtual GURL GetAssignerURL();

 private:
  // Connect to assignment source with OAuth2 token to get an assignment.
  virtual void ConnectToAssignmentSource(const std::string& client_auth_token);

  // The AssignmentCallback for when an assignment is ready. This will trigger
  // a connection to the engine.
  virtual void ConnectWithAssignment(AssignmentRequestResult result,
                                     const Assignment& assignment);

  void RegisterFeatures();

  // Provides functionality from the embedder.
  BlimpClientContextDelegate* delegate_ = nullptr;

  // The task runner to use for IO operations.
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;

  // The task runner to use for file operations.
  scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner_;

  // The AssignmentSource is used when the user of BlimpClientContextImpl calls
  // Connect() to get a valid assignment and later connect to the engine.
  std::unique_ptr<AssignmentSource> assignment_source_;

  std::unique_ptr<TabControlFeature> tab_control_feature_;

  std::unique_ptr<BlimpContentsManager> blimp_contents_manager_;

  // Container struct for network components.
  // Must be deleted on the IO thread.
  std::unique_ptr<ClientNetworkComponents> net_components_;

  std::unique_ptr<ThreadPipeManager> thread_pipe_manager_;

  // Provide OAuth2 token and propagate account sign in states change.
  std::unique_ptr<IdentitySource> identity_source_;

  base::WeakPtrFactory<BlimpClientContextImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientContextImpl);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_BLIMP_CLIENT_CONTEXT_IMPL_H_
