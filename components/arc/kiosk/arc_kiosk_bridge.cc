// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/kiosk/arc_kiosk_bridge.h"

#include "components/arc/arc_bridge_service.h"

namespace arc {

ArcKioskBridge::ArcKioskBridge(ArcBridgeService* bridge_service,
                               Delegate* delegate)
    : ArcService(bridge_service), binding_(this), delegate_(delegate) {
  DCHECK(delegate_);
  arc_bridge_service()->kiosk()->AddObserver(this);
}

ArcKioskBridge::~ArcKioskBridge() {
  arc_bridge_service()->kiosk()->RemoveObserver(this);
}

void ArcKioskBridge::OnInstanceReady() {
  mojom::KioskInstance* kiosk_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service()->kiosk(), Init);
  DCHECK(kiosk_instance);
  kiosk_instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcKioskBridge::OnMaintenanceSessionCreated(int32_t session_id) {
  session_id_ = session_id;
  delegate_->OnMaintenanceSessionCreated();
  // TODO(poromov@) Show appropriate splash screen.
}

void ArcKioskBridge::OnMaintenanceSessionFinished(int32_t session_id,
                                                  bool success) {
  // Filter only callbacks for the started kiosk session.
  if (session_id != session_id_)
    return;
  session_id_ = -1;
  delegate_->OnMaintenanceSessionFinished();
}

}  // namespace arc
