// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/api/messaging/native_message_host.h"

// Supports communication with Arc support dialog.
class ArcSupportHost : public extensions::NativeMessageHost,
                       public arc::ArcAuthService::Observer {
 public:
  static const char kHostName[];
  static const char kHostAppId[];
  static const char kStorageId[];
  static const char* const kHostOrigin[];

  static std::unique_ptr<NativeMessageHost> Create();

  ~ArcSupportHost() override;

  // Overrides NativeMessageHost:
  void Start(Client* client) override;
  void OnMessage(const std::string& request_string) override;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override;

  // Overrides arc::ArcAuthService::Observer:
  void OnOptInUIClose() override;
  void OnOptInUIShowPage(arc::ArcAuthService::UIPage page,
                         const base::string16& status) override;

 private:
  ArcSupportHost();

  void OnMetricsPreferenceChanged();
  void Initialize();
  void SendMetricsMode();
  void EnableMetrics(bool is_enabled);
  void EnableBackupRestore(bool is_enabled);

  // Unowned pointer.
  Client* client_ = nullptr;

  // Used to track metrics preference.
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ArcSupportHost);
};

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_
