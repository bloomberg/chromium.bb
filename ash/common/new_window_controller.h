// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_NEW_WINDOW_CONTROLLER_H_
#define ASH_COMMON_NEW_WINDOW_CONTROLLER_H_

#include "ash/public/interfaces/new_window.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace ash {

// Provides the NewWindowController interface to the outside world. This lets a
// consumer of ash provide a NewWindowClient, which we will dispatch to if one
// has been provided to us.
class NewWindowController : public mojom::NewWindowController,
                            public mojom::NewWindowClient {
 public:
  NewWindowController();
  ~NewWindowController() override;

  void BindRequest(mojom::NewWindowControllerRequest request);

  // NewWindowClient:
  void NewTab() override;
  void NewWindow(bool incognito) override;
  void OpenFileManager() override;
  void OpenCrosh() override;
  void OpenGetHelp() override;
  void RestoreTab() override;
  void ShowKeyboardOverlay() override;
  void ShowTaskManager() override;
  void OpenFeedbackPage() override;

 private:
  // NewWindowController:
  void SetClient(mojom::NewWindowClientAssociatedPtrInfo client) override;

  mojo::BindingSet<mojom::NewWindowController> bindings_;

  mojom::NewWindowClientAssociatedPtr client_;

  DISALLOW_COPY_AND_ASSIGN(NewWindowController);
};

}  // namespace ash

#endif  // ASH_COMMON_NEW_WINDOW_CONTROLLER_H_
