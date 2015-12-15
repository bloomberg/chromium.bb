// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/keyed_service_provider.h"

#include "base/logging.h"

namespace ios {
namespace {
KeyedServiceProvider* g_keyed_service_provider = nullptr;
}  // namespace

void SetKeyedServiceProvider(KeyedServiceProvider* provider) {
  // Since the dependency between KeyedService is only resolved at instantiation
  // time, forbid un-installation or overridden the global KeyedServiceProvider.
  DCHECK(provider && !g_keyed_service_provider);
  g_keyed_service_provider = provider;
}

KeyedServiceProvider* GetKeyedServiceProvider() {
  return g_keyed_service_provider;
}

KeyedServiceProvider::KeyedServiceProvider() {
}

KeyedServiceProvider::~KeyedServiceProvider() {
}

void KeyedServiceProvider::AssertKeyedFactoriesBuilt() {
  GetSyncServiceFactory();
}

}  // namespace ios
