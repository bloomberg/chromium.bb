// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_notification.h"

AppNotification::AppNotification(const std::string& title,
                                 const std::string& body)
    : title_(title), body_(body) {}

AppNotification::~AppNotification() {}
