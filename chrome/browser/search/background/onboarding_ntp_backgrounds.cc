// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/onboarding_ntp_backgrounds.h"

#include "url/gurl.h"

std::array<GURL, kOnboardingNtpBackgroundsCount> GetOnboardingNtpBackgrounds() {
  // A set of whitelisted NTP background image URLs that are always considered
  // to be valid URLs that are shown to the user as part of the Onboarding flow.
  // These backgrounds were handpicked from the Backdrop API.
  const std::array<GURL, kOnboardingNtpBackgroundsCount>
      kOnboardingNtpBackgrounds = {
          {// Art
           GURL(
               "https://lh4.googleusercontent.com/proxy/"
               "rh9ut9biDoATqaadvgilbUAkIDo50op0G9056C1TnbfK5063_"
               "M1OzTyOSfgbHbwcXzWz-UoM19yeObH2gXCX8wB8hVPbc_MXHBMfJWUmpg1Tg0s="
               "w3840-h2160-p-k-no-nd-mv"),

           // Landscape
           GURL("https://lh6.googleusercontent.com/proxy/"
                "5YUVHiA1x4KXBMvcnQ_"
                "fNcsGFQig9vpKgEbH98vc3mIx6963Jawv4WtVMemvIEdCHGpo60iD8t0OfwnHu"
                "5w"
                "Y5InXTyqTPZx4a77T7iIFnHti=w3840-h2160-p-k-no-nd-mv"),

           // Cityscape
           GURL(
               "https://lh6.googleusercontent.com/proxy/"
               "Ym39mP3aRahUqQD_W1bks3MdDZx-Mea7_ajAs293LciDm70mFzpsswh_"
               "QBJVFeUMWYF2jIQFvcBPeEdNAAwBlFNc2PsINAWHXe68Kg=w3840-h2160-p-k-"
               "no-nd-mv"),

           // Seascape
           GURL("https://lh4.googleusercontent.com/proxy/"
                "hl6qyPDq9FBOuU8G1A3X2khJJdAzfccHtIoX7hHtlkR4tgj_"
                "WJvpj4s8FX8cb8u9R6xhrp96xXJIIFrTK8Rdpqh0tTiSfL5k_OXcWw=w3840-"
                "h2160-p-k-no-nd-mv"),

           // Geometric forms
           GURL("https://lh5.googleusercontent.com/proxy/"
                "Dm41Zdz8s5TnQcIRSCnAXUvHItsB67S7U3DytjCwsIpG9SmJKsVkkBRCK0ODme"
                "vy"
                "_M47-2CSAxGzFaiJM3bHaPfVT90Fm6lBbNd2yueqMg=w3840-h2160-p-k-no-"
                "nd-mv")}};
  return kOnboardingNtpBackgrounds;
}
