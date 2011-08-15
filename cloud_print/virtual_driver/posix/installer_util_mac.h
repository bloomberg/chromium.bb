// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_VIRTUAL_POSIX_INSTALLER_UTIL_MAC_H_
#define CLOUD_PRINT_VIRTUAL_POSIX_INSTALLER_UTIL_MAC_H_
#pragma once

#import <ApplicationServices/ApplicationServices.h>

namespace cloud_print {
void sendServiceProcessEvent(const AEEventClass sendClass);
}   // namespace cloud_print

#endif  // CLOUD_PRINT_VIRTUAL_POSIX_INSTALLER_UTIL_MAC_H_

