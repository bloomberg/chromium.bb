// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTEXT_BLIMP_CLIENT_CONTEXT_IMPL_H_
#define BLIMP_CLIENT_CORE_CONTEXT_BLIMP_CLIENT_CONTEXT_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "blimp/client/core/compositor/blob_image_serialization_processor.h"
#include "blimp/client/core/session/client_network_components.h"
#include "blimp/client/core/session/connection_status.h"
#include "blimp/client/core/session/identity_source.h"
#include "blimp/client/core/settings/blimp_settings_delegate.h"
#include "blimp/client/public/blimp_client_context.h"
#include "blimp/client/public/contents/blimp_contents.h"
#include "blimp/client/public/session/assignment.h"
#include "blimp/net/thread_pipe_manager.h"
#include "url/gurl.h"

namespace blimp {
namespace client {

class BlimpCompositorDependencies;
class BlimpContentsManager;
class BlobChannelFeature;
class CompositorDependencies;
class GeolocationFeature;
class ImeFeature;
class NavigationFeature;
class RenderWidgetFeature;
class SettingsFeature;
class TabControlFeature;

// BlimpClientContextImpl is the implementation of the main context-class for
// the blimp client.
class BlimpClientContextImpl
    : public BlimpClientContext,
      public BlimpSettingsDelegate,
      public BlobImageSerializationProcessor::ErrorDelegate {
 public:
  // The |io_thread_task_runner| must be the task runner to use for IO
  // operations.
  // The |file_thread_task_runner| must be the task runner to use for file
  // operations.
  BlimpClientContextImpl(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
      std::unique_ptr<CompositorDependencies> compositor_dependencies);
  ~BlimpClientContextImpl() override;

  // BlimpClientContext implementation.
  void SetDelegate(BlimpClientContextDelegate* delegate) override;
  std::unique_ptr<BlimpContents> CreateBlimpContents(
      gfx::NativeWindow window) override;
  void Connect() override;
  void ConnectWithAssignment(const Assignment& assignment) override;

  // Creates a data object containing data about Blimp to be used for feedback.
  std::unordered_map<std::string, std::string> CreateFeedbackData();

 protected:
  // Returns the URL to use for connections to the assigner. Used to construct
  // the AssignmentSource.
  virtual GURL GetAssignerURL();

  // BlimpSettingsDelegate implementation.
  IdentitySource* GetIdentitySource() override;
  ConnectionStatus* GetConnectionStatus() override;

 private:
  // Called when an OAuth2 token is received.  Will then ask the
  // AssignmentSource for an Assignment with this token.
  virtual void OnAuthTokenReceived(const std::string& client_auth_token);

  // Called when the AssignmentSource is finished getting an Assignment.  Will
  // then call |ConnectWithAssignment| to initiate the actual connection.
  virtual void OnAssignmentReceived(AssignmentRequestResult result,
                                    const Assignment& assignment);

  void RegisterFeatures();
  void InitializeSettings();

  // Terminates the active connection held by |net_connections_|.
  // May be called on any thread.
  void DropConnection();

  // Create IdentitySource which provides user sign in states and OAuth2 token
  // service.
  void CreateIdentitySource();

  // BlobImageSerializationProcessor::ErrorDelegate implementation.
  void OnImageDecodeError() override;

  // Provides functionality from the embedder.
  BlimpClientContextDelegate* delegate_ = nullptr;

  // The task runner to use for IO operations.
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;

  // The task runner to use for file operations.
  scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner_;

  // The AssignmentSource is used when the user of BlimpClientContextImpl calls
  // Connect() to get a valid assignment and later connect to the engine.
  std::unique_ptr<AssignmentSource> assignment_source_;

  // A set of dependencies required by all BlimpCompositor instances.
  std::unique_ptr<BlimpCompositorDependencies> blimp_compositor_dependencies_;

  // Features to handle all incoming and outgoing protobuf messages.
  std::unique_ptr<BlobChannelFeature> blob_channel_feature_;
  std::unique_ptr<GeolocationFeature> geolocation_feature_;
  std::unique_ptr<ImeFeature> ime_feature_;
  std::unique_ptr<NavigationFeature> navigation_feature_;
  std::unique_ptr<RenderWidgetFeature> render_widget_feature_;
  std::unique_ptr<SettingsFeature> settings_feature_;
  std::unique_ptr<TabControlFeature> tab_control_feature_;

  std::unique_ptr<BlimpContentsManager> blimp_contents_manager_;

  // Container struct for network components.
  // Must be deleted on the IO thread.
  std::unique_ptr<ClientNetworkComponents> net_components_;

  std::unique_ptr<ThreadPipeManager> thread_pipe_manager_;

  // Provide OAuth2 token and propagate account sign in states change.
  std::unique_ptr<IdentitySource> identity_source_;

  // Connection status to the engine.
  ConnectionStatus connection_status_;

  base::WeakPtrFactory<BlimpClientContextImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientContextImpl);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTEXT_BLIMP_CLIENT_CONTEXT_IMPL_H_
