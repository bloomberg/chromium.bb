// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/tracked_objects.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/token_encryptor.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace {

DeviceOAuth2TokenServiceFactory* g_factory = NULL;

}  // namespace

DeviceOAuth2TokenServiceFactory::DeviceOAuth2TokenServiceFactory()
    : initialized_(false),
      token_service_(NULL),
      weak_ptr_factory_(this) {
}

DeviceOAuth2TokenServiceFactory::~DeviceOAuth2TokenServiceFactory() {
  delete token_service_;
}

// static
void DeviceOAuth2TokenServiceFactory::Get(const GetCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (!g_factory) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   static_cast<DeviceOAuth2TokenService*>(NULL)));
    return;
  }

  g_factory->RunAsync(callback);
}

// static
void DeviceOAuth2TokenServiceFactory::Initialize() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  DCHECK(!g_factory);
  g_factory = new DeviceOAuth2TokenServiceFactory;
  g_factory->CreateTokenService();
}

// static
void DeviceOAuth2TokenServiceFactory::Shutdown() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (g_factory) {
    delete g_factory;
    g_factory = NULL;
  }
}

void DeviceOAuth2TokenServiceFactory::CreateTokenService() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&DeviceOAuth2TokenServiceFactory::DidGetSystemSalt,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeviceOAuth2TokenServiceFactory::DidGetSystemSalt(
    const std::string& system_salt) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!token_service_);

  if (system_salt.empty()) {
    LOG(ERROR) << "Failed to get the system salt";
  } else {
    token_service_= new DeviceOAuth2TokenService(
        g_browser_process->system_request_context(),
        g_browser_process->local_state(),
        new CryptohomeTokenEncryptor(system_salt));
  }
  // Mark that the factory is initialized.
  initialized_ = true;

  // Run callbacks regardless of whether token_service_ is created or not,
  // but don't run callbacks immediately. Each callback would cause an
  // interesting action, hence running them consecutively could be
  // potentially expensive and dangerous.
  while (!pending_callbacks_.empty()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(pending_callbacks_.front(), token_service_));
    pending_callbacks_.pop();
  }
}

void DeviceOAuth2TokenServiceFactory::RunAsync(const GetCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (initialized_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, token_service_));
    return;
  }

  pending_callbacks_.push(callback);
}

}  // namespace chromeos
