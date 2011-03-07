// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/notification_source.h"

NotificationSource::NotificationSource(const NotificationSource& other)
    : ptr_(other.ptr_) {
}

NotificationSource::NotificationSource(const void* ptr) : ptr_(ptr) {
}

NotificationSource::~NotificationSource() {
}


