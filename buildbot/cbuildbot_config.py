# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dictionary of configuration types for cbuildbot.

Each dictionary entry is in turn a dictionary of config_param->value.

config_param's:
board -- The board of the image to build.  If build_type is CHROOT_BUILD_TYPE,
         may be an array of boards to setup.
profile -- The profile of the variant to set up and build.

master -- This bot pushes changes to the overlays.
important -- Master bot uses important bots to determine overall status.
             i.e. if master bot succeeds and other important slaves succeed
             then the master will uprev packages.  This should align
             with info vs. closer except for the master.and options.tests
useflags -- emerge use flags to use while setting up the board, building
            packages, making images, etc.
chromeos_official -- Set the variable CRHOMEOS_OFFICIAL for the build.
                     Known to affect parallel_emerge, cros_set_lsb_release,
                     and chromeos_version.sh. See bug chromium-os:14649

uprev -- Uprevs the local ebuilds to build new changes since last stable.
         build.  If master then also pushes these changes on success.
overlays -- Select what overlays to look at for revving and prebuilts. This
            can be 'public', 'private' or 'both'.
push_overlays -- Select what overlays to push at. This should be a subset of
                 overlays for the particular builder.  Must be None if
                 not a master.  There should only be one master bot pushing
                 changes to each overlay per branch.
chrome_rev -- Uprev Chrome, values of 'tot', 'stable_release', or None.

chrome_tests -- runs chrome testing binaries in a vm.

unittests -- Runs unittests for packages.
quick_unit -- If unittests is true, only run the unit tests for packages which
              have changed since the previous build.

build_tests -- Builds autotest tests.  Must be True if vm_tests is set.

vm_tests -- Runs the smoke suite and au test harness in a qemu-based VM
            using KVM.
quick_vm -- If vm_tests is true, run a minimal au test harness suite.

remote_ip -- IP of the remote Chromium OS machine used for testing.

fast -- Use parallel_emerge for faster (but slightly more risky) builds.
usepkg_chroot -- Use binary packages for make_chroot. (emerge --usepkg)
usepkg_setup_board -- Use binary packages for setup_board. (emerge --usepkg)
usepkg_build_packages -- Use binary packages for build_packages.

chroot_replace -- wipe and replace chroot, but not source.

gs_path -- Google Storage path to offload files to.
           None - No upload
           'default' - 'gs://chromeos-archive/' + bot_id
           value - Upload to explicit path

# TODO(sosa): Deprecate binary.
build_type -- Type of builder.  Checks constants.VALID_BUILD_TYPES.

prebuilts -- Upload prebuilts for this build.

test_mod -- Create a test mod image for archival.
factory_install_mod -- Create a factory install image for archival.
factory_test_mod -- Create a factory test image for archival.

push_image -- Do we push a final release image to chromeos-images.
upload_symbols -- Do we upload debug symbols.

git_url -- git repository URL for our manifests.
           External: http://git.chromium.org/chromiumos/manifest.git
           Internal:
               ssh://gerrit-int.chromium.org:29419/chromeos/manifest-internal

manifest_version -- Whether we are using the manifest_version repo that stores
                    per-build manifests.

use_lkgm -- Use the Last Known Good Manifest blessed by the pre-flight-queue

latest_toolchain -- Use the newest ebuilds for all the toolchain packages.
gcc_46 -- Use gcc-4.6 to build ChromeOS. Only works when latest_toolchain=True.
use_sdk -- Use cros_sdk to create and manage chroot.
"""

import constants


GS_PATH_DEFAULT = 'default' # Means gs://chromeos-archive/ + bot_id

MANIFEST_URL = 'http://git.chromium.org/chromiumos/manifest.git'
MANIFEST_INT_URL = constants.GERRIT_INT_SSH_URL + '/chromeos/manifest-internal'

# TODO(sosa): Move to manifest-versions-external once its created
_VERSIONS_SUFFIX = '/chromiumos/manifest-versions'
MANIFEST_VERSIONS_URL = constants.GERRIT_SSH_URL + _VERSIONS_SUFFIX

_VERSIONS_INT_SUFFIX = '/chromeos/manifest-versions'
MANIFEST_VERSIONS_INT_URL = constants.GERRIT_INT_SSH_URL + _VERSIONS_INT_SUFFIX


def _IsInternalBuild(git_url):
  """Returns whether the url is for the internal source.

  Args:
    git_url: The url of the manifest used to checkout the source
  """
  return git_url == MANIFEST_INT_URL


default = {
  'board' : None,
  'profile' : None,

  'master' : False,
  'important' : False,

  'useflags' : None,
  'chromeos_official' : False,
  'fast' : True,
  'usepkg_chroot' : True,
  'usepkg_setup_board' : True,
  'usepkg_build_packages' : True,
  'latest_toolchain' : False,
  'gcc_46' : False,

  'chroot_replace' : False,

  'uprev' : False,
  'overlays': 'public',
  'push_overlays': None,
  'chrome_rev' : None,

  'chrome_tests' : False,

  'unittests' : True,
  'quick_unit' : True,

  'build_tests' : True,
  'vm_tests' : True,
  'quick_vm' : True,

  'remote_ip' : None,

  'gs_path': GS_PATH_DEFAULT,

  'build_type': constants.PFQ_TYPE,
  'archive_build_debug' : False,

  'test_mod' : False,
  'factory_install_mod' : False,
  'factory_test_mod' : False,

  'push_image' : False,
  'upload_symbols' : False,

  'git_url' : MANIFEST_URL,
  'manifest_version' : False,
  'use_lkgm' : False,
  'prebuilts' : True,

  'use_sdk' : False,
}

arm = {
  # VM/tests are broken on arm.
  'build_tests' : False,
  'unittests' : False,
  'vm_tests' : False,
}

binary = {
  # Full builds that build fully from binaries.
  'chroot_replace' : False,
  'quick_unit' : False,

  'build_type': constants.BUILD_FROM_SOURCE_TYPE,
  'archive_build_debug' : True,
  'test_mod' : True,
  'factory_install_mod' : True,
  'factory_test_mod' : True,
}


full = {
  # Full builds are test build to show that we can build from scratch,
  # so use settings to build from scratch, and archive the results.

  'usepkg_chroot' : False,
  'usepkg_setup_board' : False,
  'usepkg_build_packages' : False,
  'chroot_replace' : True,

  'quick_unit' : False,

  'build_type': constants.BUILD_FROM_SOURCE_TYPE,
  'archive_build_debug' : True,
  'test_mod' : True,
  'factory_install_mod' : True,
  'factory_test_mod' : True,
}

internal = {
  'overlays' : 'both',
  'git_url' : MANIFEST_INT_URL,
  'quick_vm' : False,
  'gs_path' : None,
}

#
# Internal Builds
#

release = {
  # Typical matching cbuild command line
  # --autorev --official --officialversion --chromeos --with-pdf
  #   --bvt --clean --no-gstorage --ctest
  'build_tests' : True,
  'chrome_tests' : True,
  'manifest_version' : True,
  'prebuilts' : False,
  'push_image' : True,
  'upload_symbols' : True,
  # --official
  # --officialversion
}

official = {

  # Typical matching cbuild command line
  # master --official --chromeos --clean --upload-board-prebuilt
  #   --ctest --unittests --bvt

  'useflags' : ['chrome_internal', 'chrome_pdf'],
  'chromeos_official' : True,

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

add_config('chromiumos-sdk', [full, {
  'board' : ['x86-generic', 'arm-generic', 'amd64-host'],
  'build_type' : constants.CHROOT_BUILDER_TYPE,
}])

add_config('x86-generic-pre-flight-queue', [{
  'board' : 'x86-generic',
  'master' : True,
  'important': True,

  'uprev' : True,
  'overlays': 'public',
  'push_overlays': 'public',
  'manifest_version': True,
}])

add_config('x86-generic-commit-queue', [{
  'board' : 'x86-generic',
  'master' : True,
  'important': True,
  'build_type': constants.COMMIT_QUEUE_TYPE,

  'uprev' : True,
  'overlays': 'public',
  'push_overlays': 'public',
  'manifest_version': True,
}])

add_config('x86-generic-chrome-pre-flight-queue', [{
  'board' : 'x86-generic',
  'master' : True,

  'build_type': constants.CHROME_PFQ_TYPE,
  'important': True,
  'uprev' : False,
  'chrome_tests' : True,
  'overlays': 'public',
  'push_overlays': 'public',
  'manifest_version': True,

  'remote_ip' : '172.22.75.211',
}])

add_config('arm-generic-chrome-pre-flight-queue', [arm, {
  'board' : 'arm-generic',

  'build_type': constants.CHROME_PFQ_TYPE,
  'important': True,
  'uprev' : False,
  'chrome_tests' : True,
  'overlays': 'public',
  'push_overlays': None,
  'manifest_version': True,
}])

add_config('x86-pineview-bin', [{
  'board' : 'x86-pineview',

  'uprev' : True,
  'overlays': 'public',
  'push_overlays': None,
  'important': True,
  'manifest_version': True,
}])

add_config('arm-tegra2-bin', [arm, {
  'board' : 'tegra2_dev-board',

  'uprev' : True,
  'overlays': 'public',
  'push_overlays': None,
  'important': True,
  'manifest_version': True,
}])

add_config('arm-generic-bin', [arm, {
  'board' : 'arm-generic',

  'uprev' : True,
  'overlays': 'public',
  'push_overlays': None,
  'important': True,
  'manifest_version': True,
}])

add_config('arm-generic-full', [arm, full, {
  'board' : 'arm-generic',

  # TODO(zbehan): Remove this after sdk becomes the default. crosbug.com/17474
  'use_sdk' : True,
  'usepkg_chroot' : True,
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

add_config('x86-generic-gcc_46', [full, {
  'board' : 'x86-generic',
  'latest_toolchain' : True,
  'gcc_46' : True,
  'prebuilts' : False,
}])

add_config('arm-tegra2-seaboard-gcc_46', [arm, full, {
  'board' : 'tegra2_seaboard',
  'latest_toolchain' : True,
  'gcc_46' : True,
  'prebuilts' : False,
}])

add_config('x86-generic-toolchain', [full, {
  'board' : 'x86-generic',
  'latest_toolchain' : True,
  'prebuilts' : False,
}])

add_config('arm-tegra2-seaboard-toolchain', [arm, full, {
  'board' : 'tegra2_seaboard',
  'latest_toolchain' : True,
  'prebuilts' : False,
}])

#
# Internal Builds
#

add_config('x86-mario-pre-flight-queue', [internal, {
  'board' : 'x86-mario',
  'master' : True,

  'uprev' : True,
  'quick_vm' : True,
  'overlays': 'private',
  'push_overlays': 'private',
  'gs_path': 'gs://chromeos-x86-mario/pre-flight-master',
  'manifest_version' : True,
}])

add_config('x86-alex-pre-flight-branch', [internal, {
  'board' : 'x86-alex',
  'master' : False,

  'uprev' : True,
  'overlays': 'both',
  'push_overlays': None,
}])

add_config('x86-mario-pre-flight-branch', [internal, {
  'board' : 'x86-mario',
  'master' : True,

  'uprev' : True,
  'overlays': 'both',
  'push_overlays': 'both',
}])

add_config('arm-tegra2_seaboard-private-bin', [internal, arm, binary, {
  'board' : 'tegra2_seaboard',
  'prebuilts' : False,
  'usepkg_build_packages' : False,
}])

add_config('arm-tegra2_kaen-private-bin', [internal, arm, binary, {
  'board' : 'tegra2_kaen',
}])

add_config('arm-tegra2_aebl-private-bin', [internal, arm, binary, {
  'board' : 'tegra2_aebl',
}])

add_config('arm-tegra2_arthur-private-bin', [internal, arm, binary, {
  'board' : 'tegra2_arthur',
}])

add_config('arm-tegra2_dev-board-private-bin', [internal, arm, binary, {
  'board' : 'tegra2_dev-board',
  'prebuilts' : False,
  'usepkg_build_packages' : False,
}])

add_config('x86-zgb-private-bin', [internal, binary, {
  'board' : 'x86-zgb',
}])

add_config('x86-mario-private-bin', [internal, binary, {
  'board' : 'x86-mario',
}])

add_config('x86-alex-private-bin', [internal, binary, {
  'board' : 'x86-alex',
}])

add_config('x86-alex_nogobi-private-bin', [internal, binary, {
  'board' : 'x86-alex_nogobi',
}])

add_config('stumpy-private-bin', [internal, binary, {
  'board' : 'stumpy',
}])

add_config('x86-alex-toolchain', [internal, full, official, {
  'board' : 'x86-alex',
  'build_tests' : True,
  'chrome_tests' : True,
  'prebuilts' : False,
  'latest_toolchain' : True,
  'useflags' : ['chrome_internal'],
}])

add_config('arm-tegra2_seaboard-toolchain', [arm, internal, full, official, {
  'board' : 'tegra2_seaboard',
  'build_tests' : True,
  'chrome_tests' : True,
  'prebuilts' : False,
  'latest_toolchain' : True,
  'useflags' : ['chrome_internal'],
}])

add_config('arm-tegra2_seaboard-tangent-private-release',
  [internal, arm, full, {
    'board' : 'tegra2_seaboard',
    'profile' : 'cosine',
    'prebuilts' : False,
}])

add_config('arm-tegra2_seaboard-tangent-release',
  [config['arm-tegra2_seaboard-tangent-private-release']])

add_config('arm-tegra2_seaboard-tangent-private-bin',
  [internal, arm, binary, {
    'board' : 'tegra2_seaboard',
    'profile' : 'cosine',
    'prebuilts' : False,
}])

add_config('x86-mario-release', [internal, full, official, release, {
  'board' : 'x86-mario',
}])

add_config('x86-alex-release', [internal, full, official, release, {
  'board' : 'x86-alex',
}])

add_config('x86-alex_nogobi-release', [internal, full, official, release, {
  'board' : 'x86-alex_nogobi',
}])

add_config('x86-zgb-release', [internal, full, official, release, {
  'board' : 'x86-zgb',
}])

add_config('stumpy-release', [internal, full, official, release, {
  'board' : 'stumpy',
}])

add_config('arm-tegra2_seaboard-release', [
    arm, internal, full, official, release, {
  'board' : 'tegra2_seaboard',
}])

add_config('arm-tegra2_aebl-release', [
    arm, internal, full, official, release, {
  'board' : 'tegra2_aebl',
}])

add_config('arm-tegra2_arthur-release', [
    arm, internal, full, official, release, {
  'board' : 'tegra2_arthur',
}])

add_config('arm-tegra2_kaen-release', [
    arm, internal, full, official, release, {
  'board' : 'tegra2_kaen',
}])
