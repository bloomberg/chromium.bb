// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_installer.h"

PluginInstaller::~PluginInstaller() {
}

PluginInstaller::PluginInstaller(const std::string& identifier,
                                 const GURL& plugin_url,
                                 const GURL& help_url,
                                 const string16& name,
                                 bool url_for_display)
    : identifier_(identifier),
      plugin_url_(plugin_url),
      help_url_(help_url),
      name_(name),
      url_for_display_(url_for_display) {
}
