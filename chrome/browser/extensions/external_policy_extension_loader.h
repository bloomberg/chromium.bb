// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_POLICY_EXTENSION_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_POLICY_EXTENSION_LOADER_H_
#pragma once

#include "chrome/browser/extensions/external_extension_loader.h"

#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class ListValue;
class MockExternalPolicyExtensionProviderVisitor;
class Profile;

// A specialization of the ExternalExtensionProvider that uses
// prefs::kExtensionInstallForceList to look up which external extensions are
// registered.
class ExternalPolicyExtensionLoader
    : public ExternalExtensionLoader,
      public NotificationObserver {
 public:
  explicit ExternalPolicyExtensionLoader(Profile* profile);

  // NotificationObserver implementation
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  virtual void StartLoading();

 private:
  friend class base::RefCountedThreadSafe<ExternalExtensionLoader>;

  virtual ~ExternalPolicyExtensionLoader() {}

  PrefChangeRegistrar pref_change_registrar_;
  NotificationRegistrar notification_registrar_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExternalPolicyExtensionLoader);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_POLICY_EXTENSION_LOADER_H_
