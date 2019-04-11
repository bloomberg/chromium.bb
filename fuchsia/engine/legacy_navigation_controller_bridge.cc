// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/legacy_navigation_controller_bridge.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "fuchsia/engine/legacy_frame_bridge.h"

namespace {

std::unique_ptr<chromium::web::NavigationEntry> ConvertToLegacyNavigationEntry(
    const fuchsia::web::NavigationState& entry) {
  chromium::web::NavigationEntry converted;
  bool is_changed = false;

  if (entry.has_title()) {
    converted.title = entry.title();
    is_changed = true;
  }
  if (entry.has_url()) {
    converted.url = entry.url();
    is_changed = true;
  }
  if (entry.has_page_type()) {
    converted.is_error = entry.page_type() == fuchsia::web::PageType::ERROR;
    is_changed = true;
  }

  return is_changed
             ? std::make_unique<chromium::web::NavigationEntry>(converted)
             : nullptr;
}

std::vector<uint8_t> StringToUnsignedVector(base::StringPiece str) {
  const uint8_t* raw_data = reinterpret_cast<const uint8_t*>(str.data());
  return std::vector<uint8_t>(raw_data, raw_data + str.length());
}

}  // namespace

LegacyNavigationControllerBridge::LegacyNavigationControllerBridge(
    fidl::InterfaceRequest<chromium::web::NavigationController> request,
    fuchsia::web::NavigationControllerPtr handle)
    : binding_(this, std::move(request)),
      navigation_controller_(std::move(handle)) {
  binding_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " |binding_| disconnected.";
    delete this;
  });
  navigation_controller_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " |navigation_controller_| disconnected.";
    delete this;
  });
}

LegacyNavigationControllerBridge::~LegacyNavigationControllerBridge() = default;

void LegacyNavigationControllerBridge::LoadUrl(
    std::string url,
    chromium::web::LoadUrlParams params) {
  fuchsia::web::LoadUrlParams converted;
  if (params.has_type())
    converted.set_type(static_cast<fuchsia::web::LoadUrlReason>(params.type()));
  if (params.has_referrer_url())
    converted.set_referrer_url(std::move(*params.mutable_referrer_url()));
  if (params.has_was_user_activated())
    converted.set_was_user_activated(params.was_user_activated());
  if (params.has_headers()) {
    std::vector<fuchsia::net::http::Header> headers;
    headers.reserve(params.headers().size());
    for (const auto& header : params.headers()) {
      base::StringPiece full_header(
          reinterpret_cast<const char*>(header.data()));
      std::vector<std::string> values = base::SplitString(
          full_header, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (values.size() != 2) {
        // Badly formed header. Ignore.
        continue;
      }
      fuchsia::net::http::Header http_header;
      http_header.name = StringToUnsignedVector(values[0]);
      http_header.value = StringToUnsignedVector(values[1]);
      headers.emplace_back(http_header);
    }
    converted.set_headers(std::move(headers));
  }

  navigation_controller_->LoadUrl(
      std::move(url), std::move(converted),
      [](fuchsia::web::NavigationController_LoadUrl_Result) {});
}

void LegacyNavigationControllerBridge::GoBack() {
  navigation_controller_->GoBack();
}

void LegacyNavigationControllerBridge::GoForward() {
  navigation_controller_->GoForward();
}

void LegacyNavigationControllerBridge::Stop() {
  navigation_controller_->Stop();
}

void LegacyNavigationControllerBridge::Reload(chromium::web::ReloadType type) {
  navigation_controller_->Reload(static_cast<fuchsia::web::ReloadType>(type));
}

void LegacyNavigationControllerBridge::GetVisibleEntry(
    GetVisibleEntryCallback callback) {
  navigation_controller_->GetVisibleEntry(
      [callback = std::move(callback)](
          fuchsia::web::NavigationState navigation_entry) {
        callback(ConvertToLegacyNavigationEntry(navigation_entry));
      });
}
