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

vm_tests -- Run vm test type defined in constants.

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

images -- List of images we want to build -- see build_image for more details.

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
import copy

GS_PATH_DEFAULT = 'default' # Means gs://chromeos-archive/ + bot_id


def IsInternalBuild(config):
  """Returns whether a build config is an internal config.

  Args:
    config: The build configuration dictionary to test.
  """
  return config['git_url'] == constants.MANIFEST_INT_URL


def OverrideConfigForTrybot(config):
  """Apply trybot-specific configuration settings.

  Args:
    config:  The build configuration dictionary to override.  The dictionary is
             not modified.

  Returns:
    A build configuration dictionary with the overrides applied.
  """
  copy_config = copy.deepcopy(config)
  copy_config['uprev'] = True
  if IsInternalBuild(config):
    copy_config['overlays'] = 'both'

  # Most users don't have access to the pdf repository so disable pdf.
  useflags = copy_config['useflags']
  if useflags and 'chrome_pdf' in useflags:
    useflags.remove('chrome_pdf')

  return copy_config


def _GetManifestVersionsRepoUrl(internal_build, read_only=False):

  if internal_build:
    if read_only:
      # This is not good .. we needlessly load the gerrit server.
      # TODO(petermayo):  Fix re: crosbug.com/20303
      return (constants.GERRIT_INT_SSH_URL +
              constants.MANIFEST_VERSIONS_INT_SUFFIX)
    else:
      return (constants.GERRIT_INT_SSH_URL +
              constants.MANIFEST_VERSIONS_INT_SUFFIX)
  else:
    if read_only:
      return (constants.GIT_HTTP_URL + constants.MANIFEST_VERSIONS_SUFFIX +
              constants.GIT_HTTP_SUFFIX)
    else:
      return constants.GERRIT_SSH_URL + constants.MANIFEST_VERSIONS_SUFFIX


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
  'vm_tests' : constants.SIMPLE_AU_TEST_TYPE,

  'hw_tests' : None,
  'hw_tests_reimage' : True,
  'remote_ip' : None,

  'gs_path': GS_PATH_DEFAULT,

  'build_type': constants.PFQ_TYPE,
  'archive_build_debug' : False,

  'images' : ['test'],
  'factory_install_netboot' : False,

  'push_image' : False,
  'upload_symbols' : False,

  'git_url' : constants.MANIFEST_URL,
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
  'vm_tests' : None,

  # The factory install image should be a netboot image on ARM.
  'factory_install_netboot' : True,
}

binary = {
  # Full builds that build fully from binaries.
  'chroot_replace' : False,
  'quick_unit' : False,

  'build_type': constants.BUILD_FROM_SOURCE_TYPE,
  'archive_build_debug' : True,
  'images': ['test', 'factory_test', 'factory_install'],
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
  'images': ['test', 'factory_test', 'factory_install'],
  'git_sync' : True,
}

pfq = {
  'important': True,
  'uprev' : True,
  'overlays': 'public',
  'manifest_version': True,
  'trybot_list' : True,
}

# Pre Flight Queue branch builders uprev and push on both overlays.
pfq_branch = pfq.copy()
pfq_branch.update({
  'overlays': 'both',
  'push_overlays': 'both',
})

commit_queue = {
  'important': True,
  'build_type': constants.COMMIT_QUEUE_TYPE,
  'uprev' : True,
  'overlays': 'public',
  'prebuilts': False,
  'manifest_version': True,
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
  'overlays': 'public',
  'chrome_tests' : True,
  'use_lkgm' : True,
  'vm_tests': constants.SMOKE_SUITE_TEST_TYPE,
}

internal = {
  'overlays' : 'both',
  'git_url' : constants.MANIFEST_INT_URL,
  'vm_tests' : constants.FULL_AU_TEST_TYPE,
}

#
# Internal Builds
#

release = {
  # Typical matching cbuild command line
  # --autorev --official --officialversion --chromeos --with-pdf
  #   --bvt --clean --no-gstorage --ctest
  'build_type' : constants.CANARY_TYPE,
  'build_tests' : True,
  'chrome_tests' : True,
  'manifest_version' : True,
  'images': ['base', 'test', 'factory_test', 'factory_install'],
  'prebuilts' : False,
  'push_image' : True,
  'upload_symbols' : True,
  'nowithdebug' : True,
  'overlays' : 'public',
  'prebuilts' : True,
  'binhost_bucket' : 'gs://chromeos-dev-installer',
  'binhost_key' : 'RELEASE_BINHOST',
  'binhost_base_url' :
    'https://commondatastorage.googleapis.com/chromeos-dev-installer',
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
  # The amd64-host has to be last as that is when the toolchains
  # are bundled up for inclusion in the sdk.
  'board' : ['x86-generic', 'arm-generic', 'amd64-generic', 'amd64-host'],
  'build_type' : constants.CHROOT_BUILDER_TYPE,

  'use_sdk' : False,
}])

add_config('refresh-packages', [default, {
  'board' : ['x86-generic', 'arm-generic'],
  'build_type' : constants.REFRESH_PACKAGES_TYPE,
}])

add_config('x86-generic-pre-flight-queue', [pfq, {
  'board' : 'x86-generic',
  'master' : True,
  'push_overlays' : 'public',
  'description' : 'x86-generic PFQ',
}])

add_config('arm-generic-bin', [arm, pfq, {
  'board' : 'arm-generic',
  'description' : 'arm-generic PFQ',
}])

add_config('arm-tegra2-bin', [arm, pfq, {
  'board' : 'tegra2',
  'description' : 'arm-tegra2 PFQ',
}])

add_config('x86-generic-commit-queue', [commit_queue, {
  'board' : 'x86-generic',
  'master' : True,
}])

add_config('arm-generic-commit-queue', [commit_queue, {
  'board' : 'arm-generic',
  'important': False,
}])

add_config('arm-tegra2-commit-queue', [commit_queue, {
  'board' : 'tegra2',
  'important': False,
}])

add_config('x86-mario-commit-queue', [commit_queue, internal, {
  'board' : 'x86-mario',
  'master': True,
  'overlays': 'private',
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

# arm- doesn't really matter, but cycles faster
add_config('patch-tot-chrome-pfq-informational', [chrome_pfq_info, arm, {
  'board' : 'arm-generic',
  'useflags' : ['touchui_patches'],
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
  'prebuilts' : True,
}])

add_config('amd64-corei7-full', [full, {
  'board' : 'amd64-corei7',
  'prebuilts' : True,
  'unittests' : False,
  'vm_tests' : None,
}])

add_config('x86-generic-asan', [{
  'board' : 'x86-generic',
  'profile' : 'asan',
  'prebuilts' : False,
  'useflags' : ['asan'],
}])

#
# Internal Builds
#

add_config('x86-mario-pre-flight-queue', [internal, pfq, {
  'board' : 'x86-mario',
  'master' : True,

  'vm_tests': constants.SIMPLE_AU_TEST_TYPE,
  'overlays': 'private',
  'push_overlays': 'private',
  'gs_path': 'gs://chromeos-x86-mario/pre-flight-master',

  'description' : 'internal x86 PFQ',
}])

add_config('x86-alex-pre-flight-branch', [internal, pfq_branch, {
  'board' : 'x86-alex',
}])

add_config('x86-mario-pre-flight-branch', [internal, pfq_branch, {
  'board' : 'x86-mario',
  'master' : True,
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
  'prebuilts': False,
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

add_config('lumpy-private-bin', [internal, binary, {
  'board' : 'lumpy',
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

add_config('arm-tegra2_seaboard-toolchain', [internal, arm, full, official, {
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
    'gs_path': None,
}])

add_config('arm-tegra2_seaboard-tangent-private-bin',
  [internal, arm, binary, {
    'board' : 'tegra2_seaboard',
    'profile' : 'cosine',
    'prebuilts' : False,
    'gs_path': None,
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

add_config('lumpy-release', [internal, full, official, release, {
  'board' : 'lumpy',
}])

add_config('arm-tegra2_seaboard-release', [
    internal, arm, full, official, release, {
  'board' : 'tegra2_seaboard',
}])

add_config('arm-tegra2_seaboard-tangent-release',
  [internal, arm, full, official, release, {
    'board' : 'tegra2_seaboard',
    'profile' : 'cosine',
    'prebuilts' : False,
}])

add_config('arm-tegra2_aebl-release', [
    internal, arm, full, official, release, {
  'board' : 'tegra2_aebl',
}])

add_config('arm-tegra2_arthur-release', [
    internal, arm, full, official, release, {
  'board' : 'tegra2_arthur',
}])

add_config('arm-tegra2_asymptote-release', [
    internal, arm, full, official, release, {
  'board' : 'tegra2_asymptote',
}])

add_config('arm-tegra2_kaen-release', [
    internal, arm, full, official, release, {
  'board' : 'tegra2_kaen',
}])

add_config('aura-tot-chrome-pfq-informational', [chrome_pfq_info, {
  'board' : 'x86-generic',
  'profile' : 'aura',
  'vm_tests' : False,
  'chrome_tests' : False
}])

add_config('stumpy-aura-private-bin', [internal, binary, {
  'board' : 'stumpy',
  'profile' : 'aura',
}])

add_config('stumpy-aura-release', [internal, full, official, release, {
  'board' : 'stumpy',
  'profile' : 'aura',
  'vm_tests' : False,
  'chrome_tests' : False
}])
