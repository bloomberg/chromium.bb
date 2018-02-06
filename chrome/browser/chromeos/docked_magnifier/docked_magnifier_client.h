// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOCKED_MAGNIFIER_DOCKED_MAGNIFIER_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DOCKED_MAGNIFIER_DOCKED_MAGNIFIER_CLIENT_H_

#include <memory>

#include "ash/public/interfaces/docked_magnifier_controller.mojom.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "mojo/public/cpp/bindings/binding.h"

// Observes focus change events for nodes in webpages and notifies the Docked
// Magnifier with these events.
class DockedMagnifierClient : public ash::mojom::DockedMagnifierClient,
                              public content::NotificationObserver {
 public:
  DockedMagnifierClient();
  ~DockedMagnifierClient() override;

  void Start();

  // ash::mojom::DockedMagnifierClient:
  void OnEnabledStatusChanged(bool enabled) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  ash::mojom::DockedMagnifierControllerPtr docked_magnifier_controller_;
  mojo::Binding<ash::mojom::DockedMagnifierClient> binding_;

  content::NotificationRegistrar registrar_;

  bool observing_focus_changes_ = false;

  DISALLOW_COPY_AND_ASSIGN(DockedMagnifierClient);
};

#endif  // CHROME_BROWSER_CHROMEOS_DOCKED_MAGNIFIER_DOCKED_MAGNIFIER_CLIENT_H_
