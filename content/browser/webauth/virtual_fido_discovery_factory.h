// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_VIRTUAL_FIDO_DISCOVERY_FACTORY_H_
#define CONTENT_BROWSER_WEBAUTH_VIRTUAL_FIDO_DISCOVERY_FACTORY_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/virtual_fido_device.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom.h"

namespace device {
class FidoDiscoveryBase;
}

namespace content {

class VirtualAuthenticator;
class VirtualFidoDiscovery;

// Implements the Mojo interface representing a virtual authenticator manager
// for the Web Authentication API. Allows setting up and configurating virtual
// authenticator devices for testing.
class CONTENT_EXPORT VirtualFidoDiscoveryFactory
    : public blink::test::mojom::VirtualAuthenticatorManager,
      public device::FidoDiscoveryFactory {
 public:
  VirtualFidoDiscoveryFactory();
  ~VirtualFidoDiscoveryFactory() override;

  void AddBinding(
      blink::test::mojom::VirtualAuthenticatorManagerRequest request);

  // Notify that a discovery has been destroyed.
  void OnDiscoveryDestroyed(VirtualFidoDiscovery* discovery);

  // device::FidoDiscoveryFactory:
  std::unique_ptr<::device::FidoDiscoveryBase> Create(
      device::FidoTransportProtocol transport,
      ::service_manager::Connector* connector) override;
  std::unique_ptr<::device::FidoDiscoveryBase> CreateCable(
      std::vector<device::CableDiscoveryData> cable_data) override;

 protected:
  // blink::test::mojom::VirtualAuthenticatorManager:
  void CreateAuthenticator(
      blink::test::mojom::VirtualAuthenticatorOptionsPtr options,
      CreateAuthenticatorCallback callback) override;
  void GetAuthenticators(GetAuthenticatorsCallback callback) override;
  void RemoveAuthenticator(const std::string& id,
                           RemoveAuthenticatorCallback callback) override;
  void ClearAuthenticators(ClearAuthenticatorsCallback callback) override;

 private:
  mojo::BindingSet<blink::test::mojom::VirtualAuthenticatorManager> bindings_;

  // The key is the unique_id of the corresponding value (the authenticator).
  std::map<std::string, std::unique_ptr<VirtualAuthenticator>> authenticators_;

  // Discoveries are owned by U2fRequest and FidoRequestHandler, and
  // automatically unregister themselves upon their destruction.
  std::set<VirtualFidoDiscovery*> discoveries_;

  scoped_refptr<device::VirtualFidoDevice::State> virtual_device_state_;

  DISALLOW_COPY_AND_ASSIGN(VirtualFidoDiscoveryFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_VIRTUAL_FIDO_DISCOVERY_FACTORY_H_
