# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dictionary of configuration types for cbuildbot.

Each dictionary entry is in turn a dictionary of config_param->value.

config_param's:
board -- The board of the image to build.

master -- This bot pushes changes to the overlays.
important -- Master bot uses important bots to determine overall status.
             i.e. if master bot succeeds and other important slaves succeed
             then the master will uprev packages.  This should align
             with info vs. closer except for the master.and options.tests
hostname -- Needed for 'important' slaves.  The hostname of the bot.  Should
            match hostname in slaves.cfg in buildbot checkout.

useflags -- emerge use flags to use while setting up the board, building
            packages, making images, etc.
chromeos_official -- Set the variable CRHOMEOS_OFFICIAL for the build.
                     Known to affect parallel_emerge, cros_set_lsb_release,
                     and chromeos_version.sh. See bug chromium-os:14649

uprev -- Uprevs the local ebuilds to build new changes since last stable.
         build.  If master then also pushes these changes on success.
rev_overlays -- Select what overlays to look at for revving. This can be
                'public', 'private' or 'both'.
push_overlays -- Select what overlays to push at. This should be a subset of
                 rev_overlays for the particular builder.  Must be None if
                 not a master.  There should only be one master bot pushing
                 changes to each overlay per branch.
chrome_rev -- Uprev Chrome, values of 'tot', 'stable_release', or None.

chrome_tests -- runs chrome testing binaries in a vm.

unittests -- Runs unittests for packages.
quick_unit -- If unittests is true, only run the unit tests for packages which
              have changed since the previous build.

vm_tests -- Runs the smoke suite and au test harness in a qemu-based VM
            using KVM.
quick_vm -- If vm_tests is true, run a minimal au test harness suite.

usepkg -- Use binary packages to bootstrap, when possible. (emerge --usepkg)

chroot_replace -- wipe and replace chroot, but not source.

gs_path -- Google Storage path to offload files to.
           None - No upload
           'default' - 'gs://chromeos-archive/' + bot_id
           value - Upload to explicit path

build_type -- Type of builder [binary | full | chrome].
test_mod -- Create a test mod image for archival.
factory_install_mod -- Create a factory install image for archival.
factory_test_mod -- Create a factory test image for archival.

push_image -- Do we push a final release image to chromeos-images.
upload_symbols -- Do we upload debug symbols.

git_url -- git repository URL for our manifests.
           External: http://git.chromium.org/chromiumos/manifest.git
           Internal:
               ssh://gerrit-int.chromium.org:29419/chromeos/manifest-internal

manifest_version -- URL to git repo to store per-build manifest.
                    Usually None or
                    MANIFEST_VERSIONS_INT_URL
"""

GS_PATH_DEFAULT = 'default'

GERRIT_URL = 'ssh://gerrit.chromium.org:29418'
GERRIT_INT_URL = 'ssh://gerrit-int.chromium.org:29419'

MANIFEST_URL = 'http://git.chromium.org/chromiumos/manifest.git'
MANIFEST_INT_URL = GERRIT_INT_URL + '/chromeos/manifest-internal'

MANIFEST_VERSIONS_URL = GERRIT_URL + '/chromiumos/manifest-versions-external'
MANIFEST_VERSIONS_INT_URL = GERRIT_INT_URL + '/chromeos/manifest-versions'

default = {
  # 'board' No default value

  'master' : False,
  'important' : False,
  # 'hostname' No default value

  'useflags' : None,
  'chromeos_official' : False,
  'usepkg' : True,

  'chroot_replace' : False,

  'uprev' : False,
  'rev_overlays': 'public',
  'push_overlays': None,
  'chrome_rev' : None,

  'chrome_tests' : False,

  'unittests' : True,
  'quick_unit' : True,

  'vm_tests' : True,
  'quick_vm' : True,

  'gs_path': GS_PATH_DEFAULT,

  'build_type': 'binary',
  'archive_build_debug' : False,

  'test_mod' : False,
  'factory_install_mod' : False,
  'factory_test_mod' : False,

  'push_image' : False,
  'upload_symbols' : False,

  'git_url' : MANIFEST_URL,
  'manifest_version' : None,
}

arm = {
  # VM/tests are broken on arm.
  'unittests' : False,
  'vm_tests' : False,

  # These images don't work for arm.
  'factory_install_mod' : False,
  'factory_test_mod' : False,
}

full = {
  # Full builds are test build to show that we can build from scratch,
  # so use settings to build from scratch, and archive the results.
  'usepkg' : False,
  'chroot_replace' : True,

  'quick_unit' : False,
  'quick_vm' : True,

  'build_type': 'full',
  'archive_build_debug' : True,
  'test_mod' : True,
  'factory_install_mod' : True,
  'factory_test_mod' : True,
}

internal = {
  'git_url' : MANIFEST_INT_URL,
}

#
# Internal Builds
#

release = {
  # Typical matching cbuild command line
  # --autorev --official --officialversion --chromeos --with-pdf
  #   --bvt --clean --no-gstorage --ctest
  'chrome_tests' : True,
  'manifest_version' : MANIFEST_VERSIONS_INT_URL,
  # --official
  # --officialversion
}

official = {

  # Typical matching cbuild command line
  # master --official --chromeos --clean --upload-board-prebuilt
  #   --ctest --unittests --bvt

  'useflags' : ['chrome_internal', 'chrome_pdf'],
  'chromeos_official' : True,

  # TODO(dgarret): turned off for testing
  'vm_tests' : False,

  # Enable these two temporarily for Bot Testing
  'push_image' : True,
  'upload_symbols' : True,

  'gs_path' : None,

  # cbuild --official
}

config = {}

def add_config(name, updates):
  new_config = default.copy()
  for update_config in updates:
    new_config.update(update_config)

  config[name] = new_config

#
# External Builds
#

add_config('x86-generic-pre-flight-queue', [{
  'board' : 'x86-generic',
  'master' : True,
  'hostname' : 'chromeosbuild2',
  'important': False,

  'uprev' : True,
  'rev_overlays': 'public',
  'push_overlays': 'public',
}])

add_config('x86-generic-chrome-pre-flight-queue', [{
  'board' : 'x86-generic',
  'master' : True,

  'build_type': 'chrome',
  'uprev' : False,
  'chrome_tests' : True,
  'rev_overlays': 'public',
  'push_overlays': 'public',
}])


add_config('x86-mario-pre-flight-queue', [internal, {
  'board' : 'x86-mario',
  'master' : True,

  'uprev' : True,
  'rev_overlays': 'both',
  'push_overlays': 'private',
  'gs_path': 'gs://chromeos-x86-mario/pre-flight-master'
}])

add_config('x86-alex-pre-flight-branch', [internal, {
  'board' : 'x86-alex',
  'master' : False,

  'uprev' : True,
  'rev_overlays': 'both',
  'push_overlays': None,
}])

add_config('x86-mario-pre-flight-branch', [internal, {
  'board' : 'x86-mario',
  'master' : True,

  'uprev' : True,
  'rev_overlays': 'both',
  'push_overlays': 'both',
}])

add_config('x86-agz-bin', [{
  'board' : 'x86-agz',

  'uprev' : True,
  'rev_overlays': 'both',
  'push_overlays': None,
}])

add_config('x86-dogfood-bin', [{
  'board' : 'x86-dogfood',

  'uprev' : True,
  'rev_overlays': 'both',
  'push_overlays': None,
}])

add_config('x86-pineview-bin', [{
  'board' : 'x86-pineview',

  'uprev' : True,
  'rev_overlays': 'public',
  'push_overlays': None,
}])

add_config('arm-tegra2-bin', [arm, {
  'board' : 'tegra2_dev-board',

  'uprev' : True,
  'rev_overlays': 'public',
  'push_overlays': None,
}])

add_config('arm-generic-bin', [arm, {
  'board' : 'arm-generic',

  'uprev' : True,
  'rev_overlays': 'public',
  'push_overlays': None,
}])

add_config('arm-generic-full', [arm, full, {
  'board' : 'arm-generic',
}])

add_config('arm-tegra2-full', [arm, full, {
  'board' : 'tegra2_dev-board',
}])

add_config('arm-tegra2-seaboard-full', [arm, full, {
  'board' : 'tegra2_seaboard',
}])

add_config('x86-generic-full', [full, {
  'board' : 'x86-generic',
}])

add_config('x86-pineview-full', [full, {
  'board' : 'x86-pineview',
}])

#
# Internal Builds
#

add_config('x86-mario-pre-flight-queue', [internal, {
  'board' : 'x86-mario',
  'master' : True,

  'uprev' : True,
  'rev_overlays': 'both',
  'push_overlays': 'private',
  'chrome_rev' : 'stable_release',

  'gs_path': 'gs://chromeos-x86-mario/pre-flight-master'
}])

# cbuild --board=x86-mario  master --official --chromeos --clean
#   --upload-board-prebuilt --ctest --unittests --bvt
add_config('x86-mario-private-full', [internal, full, official, {
  'board' : 'x86-mario',

  'master' : True,
  'uprev' : True,
  'rev_overlays': 'both',
  'push_overlays': 'private',
  # TODO(sosa): This is wrong but restores don's builder to build with mvp.
  'manifest_version' : MANIFEST_VERSIONS_INT_URL,
}])

# cbuild --board=x86-zgb  master --official --chromeos --clean
#   --upload-board-prebuilt --ctest --unittests --bvt
add_config('x86-zgb-private-full', [internal, full, official, {
  'board' : 'x86-zgb',

  'master' : True,
  'uprev' : True,
  'rev_overlays': 'both',
  'push_overlays': 'private',
}])

# cbuild --board=x86-alex master --official --chromeos --clean
#   --upload-board-prebuilt --unittests --bvt
add_config('x86-alex-private-full', [internal, full, official, {
  'board' : 'x86-alex',

  'master' : True,
  'uprev' : True,
  'rev_overlays': 'both',
  'push_overlays': 'private',
}])

# cbuild --board=tegra2_seaboard master --official --chromeos --clean
#   --upload-board-prebuilt
add_config('arm-tegra2_seaboard-private-full', [internal, full, official, {
  'board' : 'arm-tegra2_seaboard',

  'master' : True,
  'uprev' : True,
  'rev_overlays': 'both',
  'push_overlays': 'private',
}])

# cbuild --board=tegra2_aebl  master --official --chromeos --clean
#   --upload-board-prebuilt
add_config('arm-tegra2_seaboard-private-full', [internal, full, official, {
  'board' : 'arm-tegra2_seaboard',

  'master' : True,
  'uprev' : True,
  'rev_overlays': 'both',
  'push_overlays': 'private',
}])

add_config('x86-mario-release', [internal, full, official, release, {
  'board' : 'x86-mario',
}])

add_config('x86-alex-release', [internal, full, official, release, {
  'board' : 'x86-alex',
}])

add_config('x86-zgb-release', [internal, full, official, release, {
  'board' : 'x86-zgb',
}])

add_config('arm-tegra2_seaboard-release', [
    internal, full, official, release, arm, {
  'board' : 'tegra2_seaboard',
}])

add_config('arm-tegra2_aebl-release', [
    internal, full, official, release, arm, {
  'board' : 'tegra2_aebl',
}])

add_config('arm-tegra2_kaen-release', [
    internal, full, official, release, arm, {
  'board' : 'tegra2_kaen',
}])
