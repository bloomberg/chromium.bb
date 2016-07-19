// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/blimp/chrome_blimp_client_context_delegate.h"

#include "blimp/client/public/blimp_client_context.h"
#include "blimp/client/public/blimp_client_context_delegate.h"
#include "blimp/client/public/contents/blimp_contents.h"
#include "chrome/browser/android/blimp/blimp_client_context_factory.h"
#include "chrome/browser/android/blimp/blimp_contents_profile_attachment.h"
#include "chrome/browser/profiles/profile.h"

ChromeBlimpClientContextDelegate::ChromeBlimpClientContextDelegate(
    Profile* profile)
    : profile_(profile),
      blimp_client_context_(
          BlimpClientContextFactory::GetForBrowserContext(profile)) {
  blimp_client_context_->SetDelegate(this);
}

ChromeBlimpClientContextDelegate::~ChromeBlimpClientContextDelegate() {
  blimp_client_context_->SetDelegate(nullptr);
}

void ChromeBlimpClientContextDelegate::AttachBlimpContentsHelpers(
    blimp::client::BlimpContents* blimp_contents) {
  AttachProfileToBlimpContents(blimp_contents, profile_);
}
