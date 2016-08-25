// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/budget_service/budget_service_impl.h"

#include "chrome/browser/budget_service/budget_manager.h"
#include "chrome/browser/budget_service/budget_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_process_host.h"

// static
void BudgetServiceImpl::Create(int render_process_id,
                               blink::mojom::BudgetServiceRequest request) {
  new BudgetServiceImpl(render_process_id, std::move(request));
}

BudgetServiceImpl::BudgetServiceImpl(int render_process_id,
                                     blink::mojom::BudgetServiceRequest request)
    : render_process_id_(render_process_id),
      binding_(this, std::move(request)) {}

BudgetServiceImpl::~BudgetServiceImpl() = default;

void BudgetServiceImpl::GetCost(blink::mojom::BudgetOperationType type,
                                const GetCostCallback& callback) {
  // The RenderProcessHost should still be alive as long as any connections are
  // alive, and if the BudgetService mojo connection is down, the
  // BudgetServiceImpl should have been destroyed.
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id_);
  DCHECK(host);

  // Query the BudgetManager for the cost and return it.
  content::BrowserContext* context = host->GetBrowserContext();
  double cost = BudgetManagerFactory::GetForProfile(context)->GetCost(type);
  callback.Run(cost);
}

void BudgetServiceImpl::GetBudget(const url::Origin& origin,
                                  const GetBudgetCallback& callback) {
  // TODO(harkness) Call the BudgetManager once it supports detailed GetBudget
  // calls.
  callback.Run(0);
}
