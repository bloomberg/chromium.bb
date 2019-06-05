// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_ENVIRONMENT_IMPL_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_ENVIRONMENT_IMPL_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/common/content_export.h"
#include "content/public/browser/authenticator_environment.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom.h"

namespace device {
class FidoDiscoveryFactory;
}

namespace content {

class VirtualFidoDiscovery;
class VirtualFidoDiscoveryFactory;

// Allows enabling and disabling per-frame virtual environments for the Web
// Authentication API. Disabling the environment resets its state.
//
// This class is a singleton.
class CONTENT_EXPORT AuthenticatorEnvironmentImpl
    : public AuthenticatorEnvironment,
      FrameTreeNode::Observer {
 public:
  static AuthenticatorEnvironmentImpl* GetInstance();

  // Returns a FidoDiscoveryFactory for the given node.
  device::FidoDiscoveryFactory* GetFactory(FrameTreeNode* node);

  // Returns the default FidoDiscoveryFactory.
  device::FidoDiscoveryFactory* GetFactory();

  // Enables the scoped virtual authenticator environment for the |node| and its
  // descendants.
  // Does not have any effect if the |node| already has the virtual environment
  // enabled.
  void EnableVirtualAuthenticatorFor(FrameTreeNode* node);

  // Disables the scoped virtual authenticator environment for this |node|,
  // resetting the state. If the environment is set on one of the |node|'s
  // parents instead, this won't have any effect.
  void DisableVirtualAuthenticatorFor(FrameTreeNode* node);

  // Returns the virtual fido discovery factory for the |node| if the virtual
  // environment is enabled for it, otherwise returns nullptr.
  VirtualFidoDiscoveryFactory* GetVirtualFactoryFor(FrameTreeNode* node);

  // Binds the request to the virtual authenticator enabled for the |node|. The
  // virtual authenticator must be enabled beforehand.
  void AddVirtualAuthenticatorBinding(
      FrameTreeNode* node,
      blink::test::mojom::VirtualAuthenticatorManagerRequest request);

  // Called by VirtualFidoDiscoveries when they are destructed.
  void OnDiscoveryDestroyed(VirtualFidoDiscovery* discovery);

  // AuthenticatorEnvironment:
  void ReplaceDefaultDiscoveryFactoryForTesting(
      std::unique_ptr<device::FidoDiscoveryFactory> factory) override;

  // FrameTreeNode::Observer:
  void OnFrameTreeNodeDestroyed(FrameTreeNode* node) override;

 protected:
  AuthenticatorEnvironmentImpl();
  ~AuthenticatorEnvironmentImpl() override;

 private:
  friend class base::NoDestructor<AuthenticatorEnvironmentImpl>;

  std::unique_ptr<device::FidoDiscoveryFactory> discovery_factory_;

  std::map<FrameTreeNode*, std::unique_ptr<VirtualFidoDiscoveryFactory>>
      virtual_discovery_factories_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorEnvironmentImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_ENVIRONMENT_IMPL_H_
