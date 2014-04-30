// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PRINCIPALS_PRIVATE_PRINCIPALS_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PRINCIPALS_PRIVATE_PRINCIPALS_PRIVATE_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"

// WARNING: chrome.principalsPrivate is a set of experimental APIs for the new
// profile management flows. Every new API must extend
// PrincipalsPrivateExtensionFunction which is guarded with a flag check
// for "new-profile-management".

namespace extensions {

class PrincipalsPrivateExtensionFunction : public ChromeSyncExtensionFunction {
 public:
  PrincipalsPrivateExtensionFunction() {}

 protected:
  virtual ~PrincipalsPrivateExtensionFunction() {}

  // ExtensionFunction:
  // Checks for the flag "new-profile-management", if set calls
  // RunSyncSafe which must be overriden by subclasses.
  virtual bool RunSync() OVERRIDE FINAL;

 private:
  virtual bool RunSyncSafe() = 0;
};

class PrincipalsPrivateSignOutFunction
    : public PrincipalsPrivateExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("principalsPrivate.signOut",
                             PRINCIPALSPRIVATE_SIGNOUT);
  PrincipalsPrivateSignOutFunction() {}

 protected:
  virtual ~PrincipalsPrivateSignOutFunction() {}

 private:
  // PrincipalsPrivateExtensionFunction
  virtual bool RunSyncSafe() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(PrincipalsPrivateSignOutFunction);
};

class PrincipalsPrivateShowAvatarBubbleFunction
    : public PrincipalsPrivateExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("principalsPrivate.showAvatarBubble",
                             PRINCIPALSPRIVATE_SHOWAVATARBUBBLE);
  PrincipalsPrivateShowAvatarBubbleFunction() {}

 protected:
  virtual ~PrincipalsPrivateShowAvatarBubbleFunction() {}

 private:
  // PrincipalsPrivateExtensionFunction
  virtual bool RunSyncSafe() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(PrincipalsPrivateShowAvatarBubbleFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PRINCIPALS_PRIVATE_PRINCIPALS_PRIVATE_API_H_

