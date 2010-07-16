// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/notification_details.h"

NotificationDetails::NotificationDetails() : ptr_(NULL) {
}

NotificationDetails::NotificationDetails(const NotificationDetails& other)
    : ptr_(other.ptr_) {
}

NotificationDetails::NotificationDetails(const void* ptr) : ptr_(ptr) {
}

NotificationDetails::~NotificationDetails() {
}
