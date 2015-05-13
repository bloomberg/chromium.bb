// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_BROWSER_BROWSER_H_
#define MANDOLINE_UI_BROWSER_BROWSER_H_

#include "base/memory/weak_ptr.h"
#include "components/view_manager/public/cpp/view_manager.h"
#include "components/view_manager/public/cpp/view_manager_delegate.h"
#include "components/window_manager/window_manager_app.h"
#include "components/window_manager/window_manager_delegate.h"
#include "mandoline/services/navigation/public/interfaces/navigation.mojom.h"
#include "mandoline/ui/browser/navigator_host_impl.h"
#include "mandoline/ui/browser/omnibox.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_delegate.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/application/connect.h"
#include "third_party/mojo/src/mojo/public/cpp/application/service_provider_impl.h"
#include "ui/mojo/events/input_events.mojom.h"
#include "url/gurl.h"

namespace mandoline {

class BrowserUI;
class MergedServiceProvider;

class Browser : public mojo::ApplicationDelegate,
                public mojo::ViewManagerDelegate,
                public window_manager::WindowManagerDelegate,
                public OmniboxClient,
                public mojo::InterfaceFactory<mojo::NavigatorHost> {
 public:
  Browser();
  ~Browser() override;

  base::WeakPtr<Browser> GetWeakPtr();

  void ReplaceContentWithURL(const mojo::String& url);

  mojo::View* content() { return content_; }
  mojo::View* omnibox() { return omnibox_; }

  const GURL& current_url() const { return current_url_; }

 private:
  // Overridden from mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;
  bool ConfigureOutgoingConnection(
      mojo::ApplicationConnection* connection) override;

  // Overridden from mojo::ViewManagerDelegate:
  void OnEmbed(mojo::View* root,
               mojo::InterfaceRequest<mojo::ServiceProvider> services,
               mojo::ServiceProviderPtr exposed_services) override;
  void OnViewManagerDisconnected(mojo::ViewManager* view_manager) override;

  // Overridden from WindowManagerDelegate:
  void Embed(const mojo::String& url,
             mojo::InterfaceRequest<mojo::ServiceProvider> services,
             mojo::ServiceProviderPtr exposed_services) override;
  void OnAcceleratorPressed(mojo::View* view,
                            mojo::KeyboardCode keyboard_code,
                            mojo::EventFlags flags) override;

  // Overridden from OmniboxClient:
  void OpenURL(const mojo::String& url) override;

  // Overridden from mojo::InterfaceFactory<mojo::NavigatorHost>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::NavigatorHost> request) override;

  void ShowOmnibox(const mojo::String& url,
                   mojo::InterfaceRequest<mojo::ServiceProvider> services,
                   mojo::ServiceProviderPtr exposed_services);

  scoped_ptr<window_manager::WindowManagerApp> window_manager_app_;

  // Only support being embedded once, so both application-level
  // and embedding-level state are shared on the same object.
  mojo::View* root_;
  mojo::View* content_;
  mojo::View* omnibox_;
  std::string default_url_;
  std::string pending_url_;

  mojo::ServiceProviderImpl exposed_services_impl_;
  scoped_ptr<MergedServiceProvider> merged_service_provider_;

  NavigatorHostImpl navigator_host_;

  GURL current_url_;

  scoped_ptr<BrowserUI> ui_;

  base::WeakPtrFactory<Browser> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Browser);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_BROWSER_BROWSER_H_
