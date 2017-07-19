// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_GRC_TAB_SIGNAL_OBSERVER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_GRC_TAB_SIGNAL_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/resource_coordinator/public/interfaces/tab_signal.mojom.h"

namespace content {
class WebContents;
}

namespace resource_coordinator {

// Implementation of resource_coordinator::mojom::TabSignalObserver,
// observer constructs a mojo channel to TabSignalGenerator in GRC, passes an
// interface pointer to TabSignalGenerator, and receives tab scoped signals from
// TabSignalGenerator once a signal is generated.
class TabManager::GRCTabSignalObserver : public mojom::TabSignalObserver {
 public:
  GRCTabSignalObserver();
  ~GRCTabSignalObserver() override;

  static bool IsEnabled();

  // mojom::TabSignalObserver implementation.
  void OnEventReceived(const CoordinationUnitID& cu_id,
                       mojom::TabEvent event) override;

  void AssociateCoordinationUnitIDWithWebContents(
      const CoordinationUnitID& cu_id,
      content::WebContents* web_contents);

  void RemoveCoordinationUnitID(const CoordinationUnitID& cu_id);

 private:
  mojo::Binding<mojom::TabSignalObserver> binding_;
  std::map<CoordinationUnitID, content::WebContents*> cu_id_web_contents_map_;
  DISALLOW_COPY_AND_ASSIGN(GRCTabSignalObserver);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_GRC_TAB_SIGNAL_OBSERVER_H_
