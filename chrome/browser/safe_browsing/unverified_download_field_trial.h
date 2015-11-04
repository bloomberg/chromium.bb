// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_UNVERIFIED_DOWNLOAD_FIELD_TRIAL_H_
#define CHROME_BROWSER_SAFE_BROWSING_UNVERIFIED_DOWNLOAD_FIELD_TRIAL_H_

namespace base {
class FilePath;
}

namespace safe_browsing {

// Unverified Download Field Trial
//
// It is possible in some circumstances to download a file without invoking
// SafeBrowsing to verify the source URLs or the downloaded contents. Ideally
// all code paths that writes untrusted contents to disk in a user discoverable
// manner would invoke SafeBrowsing. But until all such code paths are properly
// plumbed, this field trial will help control the behavior of known unverified
// code paths.

extern const char kUnverifiedDownloadFieldTrialName[];

// The trial groups are:
//
// * DisableByParameter[*] : Decides the fate of unverified downloads based on
//     whether the file type matches a whitelist and a blacklist specified as a
//     parameter to this trial group. Any file type that doesn't match either of
//     these lists will default to being allowed.
extern const char kUnverifiedDownloadFieldTrialDisableByParameter[];

//     The parameters are:
//
//     - 'whitelist' : A comma separated list of file types including leading
//         extension separator. E.g. ".abc,.def" would match both foo.abc and
//         foo.def, but not foo.txt. Any matching file will be allowed to be
//         downloaded.
extern const char kUnverifiedDownloadFieldTrialWhitelistParam[];

//     - 'blacklist' : Similar in format to 'whitelist' but causes matching file
//         types to be disallowed from downloading.
extern const char kUnverifiedDownloadFieldTrialBlacklistParam[];

//     - 'block_sb_types' : If non-empty, causes all file types that satisfy
//         safe_browsing::IsSupportedBinaryFile() to be blocked.
extern const char kUnverifiedDownloadFieldTrialBlockSBTypesParam[];

// Returns true if |file| is allowed to be downloaded based on the current field
// trial settings.
bool IsUnverifiedDownloadAllowedByFieldTrial(const base::FilePath& file);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_UNVERIFIED_DOWNLOAD_FIELD_TRIAL_H_
