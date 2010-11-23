// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CEEE_IE_PLUGIN_TOOLBAND_TOOLBAND_PROXY_H_
#define CEEE_IE_PLUGIN_TOOLBAND_TOOLBAND_PROXY_H_

#include <windows.h>
#include <vector>

// Registers all proxy/stubs in the current apartment.
// @param cookies if not NULL, then on return contains the registration
//      cookies for all successful proxy/stub class object registrations.
// @returns true on success, false on failure.
// @note this function logs errors for all failures.
// @note this function will also create registry entries for the sync/async
//      interface mapping for interfaces that have asynchronous variants.
bool RegisterProxyStubs(std::vector<DWORD>* cookies);

// Unregisters proxy/stub class objects from this apartment.
// @param cookies the cookies returned from an earlier call to
//      RegisterProxyStubs in this apartment.
void UnregisterProxyStubs(const std::vector<DWORD>& cookies);

// Registers or unregisters the sync to async IID mapping in registry
// for any asynchronous interfaces we implement.
// @param reg true to register, false to unregister.
bool RegisterAsyncProxies(bool reg);

#endif  // CEEE_IE_PLUGIN_TOOLBAND_TOOLBAND_PROXY_H_
