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
hw_tests -- A list of autotests and arguments to run on remote hardware.
            The format is [(test1, argument, argument,), (test2,), ...]
hw_tests_reimage -- Whether to reimage the remote device before running
                    hw_tests.

fast -- Use parallel_emerge for faster (but slightly more risky) builds.
use_sdk -- Use SDK as opposed to building the chroot from source.
usepkg_setup_board -- Use binary packages for setup_board. (emerge --usepkg)
usepkg_build_packages -- Use binary packages for build_packages.
nowithdebug -- Pass the --nowithdebug flag to build_packages (sets the sets
               -DNDEBUG compiler flag).

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
trybot_list -- List this config when user runs cbuildbot with --list option
               without the --all flag.
description -- The description string to print out for config when user runs
               --list.

binhost_bucket -- Upload prebuilts for this build to this bucket. If it equals
                  None the default buckets are used.
binhost_key -- Parameter --key for prebuilt.py. If it equals None the default
               values are used, which depend on the build type.
binhost_base_url -- Parameter --binhost-base-url for prebuilt.py. If it equals
                    None default value is used.
use_binhost_package_file -- Flag that is used to decide whether to use the
                            file with the packages to upload to the binhost.
git_sync -- Boolean that enables parameter --git-sync for prebuilt.py.
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
  'usepkg_setup_board' : True,
  'usepkg_build_packages' : True,
  'nowithdebug' : False,
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

  'hw_tests' : None,
  'hw_tests_reimage' : True,
  'remote_ip' : None,

  'gs_path': GS_PATH_DEFAULT,

  'build_type': constants.PFQ_TYPE,
  'archive_build_debug' : False,

  'test_mod' : False,
  'factory_install_mod' : False,
  'factory_test_mod' : False,
  'factory_install_netboot' : False,

  'push_image' : False,
  'upload_symbols' : False,

  'git_url' : MANIFEST_URL,
  'manifest_version' : False,
  'use_lkgm' : False,
  'prebuilts' : True,

  'use_sdk' : True,

  'trybot_list' : False,
  'description' : None,

  'binhost_bucket' : None,
  'binhost_key' : None,
  'binhost_base_url' : None,
  'use_binhost_package_file' : False,
  'git_sync' : False,
}

arm = {
  # VM/tests are broken on arm.
  'build_tests' : False,
  'unittests' : False,
  'vm_tests' : False,

  # The factory install image should be a netboot image on ARM.
  'factory_install_netboot' : True,
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
  'git_sync' : True,
}


full = {
  # Full builds are test build to show that we can build from scratch,
  # so use settings to build from scratch, and archive the results.

  'usepkg_setup_board' : False,
  'usepkg_build_packages' : False,
  'chroot_replace' : True,

  'quick_unit' : False,

  'build_type': constants.BUILD_FROM_SOURCE_TYPE,
  'archive_build_debug' : True,
  'test_mod' : True,
  'factory_install_mod' : True,
  'factory_test_mod' : True,
  'git_sync' : True,
}

chrome_pfq = {
  'build_type': constants.CHROME_PFQ_TYPE,
  'important': True,
  'uprev' : False,
  'chrome_tests' : True,
  'overlays': 'public',
  'manifest_version': True,
}

chrome_pfq_info = {
  'build_type': constants.CHROME_PFQ_TYPE,
  'chrome_rev': constants.CHROME_REV_TOT,
  'important': False,   # for the moment
  'uprev' : False,
  'overlays': 'public',
  'manifest_version': True,
  'master' : False,
  'chrome_tests' : True,
  'build_tests' : True,
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
  'nowithdebug' : True,
  'overlays' : 'public',
  'prebuilts' : True,
  'binhost_bucket' : 'gs://chromeos-dev-installer',
  'binhost_key' : 'RELEASE_BINHOST',
  'binhost_base_url' : 'https://commondatastorage.googleapis.com/chromeos-dev-installer',
  'use_binhost_package_file' : True,
  'git_sync' : False,
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

  'use_sdk' : False,
}])

add_config('refresh-packages', [default, {
  'board' : ['x86-generic', 'arm-generic'],
  'build_type' : constants.REFRESH_PACKAGES_TYPE,
}])

add_config('x86-generic-pre-flight-queue', [{
  'board' : 'x86-generic',
  'master' : True,
  'important': True,

  'uprev' : True,
  'overlays': 'public',
  'push_overlays': 'public',
  'manifest_version': True,

  'trybot_list' : True,
  'description' : 'x86-generic PFQ',
}])

add_config('x86-generic-commit-queue', [{
  'board' : 'x86-generic',
  'master' : True,
  'important': True,
  'build_type': constants.COMMIT_QUEUE_TYPE,

  'uprev' : True,
  'overlays': 'public',
  'push_overlays': None,
  'prebuilts': False,
  'manifest_version': True,
}])

add_config('arm-generic-commit-queue', [{
  'board' : 'arm-generic',
  'important': False,
  'build_type': constants.COMMIT_QUEUE_TYPE,

  'uprev' : True,
  'overlays': 'public',
  'prebuilts': False,
  'manifest_version': True,
}])

add_config('x86-generic-tot-chrome-pre-flight-queue', [chrome_pfq, {
  'board' : 'x86-generic',
  'master' : True,
  'push_overlays': 'public',
  'hw_tests' : [('desktopui_PyAutoFunctionalTests', 'CONTINUOUS_PUBLIC')],
  'chrome_rev': constants.CHROME_REV_TOT,
  'remote_ip' : '172.22.75.211',
}])

add_config('arm-generic-tot-chrome-pre-flight-queue', [chrome_pfq, {
  'board' : 'arm-generic',
  'chrome_rev': constants.CHROME_REV_TOT,
}])

add_config('x86-generic-chrome-pre-flight-queue', [chrome_pfq, {
  'board' : 'x86-generic',
  'master' : True,
  'push_overlays': 'public',
  'chrome_rev': constants.CHROME_REV_LATEST,
}])

add_config('arm-generic-chrome-pre-flight-queue', [chrome_pfq, arm, {
  'board' : 'arm-generic',
  'chrome_rev': constants.CHROME_REV_LATEST,
}])

add_config('arm-tegra2-chrome-pre-flight-queue', [chrome_pfq, arm, {
  'board' : 'tegra2',
  'chrome_rev': constants.CHROME_REV_LATEST,
}])

add_config('x86-generic-tot-chrome-pfq-informational', [chrome_pfq_info, {
  'board' : 'x86-generic',
}])

add_config('arm-generic-tot-chrome-pfq-informational', [chrome_pfq_info, arm, {
  'board' : 'arm-generic',
}])

add_config('arm-tegra2-tot-chrome-pfq-informational', [chrome_pfq_info, arm, {
  'board' : 'tegra2',
}])


# TODO(sosa): Remove x86-pineview bin.
add_config('x86-pineview-bin', [{
  'board' : 'x86-pineview',

  'uprev' : True,
  'overlays': 'public',
  'push_overlays': None,
  'important': False,
  'manifest_version': True,
}])

add_config('arm-generic-bin', [arm, {
  'board' : 'arm-generic',

  'uprev' : True,
  'overlays': 'public',
  'push_overlays': None,
  'important': True,
  'manifest_version': True,

  'trybot_list' : True,
  'description' : 'arm-generic PFQ',
}])

add_config('arm-tegra2-bin', [arm, {
  'board' : 'tegra2',

  'uprev' : True,
  'overlays': 'public',
  'push_overlays': None,
  'important': True,
  'manifest_version': True,

  'trybot_list' : True,
  'description' : 'arm-tegra2 PFQ',
}])

add_config('arm-generic-full', [arm, full, {
  'board' : 'arm-generic',
}])

add_config('arm-tegra2-full', [arm, full, {
  'board' : 'tegra2',
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

add_config('amd64-generic-full', [full, {
  'board' : 'amd64-generic',
  'prebuilts' : False,
}])

#
# Internal Builds
#

add_config('x86-mario-pre-flight-queue', [internal, {
  'board' : 'x86-mario',
  'master' : True,
  'important': True,

  'uprev' : True,
  'quick_vm' : True,
  'overlays': 'private',
  'push_overlays': 'private',
  'gs_path': 'gs://chromeos-x86-mario/pre-flight-master',
  'manifest_version' : True,

  'trybot_list' : True,
  'description' : 'internal x86 PFQ',
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

add_config('arm-tegra2_asymptote-private-bin', [internal, arm, binary, {
  'board' : 'tegra2_asymptote',
}])

add_config('arm-tegra2_dev-board-private-bin', [internal, arm, binary, {
  'board' : 'tegra2_dev-board',
  'prebuilts' : False,
  'usepkg_build_packages' : False,
}])

add_config('x86-zgb-private-bin', [internal, binary, {
  'board' : 'x86-zgb',
}])

add_config('x86-zgb_he-private-bin', [internal, binary, {
  'board' : 'x86-zgb_he',
}])

add_config('x86-mario-private-bin', [internal, binary, {
  'board' : 'x86-mario',
}])

add_config('x86-alex-private-bin', [internal, binary, {
  'board' : 'x86-alex',
}])

add_config('x86-alex_he-private-bin', [internal, binary, {
  'board' : 'x86-alex_he',
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
  'hw_tests' : [('platform_GCC',)],
  'hw_tests_reimage' : False,
  'remote_ip' : '172.18.221.163',
  'use_lkgm' : True,
  'gs_path': GS_PATH_DEFAULT,
}])

add_config('arm-tegra2_seaboard-toolchain', [arm, internal, full, official, {
  'board' : 'tegra2_seaboard',
  'build_tests' : True,
  'chrome_tests' : True,
  'prebuilts' : False,
  'latest_toolchain' : True,
  'useflags' : ['chrome_internal'],
  'use_lkgm' : True,
  'gs_path': GS_PATH_DEFAULT,
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

add_config('x86-alex_he-release', [internal, full, official, release, {
  'board' : 'x86-alex_he',
}])

add_config('x86-zgb-release', [internal, full, official, release, {
  'board' : 'x86-zgb',
}])

add_config('x86-zgb_he-release', [internal, full, official, release, {
  'board' : 'x86-zgb_he',
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

add_config('arm-tegra2_asymptote-release', [
    arm, internal, full, official, release, {
  'board' : 'tegra2_asymptote',
}])

add_config('arm-tegra2_kaen-release', [
    arm, internal, full, official, release, {
  'board' : 'tegra2_kaen',
}])
