// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROFILE_STARTUP_H_
#define CHROME_BROWSER_CHROMEOS_PROFILE_STARTUP_H_
#pragma once

class Profile;

namespace chromeos {

// Initialize a bunch of services that are tied to a browser profile.
// TODO(beng): It is very fishy that any of these services need to be tied to
//             a browser profile. This dependency should be severed and this
//             init moved earlier in startup.
void ProfileStartup(Profile* profile, bool process_startup);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROFILE_STARTUP_H_
