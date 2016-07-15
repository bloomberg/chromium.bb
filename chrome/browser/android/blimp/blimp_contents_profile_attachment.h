// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BLIMP_BLIMP_CONTENTS_PROFILE_ATTACHMENT_H_
#define CHROME_BROWSER_ANDROID_BLIMP_BLIMP_CONTENTS_PROFILE_ATTACHMENT_H_

class Profile;

namespace blimp {
namespace client {
class BlimpContents;
}  // namespace client
}  // namespace blimp

// Attaches a profile to the given BlimpContents as SupportsUserData::Data. This
// must be called whenever a new BlimpContents has been created.
void AttachProfileToBlimpContents(blimp::client::BlimpContents* blimp_contents,
                                  Profile* profile);

// Returns the profile related to a specific BlimpContents. It must have been
// previously attached using AttachProfileToBlimpContents.
Profile* GetProfileFromBlimpContents(
    blimp::client::BlimpContents* blimp_contents);

#endif  // CHROME_BROWSER_ANDROID_BLIMP_BLIMP_CONTENTS_PROFILE_ATTACHMENT_H_
