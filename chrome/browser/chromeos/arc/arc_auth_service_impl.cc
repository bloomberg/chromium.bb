// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_auth_service_impl.h"

#include <utility>

namespace arc {

ArcAuthServiceImpl::ArcAuthServiceImpl() : binding_(this) {}

ArcAuthServiceImpl::~ArcAuthServiceImpl() {
  ArcBridgeService::Get()->RemoveObserver(this);
}

void ArcAuthServiceImpl::StartObservingBridgeServiceChanges() {
  ArcBridgeService::Get()->AddObserver(this);
}

void ArcAuthServiceImpl::OnAuthInstanceReady() {
  arc::AuthHostPtr host;
  binding_.Bind(GetProxy(&host));
  ArcBridgeService::Get()->auth_instance()->Init(std::move(host));
}

void ArcAuthServiceImpl::GetAuthCode(const GetAuthCodeCallback& callback) {
  // TODO(victorhsieh): request auth code from LSO (crbug.com/571146).
  callback.Run(mojo::String("fake auth code from ArcAuthService in Chrome"));
}

}  // namespace arc
