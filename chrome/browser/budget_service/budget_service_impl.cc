// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/budget_service/budget_service_impl.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/budget_service/budget_manager.h"
#include "chrome/browser/budget_service/budget_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

BudgetServiceImpl::BudgetServiceImpl(int render_process_id)
    : render_process_id_(render_process_id) {}

BudgetServiceImpl::~BudgetServiceImpl() = default;

// static
void BudgetServiceImpl::Create(int render_process_id,
                               blink::mojom::BudgetServiceRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<BudgetServiceImpl>(render_process_id),
      std::move(request));
}

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
  // The RenderProcessHost should still be alive as long as any connections are
  // alive, and if the BudgetService mojo connection is down, the
  // BudgetServiceImpl should have been destroyed.
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id_);
  DCHECK(host);

  // Query the BudgetManager for the budget.
  content::BrowserContext* context = host->GetBrowserContext();
  BudgetManagerFactory::GetForProfile(context)->GetBudget(origin, callback);
}

void BudgetServiceImpl::Reserve(const url::Origin& origin,
                                blink::mojom::BudgetOperationType operation,
                                const ReserveCallback& callback) {
  // The RenderProcessHost should still be alive as long as any connections are
  // alive, and if the BudgetService mojo connection is down, the
  // BudgetServiceImpl should have been destroyed.
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id_);
  DCHECK(host);

  // Request a reservation from the BudgetManager.
  content::BrowserContext* context = host->GetBrowserContext();
  BudgetManagerFactory::GetForProfile(context)->Reserve(origin, operation,
                                                        callback);
}
