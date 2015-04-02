// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_KIOSK_WM_KIOSK_WM_H_
#define SERVICES_KIOSK_WM_KIOSK_WM_H_

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/services/kiosk_wm/navigator_host_impl.h"
#include "mojo/services/window_manager/window_manager_app.h"
#include "mojo/services/window_manager/window_manager_delegate.h"
#include "third_party/mojo_services/src/input_events/public/interfaces/input_events.mojom.h"
#include "third_party/mojo_services/src/navigation/public/interfaces/navigation.mojom.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_manager.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_manager_delegate.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_observer.h"
#include "ui/base/accelerators/accelerator.h"

namespace kiosk_wm {

class MergedServiceProvider;

class KioskWM : public mojo::ApplicationDelegate,
                public mojo::ViewManagerDelegate,
                public mojo::ViewObserver,
                public window_manager::WindowManagerDelegate,
                public mojo::InterfaceFactory<mojo::NavigatorHost>,
                public ui::AcceleratorTarget {
 public:
  KioskWM();
  ~KioskWM() override;

  base::WeakPtr<KioskWM> GetWeakPtr();

  void ReplaceContentWithURL(const mojo::String& url);

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

  // Overriden from mojo::ViewObserver:
  void OnViewDestroyed(mojo::View* view) override;
  void OnViewBoundsChanged(mojo::View* view,
                           const mojo::Rect& old_bounds,
                           const mojo::Rect& new_bounds) override;

  // Overridden from WindowManagerDelegate:
  void Embed(const mojo::String& url,
             mojo::InterfaceRequest<mojo::ServiceProvider> services,
             mojo::ServiceProviderPtr exposed_services) override;

  // Overridden from mojo::InterfaceFactory<mojo::NavigatorHost>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::NavigatorHost> request) override;

  // Overriden from ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  scoped_ptr<window_manager::WindowManagerApp> window_manager_app_;

  // Only support being embedded once, so both application-level
  // and embedding-level state are shared on the same object.
  mojo::View* root_;
  mojo::View* content_;
  std::string default_url_;
  std::string pending_url_;

  mojo::ServiceProviderImpl exposed_services_impl_;
  scoped_ptr<MergedServiceProvider> merged_service_provider_;

  NavigatorHostImpl navigator_host_;

  base::WeakPtrFactory<KioskWM> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(KioskWM);
};

}  // namespace kiosk_wm

#endif  // SERVICES_KIOSK_WM_KIOSK_WM_H_
