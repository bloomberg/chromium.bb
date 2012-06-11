// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PEPPER_FLASH_SETTINGS_MANAGER_H_
#define CHROME_BROWSER_PEPPER_FLASH_SETTINGS_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

class PluginPrefs;
class PrefService;

namespace content {
class BrowserContext;
}

namespace webkit {
struct WebPluginInfo;
}

// PepperFlashSettingsManager communicates with a PPAPI broker process to
// read/write Pepper Flash settings.
class PepperFlashSettingsManager {
 public:
  class Client {
   public:
    virtual ~Client() {}

    virtual void OnDeauthorizeContentLicensesCompleted(uint32 request_id,
                                                       bool success) = 0;
  };

  // |client| must outlive this object. It is guaranteed that |client| won't
  // receive any notifications after this object goes away.
  PepperFlashSettingsManager(Client* client,
                             content::BrowserContext* browser_context);
  ~PepperFlashSettingsManager();

  // |plugin_info| will be updated if it is not NULL and the method returns
  // true.
  static bool IsPepperFlashInUse(PluginPrefs* plugin_prefs,
                                 webkit::WebPluginInfo* plugin_info);

  static void RegisterUserPrefs(PrefService* prefs);

  // Requests to deauthorize content licenses.
  // Client::OnDeauthorizeContentLicensesCompleted() will be called when the
  // operation is completed.
  // The return value is the same as the request ID passed into
  // Client::OnDeauthorizeContentLicensesCompleted().
  uint32 DeauthorizeContentLicenses();

 private:
  // Core does most of the work. It is ref-counted so that its lifespan can be
  // independent of the containing object's:
  // - The manager can be deleted on the UI thread while the core still being
  // used on the I/O thread.
  // - The manager can delete the core when it encounters errors and create
  // another one to handle new requests.
  class Core;

  uint32 GetNextRequestId();

  void EnsureCoreExists();

  // Notified by |core_| when an error occurs.
  void OnError();

  // |client_| is not owned by this object and must outlive it.
  Client* client_;

  // The browser context for the profile.
  content::BrowserContext* browser_context_;

  scoped_refptr<Core> core_;

  uint32 next_request_id_;

  DISALLOW_COPY_AND_ASSIGN(PepperFlashSettingsManager);
};

#endif  // CHROME_BROWSER_PEPPER_FLASH_SETTINGS_MANAGER_H_
