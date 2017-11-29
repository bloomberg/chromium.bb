// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_FORWARDING_COMPOSITING_MODE_REPORTER_IMPL_H_
#define COMPONENTS_VIZ_HOST_FORWARDING_COMPOSITING_MODE_REPORTER_IMPL_H_

#include "components/viz/host/viz_host_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/viz/public/interfaces/compositing/compositing_mode_watcher.mojom.h"

namespace viz {

// This class is both a CompositingModeReporter and CompositingModeWatcher whose
// role is to forward the compositing mode state from a more authoritative
// reporter on to its own observers.
// This class should be added as a CompositingModeWatcher to that more
// authoritative reporter, and then acts as a CompositingModeReporter by
// forwarding state along.
class VIZ_HOST_EXPORT ForwardingCompositingModeReporterImpl
    : public mojom::CompositingModeReporter,
      public mojom::CompositingModeWatcher {
 public:
  ForwardingCompositingModeReporterImpl();
  ~ForwardingCompositingModeReporterImpl() override;

  // Bind this object into a mojo pointer and returns it. The caller should
  // register the mojom pointer with another CompositingModeReporter. Then
  // that reporter's state will be forwarded to watchers of this reporter.
  mojom::CompositingModeWatcherPtr BindAsWatcher();

  // Called for each consumer of the CompositingModeReporter interface, to
  // fulfill a mojo pointer for them.
  void BindRequest(mojom::CompositingModeReporterRequest request);

  // mojom::CompositingModeReporter implementation.
  void AddCompositingModeWatcher(
      mojom::CompositingModeWatcherPtr watcher) override;

  // mojom::CompositingModeWatcher implementation.
  void CompositingModeFallbackToSoftware() override;

 private:
  // A mirror of the state from the authoritative CompositingModeReporter that
  // this watches.
  bool gpu_ = true;
  // A binding to |this| that is given to the authoritative reporter to listen
  // for the compositing mode state.
  mojo::Binding<mojom::CompositingModeWatcher> forwarding_source_binding_;
  mojo::BindingSet<mojom::CompositingModeReporter> bindings_;
  mojo::InterfacePtrSet<mojom::CompositingModeWatcher> watchers_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_FORWARDING_COMPOSITING_MODE_REPORTER_IMPL_H_
