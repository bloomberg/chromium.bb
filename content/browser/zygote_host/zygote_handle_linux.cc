// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/zygote_handle_linux.h"

#include "content/browser/zygote_host/zygote_communication_linux.h"

namespace content {
namespace {

// Intentionally leaked.
ZygoteHandle g_generic_zygote = nullptr;

}  // namespace

ZygoteHandle CreateGenericZygote() {
  CHECK(!g_generic_zygote);
  g_generic_zygote = new ZygoteCommunication();
  g_generic_zygote->Init();
  return g_generic_zygote;
}

ZygoteHandle GetGenericZygote() {
  CHECK(g_generic_zygote);
  return g_generic_zygote;
}

}  // namespace content
