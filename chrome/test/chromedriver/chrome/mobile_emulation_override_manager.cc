// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/mobile_emulation_override_manager.h"

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/device_metrics.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

MobileEmulationOverrideManager::MobileEmulationOverrideManager(
    DevToolsClient* client,
    const DeviceMetrics* device_metrics)
    : client_(client),
      overridden_device_metrics_(device_metrics) {
  if (overridden_device_metrics_)
    client_->AddListener(this);
}

MobileEmulationOverrideManager::~MobileEmulationOverrideManager() {
}

Status MobileEmulationOverrideManager::OnConnected(DevToolsClient* client) {
  return ApplyOverrideIfNeeded();
}

Status MobileEmulationOverrideManager::OnEvent(
    DevToolsClient* client,
    const std::string& method,
    const base::DictionaryValue& params) {
  if (method == "Page.frameNavigated") {
    const base::Value* unused_value;
    if (!params.Get("frame.parentId", &unused_value))
      return ApplyOverrideIfNeeded();
  }
  return Status(kOk);
}

bool MobileEmulationOverrideManager::IsEmulatingTouch() {
  return overridden_device_metrics_ && overridden_device_metrics_->touch;
}

Status MobileEmulationOverrideManager::ApplyOverrideIfNeeded() {
  if (overridden_device_metrics_ == NULL)
    return Status(kOk);

  base::DictionaryValue params;
  params.SetInteger("width", overridden_device_metrics_->width);
  params.SetInteger("height", overridden_device_metrics_->height);
  params.SetDouble("deviceScaleFactor",
                   overridden_device_metrics_->device_scale_factor);
  params.SetBoolean("mobile", overridden_device_metrics_->mobile);
  params.SetBoolean("fitWindow", overridden_device_metrics_->fit_window);
  params.SetBoolean("textAutosizing",
                    overridden_device_metrics_->text_autosizing);
  params.SetDouble("fontScaleFactor",
                   overridden_device_metrics_->font_scale_factor);
  Status status = client_->SendCommand("Page.setDeviceMetricsOverride", params);
  if (status.IsError())
    return status;

  if (overridden_device_metrics_->touch) {
    base::DictionaryValue emulate_touch_params;
    emulate_touch_params.SetBoolean("enabled", true);
    status = client_->SendCommand("Emulation.setTouchEmulationEnabled",
                                  emulate_touch_params);
    if (status.IsError())
      return status;

  }

  return Status(kOk);
}
