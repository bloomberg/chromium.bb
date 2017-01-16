// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IME_DRIVER_IME_DRIVER_MUS_H_
#define CHROME_BROWSER_UI_VIEWS_IME_DRIVER_IME_DRIVER_MUS_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/ime/ime.mojom.h"

class IMEDriver : public ui::mojom::IMEDriver {
 public:
  IMEDriver();
  ~IMEDriver() override;

  // Instantiate the IME driver and register it to the UI service.
  static void Register();

 private:
  // ui::mojom::IMEDriver:
  void StartSession(int32_t session_id,
                    ui::mojom::StartSessionDetailsPtr details) override;
  void CancelSession(int32_t session_id) override;

  std::map<int32_t, std::unique_ptr<mojo::Binding<ui::mojom::InputMethod>>>
      input_method_bindings_;

  DISALLOW_COPY_AND_ASSIGN(IMEDriver);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IME_DRIVER_IME_DRIVER_MUS_H_
