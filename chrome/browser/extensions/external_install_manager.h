// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_MANAGER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry_observer.h"

class GlobalErrorService;

namespace content {
class BrowserContext;
class NotificationDetails;
class NotificationSource;
}

namespace extensions {
class Extension;
class ExtensionRegistry;
class ExternalInstallError;

class ExternalInstallManager : public ExtensionRegistryObserver,
                               public content::NotificationObserver {
 public:
  explicit ExternalInstallManager(content::BrowserContext* browser_context);
  virtual ~ExternalInstallManager();

  // Adds a global error informing the user that an external extension was
  // installed. If |is_new_profile| is true, then this error is from the first
  // time our profile checked for new extensions.
  void AddExternalInstallError(const Extension* extension, bool is_new_profile);

  // Removes the global error, if one existed.
  void RemoveExternalInstallError();

  // Returns true if there is a global error for an external install.
  bool HasExternalInstallError() const;

  // Returns true if there is a global error with a bubble view for an external
  // install. Used for testing.
  bool HasExternalInstallBubble() const;

  // Returns the current install error, if one exists.
  const ExternalInstallError* error() { return error_.get(); }

  // Returns a mutable copy of the error for testing purposes.
  ExternalInstallError* error_for_testing() { return error_.get(); }

 private:
  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const Extension* extension) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // The associated BrowserContext.
  content::BrowserContext* browser_context_;

  // The current ExternalInstallError, if one exists.
  scoped_ptr<ExternalInstallError> error_;

  content::NotificationRegistrar registrar_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExternalInstallManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_MANAGER_H_
