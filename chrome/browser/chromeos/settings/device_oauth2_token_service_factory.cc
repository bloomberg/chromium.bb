// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"

#include "base/message_loop/message_loop.h"
#include "base/tracked_objects.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/token_encryptor.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace {

DeviceOAuth2TokenServiceFactory* g_factory = NULL;

}  // namespace

DeviceOAuth2TokenServiceFactory::DeviceOAuth2TokenServiceFactory()
    : token_service_(new DeviceOAuth2TokenService(
        g_browser_process->system_request_context(),
        g_browser_process->local_state(),
        new CryptohomeTokenEncryptor)) {
}

DeviceOAuth2TokenServiceFactory::~DeviceOAuth2TokenServiceFactory() {
  delete token_service_;
}

// static
void DeviceOAuth2TokenServiceFactory::Get(const GetCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  DeviceOAuth2TokenService* token_service = NULL;
  if (g_factory)
    token_service = g_factory->token_service_;

  // TODO(satorux): Implement async initialization logic for
  // DeviceOAuth2TokenService. crbug.com/309959.
  // Here's how that should work:
  //
  // if token_service is ready:
  //   run callback asynchronously via MessageLoop
  //   return
  //
  // add callback to the pending callback list
  //
  // if there is only one pending callback:
  //   start getting the system salt asynchronously...
  //
  // upon receiving the system salt:
  //   create CryptohomeTokenEncryptor with that key
  //   create DeviceOAuth2TokenService
  //   run all the pending callbacks
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, token_service));
}

// static
void DeviceOAuth2TokenServiceFactory::Initialize() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  DCHECK(!g_factory);
  g_factory = new DeviceOAuth2TokenServiceFactory;
}

// static
void DeviceOAuth2TokenServiceFactory::Shutdown() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (g_factory) {
    delete g_factory;
    g_factory = NULL;
  }
}

}  // namespace chromeos
