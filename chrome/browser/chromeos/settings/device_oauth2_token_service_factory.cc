// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"

#include <memory>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_delegate.h"
#include "chrome/browser/chromeos/settings/token_encryptor.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

namespace {

static DeviceOAuth2TokenService* g_device_oauth2_token_service_ = NULL;

}  // namespace

// static
DeviceOAuth2TokenService* DeviceOAuth2TokenServiceFactory::Get() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_device_oauth2_token_service_;
}

void DeviceOAuth2TokenServiceFactory::Initialize() {
  Initialize(g_browser_process->system_network_context_manager()
                 ->GetSharedURLLoaderFactory());
}

// static
void DeviceOAuth2TokenServiceFactory::Initialize(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!g_device_oauth2_token_service_);
  g_device_oauth2_token_service_ = new DeviceOAuth2TokenService(
      std::make_unique<DeviceOAuth2TokenServiceDelegate>(
          url_loader_factory, g_browser_process->local_state()));
}

// static
void DeviceOAuth2TokenServiceFactory::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_device_oauth2_token_service_) {
    delete g_device_oauth2_token_service_;
    g_device_oauth2_token_service_ = NULL;
  }
}

}  // namespace chromeos
