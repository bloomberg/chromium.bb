// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/webshare_target.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"

WebShareTargetFiles::WebShareTargetFiles() {}

WebShareTargetFiles::WebShareTargetFiles(const std::string& name,
                                         const std::vector<std::string>& accept)
    : name_(name), accept_(accept) {}

WebShareTargetFiles::~WebShareTargetFiles() {}

WebShareTargetFiles::WebShareTargetFiles(WebShareTargetFiles&& other) = default;

bool WebShareTargetFiles::operator==(const WebShareTargetFiles& other) const {
  return std::tie(name_, accept_) == std::tie(other.name_, other.accept_);
}

WebShareTarget::WebShareTarget(const GURL& manifest_url,
                               const std::string& name,
                               const GURL& action,
                               const std::string& method,
                               const std::string& enctype,
                               const std::string& text,
                               const std::string& title,
                               const std::string& url,
                               std::vector<WebShareTargetFiles> files)
    : manifest_url_(manifest_url),
      name_(name),
      action_(action),
      method_(method),
      enctype_(enctype),
      text_(text),
      title_(title),
      url_(url),
      files_(std::move(files)) {}

WebShareTarget::~WebShareTarget() {}

WebShareTarget::WebShareTarget(WebShareTarget&& other) = default;

bool WebShareTarget::operator==(const WebShareTarget& other) const {
  return std::tie(manifest_url_, name_, action_, method_, enctype_, text_,
                  title_, url_, files_) ==
         std::tie(other.manifest_url_, other.name_, other.action_,
                  other.method_, other.enctype_, other.text_, other.title_,
                  other.url_, other.files_);
}

std::ostream& operator<<(std::ostream& out, const WebShareTargetFiles& files) {
  out << "WebShareTargetFiles(" << files.name() << ", [";
  for (const std::string& accept : files.accept()) {
    out << accept << ", ";
  }
  return out << "])";
}

std::ostream& operator<<(std::ostream& out, const WebShareTarget& target) {
  out << "WebShareTarget(GURL(" << target.manifest_url().spec() << "), "
      << target.name() << ", " << target.action() << ", " << target.method()
      << ", " << target.enctype() << ", " << target.text() << ", "
      << target.title() << ", " << target.url() << ", [";
  for (const WebShareTargetFiles& files : target.files()) {
    out << files << ", ";
  }
  return out << "])";
}
