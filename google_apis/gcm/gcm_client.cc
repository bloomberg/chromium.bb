// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/gcm_client.h"

#include "base/lazy_instance.h"
#include "google_apis/gcm/gcm_client_impl.h"

namespace gcm {

namespace {

static base::LazyInstance<GCMClientImpl>::Leaky g_gcm_client =
    LAZY_INSTANCE_INITIALIZER;
static GCMClient::TestingFactoryFunction g_gcm_client_factory = NULL;
static GCMClient* g_gcm_client_override = NULL;

}  // namespace

GCMClient::OutgoingMessage::OutgoingMessage()
    : time_to_live(0) {
}

GCMClient::OutgoingMessage::~OutgoingMessage() {
}

GCMClient::IncomingMessage::IncomingMessage() {
}

GCMClient::IncomingMessage::~IncomingMessage() {
}

// static
GCMClient* GCMClient::Get() {
  if (g_gcm_client_override)
    return g_gcm_client_override;
  if (g_gcm_client_factory) {
    g_gcm_client_override = g_gcm_client_factory();
    return g_gcm_client_override;
  }
  return g_gcm_client.Pointer();
}

// static
void GCMClient::SetTestingFactory(TestingFactoryFunction factory) {
  if (g_gcm_client_override) {
    delete g_gcm_client_override;
    g_gcm_client_override = NULL;
  }
  g_gcm_client_factory = factory;
}

}  // namespace gcm
