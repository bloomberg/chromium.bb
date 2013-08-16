// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_LOCAL_SETTINGS_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_LOCAL_SETTINGS_H_

// Contains local settings.
struct LocalSettings {
  enum State {
    CURRENT,
    PENDING,
    PRINTER_DELETED,
  };

  LocalSettings()
      : local_discovery(true),
        access_token_enabled(false),
        local_printing_enabled(false),
        xmpp_timeout_value(300) {
  }
  ~LocalSettings() {}

  bool local_discovery;
  bool access_token_enabled;
  bool local_printing_enabled;
  int xmpp_timeout_value;
};

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_LOCAL_SETTINGS_H_

