// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Product-specific constants.

#ifndef CHROME_BROWSER_SYNC_PROTOCOL_SERVICE_CONSTANTS_H_
#define CHROME_BROWSER_SYNC_PROTOCOL_SERVICE_CONSTANTS_H_
#pragma once

// These fixed service names are used to obtain auth cookies for the
// corresponding services. It might be interesting to make these updateable
// as well as have the ability to add new ones.
#define SYNC_SERVICE_NAME "chromiumsync"

#define DEFAULT_SIGNIN_DOMAIN "gmail.com"

#define PRODUCT_NAME_STRING_NARROW "Chromium Browser Sync"

#define PRODUCT_NAME_STRING PRODUCT_NAME_STRING_NARROW
#define PRODUCT_NAME_STRING_WIDE L##PRODUCT_NAME_STRING

#endif  // CHROME_BROWSER_SYNC_PROTOCOL_SERVICE_CONSTANTS_H_
