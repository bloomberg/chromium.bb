// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_NEW_WINDOW_CLIENT_PROXY_H_
#define ASH_COMMON_NEW_WINDOW_CLIENT_PROXY_H_

#include "ash/public/interfaces/new_window.mojom.h"
#include "base/macros.h"

namespace service_manager {
class Connector;
}

namespace ash {

// A NewWindowClient which lazily connects to an exported mojom::NewWindowClient
// in "service:content_browser" on first use.
class NewWindowClientProxy : public mojom::NewWindowClient {
 public:
  explicit NewWindowClientProxy(service_manager::Connector* connector);
  ~NewWindowClientProxy() override;

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
  // Ensures that we have a valid |client_| before we try to use it.
  void EnsureInterface();

  // Clears |client_| when |client_| has a connection error.
  void OnClientConnectionError();

  service_manager::Connector* connector_;
  mojom::NewWindowClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(NewWindowClientProxy);
};

}  // namespace ash

#endif  // ASH_COMMON_NEW_WINDOW_CLIENT_PROXY_H_
