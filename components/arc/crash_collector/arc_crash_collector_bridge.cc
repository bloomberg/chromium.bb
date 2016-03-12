// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/crash_collector/arc_crash_collector_bridge.h"

#include <utility>

#include "base/files/file.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "mojo/edk/embedder/embedder.h"

namespace arc {

ArcCrashCollectorBridge::ArcCrashCollectorBridge(ArcBridgeService* bridge)
    : ArcService(bridge), binding_(this) {
  arc_bridge_service()->AddObserver(this);
}

ArcCrashCollectorBridge::~ArcCrashCollectorBridge() {
  arc_bridge_service()->RemoveObserver(this);
}

void ArcCrashCollectorBridge::OnCrashCollectorInstanceReady() {
  CrashCollectorHostPtr host_ptr;
  binding_.Bind(mojo::GetProxy(&host_ptr));
  arc_bridge_service()->crash_collector_instance()->Init(std::move(host_ptr));
}

void ArcCrashCollectorBridge::DumpCrash(const mojo::String& type,
                                        mojo::ScopedHandle pipe) {
  mojo::edk::ScopedPlatformHandle handle;
  mojo::edk::PassWrappedPlatformHandle(pipe.get().value(), &handle);

  // TODO(domlaskowski): Spawn crash_reporter with --arc flag.
  base::File file(handle.release().handle);

  static const int kSize = 1 << 10;
  char buffer[kSize];
  int total = 0;
  int count;

  while ((count = file.ReadAtCurrentPosNoBestEffort(buffer, kSize)) > 0)
    total += count;

  LOG(WARNING) << "Ignoring " << type << " dump ("
               << base::FormatBytesUnlocalized(total) << ").";
}

}  // namespace arc
