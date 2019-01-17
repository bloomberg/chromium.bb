// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IME_DRIVER_IME_DRIVER_MUS_H_
#define CHROME_BROWSER_UI_VIEWS_IME_DRIVER_IME_DRIVER_MUS_H_

#include "base/macros.h"
#include "services/ws/public/mojom/ime/ime.mojom.h"

// Creates an InputMethodBridge when an IME session is started via mojo.
class IMEDriver : public ws::mojom::IMEDriver {
 public:
  IMEDriver();
  ~IMEDriver() override;

  // Instantiate the IME driver and register it to the UI service.
  static void Register();

 private:
  // ws::mojom::IMEDriver:
  void StartSession(ws::mojom::InputMethodRequest input_method_request,
                    ws::mojom::TextInputClientPtr client,
                    ws::mojom::SessionDetailsPtr details) override;

  DISALLOW_COPY_AND_ASSIGN(IMEDriver);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IME_DRIVER_IME_DRIVER_MUS_H_
