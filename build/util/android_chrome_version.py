# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Different build variants of chrome for android have different version codes.
Reason: for targets that have the same package name (e.g. chrome, chome
modern, monochrome, trichrome), Play Store considers them the same app
and will push the supported app with the highest version code to devices.
(note Play Store does not support hosting two different apps with same
version code and package name)
"""
ANDROID_CHROME_APK_VERSION_CODE_DIFFS = {
    'CHROME': 0,
    'CHROME_MODERN': 1,
    'MONOCHROME': 2,
    'TRICHROME': 3,
    'WEBVIEW': 0
}

"""The architecture preference is encoded into the version_code for devices
that support multiple architectures. (exploiting play store logic that pushes
apk with highest version code)

Detail:
Many Android devices support multiple architectures, and can run applications
built for any of them; the Play Store considers all of the supported
architectures compatible and does not, itself, have any preference for which
is "better". The common cases here:

- All production arm64 devices can also run arm
- All production x64 devices can also run x86
- Pretty much all production x86/x64 devices can also run arm (via a binary
  translator)

Since the Play Store has no particular preferences, you have to encode your own
preferences into the ordering of the version codes. There's a few relevant
things here:

- For any android app, it's theoretically preferable to ship a 64-bit version to
  64-bit devices if it exists, because the 64-bit architectures are supposed to
  be "better" than their 32-bit predecessors (unfortunately this is not always
  true due to the effect on memory usage, but we currently deal with this by
  simply not shipping a 64-bit version *at all* on the configurations where we
  want the 32-bit version to be used).
- For any android app, it's definitely preferable to ship an x86 version to x86
  devices if it exists instead of an arm version, because running things through
  the binary translator is a performance hit.
- For WebView, Monochrome, and Trichrome specifically, they are a special class
  of APK called "multiarch" which means that they actually need to *use* more
  than one architecture at runtime (rather than simply being compatible with
  more than one). The 64-bit builds of these multiarch APKs contain both 32-bit
  and 64-bit code, so that Webview is available for both ABIs. If you're
  multiarch you *must* have a version that supports both 32-bit and 64-bit
  version on a 64-bit device, otherwise it won't work properly. So, the 64-bit
  version needs to be a higher versionCode, as otherwise a 64-bit device would
  prefer the 32-bit version that does not include any 64-bit code, and fail.
- The relative order of mips isn't important, but it needs to be a *distinct*
  value to the other architectures because all builds need unique version codes.
"""
ARCH_VERSION_CODE_DIFF = {
    'arm': 0,
    'x86': 10,
    'mipsel': 20,
    'arm64': 30,
    'x64': 60
}
ARCH_CHOICES = ARCH_VERSION_CODE_DIFF.keys()

""" "Next" builds get +5 last version code digit.
We choose 5 because it won't conflict with values in
ANDROID_CHROME_APK_VERSION_CODE_DIFFS

We also increment BUILD (branch) number to ensure that the version code is
higher for the next build than any build with the same BUILD value (even if the
other builds have a higher PATCH value). This is needed for release logistics
when working with unreleased Android versions: upgrading android will install
the chrome build (the "next" build) that uses the new android sdk.
"""
NEXT_BUILD_VERSION_CODE_DIFF = 100005


def GenerateVersionCodes(version_values, arch, is_next_build):
  """Get dict of version codes for chrome-for-android-related targets

  e.g.
  {
    'CHROME_VERSION_CODE': '378100010',
    'MONOCHROME_VERSION_CODE': '378100013',
    ...
  }

  versionCode values are built like this:
  {full BUILD int}{3 digits for PATCH}{1 digit for architecture}{final digit}.

  MAJOR and MINOR values are not used for generating versionCode.
  - MINOR is always 0. It was used for something long ago in Chrome's history
    but has not been used since, and has never been nonzero on Android.
  - MAJOR is cosmetic and controlled by the release managers. MAJOR and BUILD
    always have reasonable sort ordering: for two version codes A and B, it's
    always the case that (A.MAJOR < B.MAJOR) implies (A.BUILD < B.BUILD), and
    that (A.MAJOR > B.MAJOR) implies (A.BUILD > B.BUILD). This property is just
    maintained by the humans who set MAJOR.

  Thus, this method is responsible for the final two digits of versionCode.
  """

  base_version_code = '%s%03d00' % (version_values['BUILD'],
                                    int(version_values['PATCH']))
  new_version_code = int(base_version_code)

  new_version_code += ARCH_VERSION_CODE_DIFF[arch]
  if is_next_build:
    new_version_code += NEXT_BUILD_VERSION_CODE_DIFF

  version_codes = {}
  for apk, diff in ANDROID_CHROME_APK_VERSION_CODE_DIFFS.iteritems():
    version_codes[apk + '_VERSION_CODE'] = str(new_version_code + diff)

  return version_codes
