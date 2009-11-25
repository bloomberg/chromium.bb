// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stand alone media player application used for testing the media library.

#ifndef MEDIA_TOOLS_PLAYER_WTL_PLAYER_WTL_H_
#define MEDIA_TOOLS_PLAYER_WTL_PLAYER_WTL_H_

// Enable timing code by turning on TESTING macro.
//#define TESTING 1

// ATL and WTL require order dependent includes.
#include <atlbase.h>    // NOLINT
#include <atlapp.h>     // NOLINT
#include <atlcrack.h>   // NOLINT
#include <atlctrls.h>   // NOLINT
#include <atlctrlw.h>   // NOLINT
#include <atldlgs.h>    // NOLINT
#include <atlframe.h>   // NOLINT
#include <atlmisc.h>    // NOLINT
#include <atlscrl.h>    // NOLINT
#include <winspool.h>   // NOLINT
#include <atlprint.h>   // NOLINT

extern CAppModule g_module;

#endif  // MEDIA_TOOLS_PLAYER_WTL_PLAYER_WTL_H_
