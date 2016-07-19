// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/blimp/blimp_contents_profile_attachment.h"

#include "base/logging.h"
#include "base/supports_user_data.h"
#include "blimp/client/public/contents/blimp_contents.h"

namespace {
const char kBlimpContentsProfileAttachment[] =
    "blimp_contents_profile_attachment";

// A holder struct for a Profile that can be used as data.
struct Attachment : public base::SupportsUserData::Data {
  Profile* profile;
};

}  // namespace

void AttachProfileToBlimpContents(blimp::client::BlimpContents* blimp_contents,
                                  Profile* profile) {
  DCHECK(profile);
  Attachment* attachment = new Attachment;
  attachment->profile = profile;
  blimp_contents->SetUserData(kBlimpContentsProfileAttachment, attachment);
}

Profile* GetProfileFromBlimpContents(
    blimp::client::BlimpContents* blimp_contents) {
  Attachment* attachment = static_cast<Attachment*>(
      blimp_contents->GetUserData(kBlimpContentsProfileAttachment));
  DCHECK(attachment);
  return attachment->profile;
}
