// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_SIDEBAR_DEFAULTS_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_SIDEBAR_DEFAULTS_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "googleurl/src/gurl.h"

class SkBitmap;

// ExtensionSidebarDefaults encapsulates the default parameters of a sidebar,
// as defined in the extension manifest.
class ExtensionSidebarDefaults {
 public:
  // Default title, stores manifest default_title key value.
  void set_default_title(const string16& title) {
    default_title_ = title;
  }
  const string16& default_title() const { return default_title_; }

  // Default icon path, stores manifest default_icon key value.
  void set_default_icon_path(const std::string& path) {
    default_icon_path_ = path;
  }
  const std::string& default_icon_path() const {
    return default_icon_path_;
  }

  // Default sidebar url, resolved and verified against extension permissions.
  void set_default_url(const GURL& url) {
    default_url_ = url;
  }
  const GURL& default_url() const {
    return default_url_;
  }

 private:
  string16 default_title_;
  std::string default_icon_path_;
  GURL default_url_;
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_SIDEBAR_DEFAULTS_H_
