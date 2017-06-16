// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/webshare_target.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"

WebShareTarget::WebShareTarget(GURL manifest_url,
                               std::string name,
                               std::string url_template)
    : manifest_url_(std::move(manifest_url)),
      name_(std::move(name)),
      url_template_(std::move(url_template)) {}

WebShareTarget::~WebShareTarget() {}

bool WebShareTarget::operator==(const WebShareTarget& other) const {
  return std::tie(manifest_url_, name_, url_template_) ==
         std::tie(other.manifest_url_, other.name_, other.url_template_);
}

std::ostream& operator<<(std::ostream& out, const WebShareTarget& target) {
  return out << "WebShareTarget(GURL(" << target.manifest_url().spec() << "), "
             << target.name() << ", " << target.url_template() << ")";
}
