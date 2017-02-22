# Adding third_party Libraries

[TOC]

Using third party code can save time and is consistent with our values - no need
to reinvent the wheel! We put all code that isn't written by Chromium developers
into src/third_party (even if you end up modifying just a few functions). We do
this to make it easy to track license compliance, security patches, and supply
the right credit and attributions. It also makes it a lot easier for other
projects that embed our code to track what is Chromium licensed and what is
covered by other licenses.

## Get the Code

When you find code you want to use, get it. This often means downloading: from
Sourceforge, from the hosting feature of Google Code, or from somewhere else.
Sometimes it can mean negotiating a license with another company and receiving
the code another way. Please describe the source in the README.chromium file,
described below.  For security reasons, please retrieve the code as securely as
you can, using HTTPS and GPG signatures if available.  If retrieving a tarball,
please do not check the tarball itself into the tree, but do list the source and
the SHA-512 hash (for verification) in the README.chromium and Change List. The
SHA-512 hash can be computed via the `shasum (sha512sum)` or `openssl`
utilities.  If retrieving from a git repository, please list the SHA-512 hash.

## Put the Code in (the right) third_party

By default, all code should be checked into
[src/third_party](http://src.chromium.org/viewvc/chrome/trunk/src/third_party/).
It is OK to have third_party subdirectories that are not top-level (e.g.
src/net/third_party), but don't add more third_party directories than necessary.

## Document the Code's Context

### Add OWNERS

Your OWNERS file must include 2 Chromium developer accounts. This will ensure
accountability for maintenance of the code over time. While there isn't always
an ideal or obvious set of people that should go in OWNERS, this is critical for
first-line triage of any issues that crop up in the code.

As an OWNER, you're expected to:

* Remove the dependency when/if it is no longer needed
* Update the dependency when a security or stability bug is fixed upstream
* Help ensure the Chrome feature that uses the dependency continues to use the
  dependency in the best way, as the feature and the dependency change over
  time.

### Add a README.chromium

You need a README.chromium file with information about the project from which
you're re-using code. See
[README.chromium.template](http://src.chromium.org/viewvc/chrome/trunk/src/third_party/README.chromium.template)
for a list of fields to include. A presubmit check will check this has the right
format.

### Add a LICENSE file and run related checks

You need a LICENSE file. Example:
[third_party/libjpeg/LICENSE](http://src.chromium.org/viewvc/chrome/trunk/src/third_party/libjpeg/LICENSE?revision=42288&view=markup).

Run the following scripts:

* `src/tools/licenses.py scan` - This will complain about incomplete or missing
  data for third_party checkins. We use 'licenses.py credits' to generate the
  about:credits page in Google Chrome builds.
* `src/tools/checklicenses/checklicenses.py` - See below for info how to handle
  possible failures.
* If you are adding code that will be present in the content layer, please make
  sure that the license used is compliant with Android tree requirements because
  this code will also be used in Android WebView. You need to run
  `src/android_webview/tools/webview_licenses.py scan`

See the ["Odds n' Ends"](adding_to_third_party.md#Odds-n_Ends) Section below if
you run into any failures running these.

If the library will never be shipped as a part of Chrome (e.g. build-time tools,
testing tools), make sure to set "License File" as "NOT_SHIPPED" so that the
license is not included in about:credits page.

### Modify DEPS

If the code is applicable and will be compiled on all supported Chromium
platforms (Windows, Mac, Linux, Chrome OS, iOS, Android), check it in to
[src/third_party](http://src.chromium.org/viewvc/chrome/trunk/src/third_party/). 

If the code is only applicable to certain platforms, check it in to
[src/third_party](http://src.chromium.org/viewvc/chrome/trunk/src/third_party/)
and add an entry to the
[DEPS](http://src.chromium.org/viewvc/chrome/trunk/src/DEPS) file that causes
the code to be checked out from src/third_party into src/third_party by gclient.

_Explanation: Checking it into src/third_party causes all developers to need to
check out your code. This wastes disk space cause syncing to take longer for
developers that don't need your code. When all platforms really do need the
code, checking it in to src/third_party allows some slight improvements over
DEPS._

As for specifying the path where the library is fetched, a path like
`src/third_party/<project_name>/src` is highly recommended so that you can put
the file like OWNERS or README.chromium at `third_party/<project_name>`. If you
have a wrong path in DEPS and want to change the path of the existing library in
DEPS, please ask the infrastructure team before committing the change.

### Checking in large files
_Accessible to Googlers only. Non-Googlers can email one of the people in
third_party/OWNERS for help._

See [Moving large files to Google Storage](https://goto.google.com/checking-in-large-files)

## Setting up ignore

If your code is synced via DEPS, you should add the new directory to Chromium's
`.gitignore`. This is necessary because Chromium's main git repository already
contains
[src/third_party](http://src.chromium.org/viewvc/chrome/trunk/src/third_party/)
and the project synced via DEPS is nested inside of it and its files regarded as
untracked. That is, anyone running `git status` from `src/` would see a clutter.
Your project's files are tracked by your repository, not Chromium's, so make
sure the directory is listed in Chromium's `.gitignore`.

## Get a Review

All third party additions and substantive changes like re-licensing need the
following sign-offs. Some of these are accessible to Googlers only. Non-Googlers
can email one of the people in third_party/OWNERS for help.

* Chrome Eng Review. Googlers should see go/chrome-eng-review (please include information about the additional checkout size, build times, and binary sizes. Please also make sure that the motivation for your project is clear, e.g., a design doc has been circulated).
* open-source-third-party-reviews@google.com (ping the list with relevant
  details and a link to the CL).
* security@chromium.org (ping the list with relevant details and a link to the
  CL).

Please send separate emails to the three lists.

Third party code is a hot spot for security vulnerabilities. When adding a new
package that could potentially carry security risk, make sure to highlight risk
to security@chromium.org. You may be asked to add a README.security or, in
dangerous cases, README.SECURITY.URGENTLY file. When you update your code, be
mindful of security-related mailing lists for the project and relevant CVE to
update your package.

Subsequent changes don't require third-party-owners approval; you can modify the
code as much as you want.

## Ask the infrastructure team to add a git mirror for the dependency (or
configure the git repo, if using googlesource.com)

Before committing the DEPS, you need to ask the infra team to create a git
mirror for your dependency. [Create a
ticket](https://bugs.chromium.org/p/chromium/issues/entry?template=Infra-Git)
for infra and ask the infra team.

If you are using a git repo from googlesource.com then you must ensure that the
repository is configured to give the build bots unlimited quota, or else the
builders will fail to checkout with an "Over Quota" error. [Create a
ticket](https://bugs.chromium.org/p/chromium/issues/entry?template=Infra-Git)
for infra and ask the infra team what needs to be done. Note that you'll need
unlimited quota for at least two role accounts. See the quota status of
`boringssl` as an example.

## Odds n' Ends

### Handling `licenses_check (checklicenses.py)` failures

This is needed for [Issue
28291](http://code.google.com/p/chromium/issues/detail?id=28291): Pass the
Ubuntu license check script:

__If the failure looks like ...   ... the right action is to ... __

* 'filename' has non-whitelisted license 'UNKNOWN'
    * Ideally make the licensecheck.pl script recognize the license of that
      file.  Often this requires __adding a license header__. Does it have
      license header? If it's third party code, ask the upstream project to make
      sure all their files have license headers.  If the license header is there
      but is not recognized, we should try to patch licensecheck.pl.  If in
      doubt please contact phajdan.jr@chromium.org
* 'filename' has non-whitelisted license 'X' and X is BSD-compatible
    * Add the license X to WHITELISTED_LICENSES in checklicenses.py . Make sure
      to respect the OWNERS of that file. You must get an approval before
      landing. CLs violating this requirement may be reverted.
* 'filename' has non-whitelisted license 'X' and X is not BSD-compatible (i.e.
  GPL)
    * Do you really need to add those files? Chromium is BSD-licensed so the
      resulting binaries can't use GPL code. Ideally we just shouldn't have
      those files at all in the tree. If in doubt, please ask mal@chromium.org

### Handling `webview_licenses.py` failures

__If the failure looks like ...   ... the right action is to ... __

* Missing license file
    * Make sure that the license file is present. It should be called 'LICENSE',
      or otherwise README.chromium file must point to it explicitly.
* The following files contain a third-party license but are not in a listed
  third-party directory...
    * Check if it's a false positive (e.g. 'copyright' word used in a string
      literal), if so, update
      [src/tools/copyright_scanner/third_party_files_whitelist.txt](https://code.google.com/p/chromium/codesearch#chromium/src/tools/copyright_scanner/third_party_files_whitelist.txt)
      file. Otherwise, please move the code into third_party.
