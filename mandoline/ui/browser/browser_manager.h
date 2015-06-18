// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_BROWSER_BROWSER_MANAGER_H_
#define MANDOLINE_UI_BROWSER_BROWSER_MANAGER_H_

#include <set>

#include "base/memory/scoped_vector.h"
#include "mandoline/ui/browser/public/interfaces/launch_handler.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/common/weak_binding_set.h"

#if defined(USE_AURA)
#include "mandoline/ui/aura/aura_init.h"
#endif

namespace mandoline {

class Browser;

// BrowserManager creates and manages the lifetime of Browsers.
class BrowserManager : public mojo::ApplicationDelegate,
                       public LaunchHandler,
                       public mojo::InterfaceFactory<LaunchHandler> {
 public:
  BrowserManager();
  ~BrowserManager() override;

  // BrowserManager owns the returned Browser.
  Browser* CreateBrowser();

  // Invoked by |browser| when it has closed.
  void BrowserClosed(Browser* browser);

 private:
  // Overridden from LaunchHandler:
  void LaunchURL(const mojo::String& url) override;

  // Overridden from mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // Overridden from mojo::InterfaceFactory<LaunchHandler>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<LaunchHandler> request) override;

  mojo::ApplicationImpl* app_;
#if defined(USE_AURA)
  scoped_ptr<AuraInit> aura_init_;
#endif
  mojo::WeakBindingSet<LaunchHandler> launch_handler_bindings_;
  std::set<Browser*> browsers_;

  DISALLOW_COPY_AND_ASSIGN(BrowserManager);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_BROWSER_BROWSER_MANAGER_H_
