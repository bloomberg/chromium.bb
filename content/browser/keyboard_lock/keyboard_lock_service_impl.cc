// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/keyboard_lock/keyboard_lock_service_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/content_features.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace content {

namespace {

// These values must stay in sync with tools/metrics/histograms.xml.
// Enum values should never be renumbered or reused as they are stored and can
// be used for multi-release queries.  Insert any new values before |kCount| and
// increment the count.
enum class KeyboardLockMethods {
  kRequestAllKeys = 0,
  kRequestSomeKeys = 1,
  kCancelLock = 2,
  kCount = 3
};

void LogKeyboardLockMethodCalled(KeyboardLockMethods method) {
  UMA_HISTOGRAM_ENUMERATION("Blink.KeyboardLock.MethodCalled", method,
                            KeyboardLockMethods::kCount);
}

}  // namespace

KeyboardLockServiceImpl::KeyboardLockServiceImpl(
    RenderFrameHost* render_frame_host)
    : render_frame_host_(static_cast<RenderFrameHostImpl*>(render_frame_host)) {
  DCHECK(render_frame_host_);
}

KeyboardLockServiceImpl::~KeyboardLockServiceImpl() = default;

// static
void KeyboardLockServiceImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    blink::mojom::KeyboardLockServiceRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<KeyboardLockServiceImpl>(render_frame_host),
      std::move(request));
}

void KeyboardLockServiceImpl::RequestKeyboardLock(
    const std::vector<std::string>& key_codes,
    RequestKeyboardLockCallback callback) {
  if (key_codes.empty())
    LogKeyboardLockMethodCalled(KeyboardLockMethods::kRequestAllKeys);
  else
    LogKeyboardLockMethodCalled(KeyboardLockMethods::kRequestSomeKeys);

  if (!base::FeatureList::IsEnabled(features::kKeyboardLockAPI)) {
    std::move(callback).Run(blink::mojom::KeyboardLockRequestResult::SUCCESS);
    return;
  }

  if (!render_frame_host_->IsCurrent() || render_frame_host_->GetParent()) {
    // TODO(joedow): Return an error code here.
    std::move(callback).Run(blink::mojom::KeyboardLockRequestResult::SUCCESS);
    return;
  }

  // Per base::flat_set usage notes, the proper way to init a flat_set is
  // inserting into a vector and using that to init the flat_set.
  std::vector<int> native_key_codes;
  const int invalid_key_code = ui::KeycodeConverter::InvalidNativeKeycode();
  for (const std::string& code : key_codes) {
    int native_key_code = ui::KeycodeConverter::CodeStringToNativeKeycode(code);
    if (native_key_code != invalid_key_code)
      native_key_codes.push_back(native_key_code);
  }

  // If we are provided with a vector containing only invalid keycodes, then
  // exit without enabling keyboard lock.  An empty vector is treated as
  // 'capture all keys' which is not what the caller intended.
  if (!key_codes.empty() && native_key_codes.empty()) {
    // TODO(joedow): Return an error code here.
    std::move(callback).Run(blink::mojom::KeyboardLockRequestResult::SUCCESS);
    return;
  }

  base::Optional<base::flat_set<int>> key_code_set;
  if (!native_key_codes.empty())
    key_code_set = std::move(native_key_codes);

  render_frame_host_->GetRenderWidgetHost()->RequestKeyboardLock(
      std::move(key_code_set));

  std::move(callback).Run(blink::mojom::KeyboardLockRequestResult::SUCCESS);
}

void KeyboardLockServiceImpl::CancelKeyboardLock() {
  LogKeyboardLockMethodCalled(KeyboardLockMethods::kCancelLock);

  if (base::FeatureList::IsEnabled(features::kKeyboardLockAPI))
    render_frame_host_->GetRenderWidgetHost()->CancelKeyboardLock();
}

}  // namespace content
