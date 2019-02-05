// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALL_FINALIZER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "extensions/common/extension_id.h"

class GURL;
class Profile;

struct WebApplicationInfo;

namespace extensions {

class CrxInstaller;
class CrxInstallError;

// Class used to actually install the Bookmark App in the system.
class BookmarkAppInstallFinalizer {
 public:
  using ResultCallback = base::OnceCallback<void(const ExtensionId&)>;

  // Constructs a BookmarkAppInstallFinalizer that will install the Bookmark App
  // in |profile|.
  explicit BookmarkAppInstallFinalizer(Profile* profile);
  virtual ~BookmarkAppInstallFinalizer();

  // TODO(crbug.com/864904): This should take more options e.g. what container
  // to launch the app in, should the app sync, etc.
  virtual void Install(const WebApplicationInfo& web_app_info,
                       ResultCallback callback);

  void SetCrxInstallerForTesting(scoped_refptr<CrxInstaller> crx_installer);

 private:
  void OnInstall(ResultCallback callback,
                 const GURL& app_url,
                 const base::Optional<CrxInstallError>& error);

  scoped_refptr<CrxInstaller> crx_installer_;

  // We need a WeakPtr because CrxInstaller is refcounted and it can run its
  // callback after this class has been destroyed.
  base::WeakPtrFactory<BookmarkAppInstallFinalizer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppInstallFinalizer);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALL_FINALIZER_H_
