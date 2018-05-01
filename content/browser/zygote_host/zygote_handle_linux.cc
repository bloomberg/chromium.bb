// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/zygote_handle.h"

#include "content/browser/zygote_host/zygote_communication_linux.h"

namespace content {
namespace {

// Intentionally leaked.
ZygoteHandle g_generic_zygote = nullptr;

}  // namespace

ZygoteHandle CreateGenericZygote(
    base::OnceCallback<pid_t(base::CommandLine*, base::ScopedFD*)> launcher) {
  CHECK(!g_generic_zygote);
  g_generic_zygote = new ZygoteCommunication();
  g_generic_zygote->Init(std::move(launcher));
  return g_generic_zygote;
}

ZygoteHandle GetGenericZygote() {
  CHECK(g_generic_zygote);
  return g_generic_zygote;
}

}  // namespace content
