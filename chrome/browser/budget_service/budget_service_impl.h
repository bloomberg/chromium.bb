// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BUDGET_SERVICE_BUDGET_SERVICE_IMPL_H_
#define CHROME_BROWSER_BUDGET_SERVICE_BUDGET_SERVICE_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/WebKit/public/platform/modules/budget_service/budget_service.mojom.h"

// Implementation of the BudgetService Mojo service provided by the browser
// layer. It is responsible for dispatching budget requests to the
// BudgetManager.
class BudgetServiceImpl : public blink::mojom::BudgetService {
 public:
  static void Create(int render_process_id,
                     blink::mojom::BudgetServiceRequest request);

  // blink::mojom::BudgetService implementation.
  void GetCost(blink::mojom::BudgetOperationType operation,
               const GetCostCallback& callback) override;
  void GetBudget(const url::Origin& origin,
                 const GetBudgetCallback& callbac) override;

 private:
  BudgetServiceImpl(int render_process_id,
                    blink::mojom::BudgetServiceRequest request);
  ~BudgetServiceImpl() override;

  // Render process ID is used to get the browser context.
  int render_process_id_;

  // The lifetime of the mojo service is tied to the lifetime of the connection.
  mojo::StrongBinding<blink::mojom::BudgetService> binding_;

  DISALLOW_COPY_AND_ASSIGN(BudgetServiceImpl);
};

#endif  // CHROME_BROWSER_BUDGET_SERVICE_BUDGET_SERVICE_IMPL_H_
