// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_tracing_bridge.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "content/public/browser/browser_thread.h"

namespace arc {

namespace {

// The prefix of the categories to be shown on the trace selection UI.
// The space at the end of the string is intentional as the separator between
// the prefix and the real categories.
constexpr char kCategoryPrefix[] = TRACE_DISABLED_BY_DEFAULT("android ");

}  // namespace

struct ArcTracingBridge::Category {
  // The name used by Android to trigger tracing.
  std::string name;
  // The full name shown in the tracing UI in chrome://tracing.
  std::string full_name;
};

ArcTracingBridge::ArcTracingBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), weak_ptr_factory_(this) {
  arc_bridge_service()->tracing()->AddObserver(this);
  content::ArcTracingAgent::GetInstance()->SetDelegate(this);
}

ArcTracingBridge::~ArcTracingBridge() {
  content::ArcTracingAgent::GetInstance()->SetDelegate(nullptr);
  arc_bridge_service()->tracing()->RemoveObserver(this);
}

void ArcTracingBridge::OnInstanceReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::TracingInstance* tracing_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->tracing(), QueryAvailableCategories);
  if (!tracing_instance)
    return;
  tracing_instance->QueryAvailableCategories(base::Bind(
      &ArcTracingBridge::OnCategoriesReady, weak_ptr_factory_.GetWeakPtr()));
}

void ArcTracingBridge::OnCategoriesReady(
    const std::vector<std::string>& categories) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // There is no API in TraceLog to remove a category from the UI. As an
  // alternative, the old category that is no longer in |categories_| will be
  // ignored when calling |StartTracing|.
  categories_.clear();
  for (const auto& category : categories) {
    categories_.push_back({category, kCategoryPrefix + category});
    // Show the category name in the selection UI.
    base::trace_event::TraceLog::GetCategoryGroupEnabled(
        categories_.back().full_name.c_str());
  }
}

void ArcTracingBridge::StartTracing(
    const base::trace_event::TraceConfig& trace_config,
    const StartTracingCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  mojom::TracingInstance* tracing_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->tracing(), StartTracing);
  if (!tracing_instance) {
    // Use PostTask as the convention of TracingAgent. The caller expects
    // callback to be called after this function returns.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, false));
    return;
  }

  std::vector<std::string> selected_categories;
  for (const auto& category : categories_) {
    if (trace_config.IsCategoryGroupEnabled(category.full_name.c_str()))
      selected_categories.push_back(category.name);
  }

  tracing_instance->StartTracing(selected_categories, callback);
}

void ArcTracingBridge::StopTracing(const StopTracingCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  mojom::TracingInstance* tracing_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service()->tracing(), StopTracing);
  if (!tracing_instance) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, false));
    return;
  }
  tracing_instance->StopTracing(callback);
}

}  // namespace arc
