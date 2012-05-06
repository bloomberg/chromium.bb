// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_EXTENSION_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_EXTENSION_UTILS_H_
#pragma once

class Profile;
class Extension;

namespace extension_utils {

// Opens an extension.  |event_flags| holds the flags of the event
// which triggered this extension.
void OpenExtension(Profile* profile,
                   const Extension* extension,
                   int event_flags);

}  // namespace extension_utils

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_EXTENSION_UTILS_H_
