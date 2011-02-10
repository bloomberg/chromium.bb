// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROTOCOL_PROTO_ENUM_CONVERSIONS_H_
#define CHROME_BROWSER_SYNC_PROTOCOL_PROTO_ENUM_CONVERSIONS_H_
#pragma once

// Keep this file in sync with the .proto files in this directory.

#include "chrome/browser/sync/protocol/session_specifics.pb.h"

// Utility functions to get the string equivalent for some sync proto
// enums.

namespace browser_sync {

// The returned strings (which don't have to be freed) are in ASCII.
// The result of passing in an invalid enum value is undefined.

const char* GetBrowserTypeString(
    sync_pb::SessionWindow::BrowserType browser_type);

const char* GetPageTransitionString(
    sync_pb::TabNavigation::PageTransition page_transition);

const char* GetPageTransitionQualifierString(
    sync_pb::TabNavigation::PageTransitionQualifier
        page_transition_qualifier);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_PROTOCOL_PROTO_ENUM_CONVERSIONS_H_
