// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_INIT_WIN_H_
#define CHROME_BROWSER_UI_BROWSER_INIT_WIN_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"

class Profile;

namespace browser {
// If we are in Windows 8 metro mode and were launched as a result of the
// search charm or via a url navigation in metro, then this function fetches
// the url/search term from the metro driver.
GURL GetURLToOpen(Profile* profile);
}  // namespace browser;

#endif  // CHROME_BROWSER_UI_BROWSER_INIT_WIN_H_
