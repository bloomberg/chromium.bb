// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_OEMCRYPTO_ARC_OEMCRYPTO_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_OEMCRYPTO_ARC_OEMCRYPTO_BRIDGE_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "components/arc/common/oemcrypto.mojom.h"
#include "components/arc/common/oemcrypto_daemon.mojom.h"
#include "components/arc/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcOemCryptoBridge : public KeyedService,
                           public ConnectionObserver<mojom::OemCryptoInstance>,
                           public mojom::OemCryptoHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcOemCryptoBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcOemCryptoBridge(content::BrowserContext* context,
                     ArcBridgeService* bridge_service);
  ~ArcOemCryptoBridge() override;

  // Overridden from ConnectionObserver<mojom::OemCryptoInstance>:
  void OnConnectionReady() override;

  // OemCrypto Mojo host interface
  void Connect(mojom::OemCryptoServiceRequest request) override;

 private:
  void OnBootstrapMojoConnection(mojom::OemCryptoServiceRequest request,
                                 bool result);
  void ConnectToDaemon(mojom::OemCryptoServiceRequest request);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
  mojo::Binding<mojom::OemCryptoHost> binding_;
  arc_oemcrypto::mojom::OemCryptoHostDaemonPtr oemcrypto_host_daemon_ptr_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcOemCryptoBridge> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcOemCryptoBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_OEMCRYPTO_ARC_OEMCRYPTO_BRIDGE_H_
