// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_NEW_WINDOW_CONTROLLER_H_
#define ASH_NEW_WINDOW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/interfaces/new_window.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace ash {

// Provides the NewWindowController interface to the outside world. This lets a
// consumer of ash provide a NewWindowClient, which we will dispatch to if one
// has been provided to us.
class ASH_EXPORT NewWindowController : public mojom::NewWindowController {
 public:
  NewWindowController();
  ~NewWindowController() override;

  void BindRequest(mojom::NewWindowControllerRequest request);

  // NewWindowController:
  void SetClient(mojom::NewWindowClientAssociatedPtrInfo client) override;
  void ShowKeyboardOverlay() override;

  // Pass throughs for methods of the same name on |client_|.
  void NewTab();
  void NewWindow(bool incognito);
  void OpenFileManager();
  void OpenCrosh();
  void OpenGetHelp();
  void RestoreTab();
  void ShowTaskManager();
  void OpenFeedbackPage();

 private:
  mojo::BindingSet<mojom::NewWindowController> bindings_;

  mojom::NewWindowClientAssociatedPtr client_;

  DISALLOW_COPY_AND_ASSIGN(NewWindowController);
};

}  // namespace ash

#endif  // ASH_NEW_WINDOW_CONTROLLER_H_
