#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# Disable relative import warning from pylint.
# pylint: disable=W0403
import constants
import copy
import itertools
import json
import optparse
import sys
import urllib

GS_PATH_DEFAULT = 'default' # Means gs://chromeos-archive/ + bot_id

# Contains the valid build types in the order that they are dumped.
BUILD_TYPE_DUMP_ORDER = [
    constants.PALADIN_TYPE,
    constants.INCREMENTAL_TYPE,
    constants.BUILD_FROM_SOURCE_TYPE,
    constants.CANARY_TYPE,
    constants.CHROOT_BUILDER_TYPE,
    constants.CHROOT_BUILDER_BOARD,
    constants.CHROME_PFQ_TYPE,
    constants.PFQ_TYPE,
    constants.REFRESH_PACKAGES_TYPE]


def OverrideConfigForTrybot(build_config, remote_trybot):
  """Apply trybot-specific configuration settings.

  Args:
    build_config:  The build configuration dictionary to override.
      The dictionary is not modified.
    remote_trybot: Whether we are overriding as remote trybot.
  Returns:
    A build configuration dictionary with the overrides applied.
  """
  copy_config = copy.deepcopy(build_config)
  board_specific_configs = copy_config['board_specific_configs'].values()
  for my_config in [copy_config] + board_specific_configs:
    my_config['uprev'] = True
    if my_config['internal']:
      my_config['overlays'] = constants.BOTH_OVERLAYS

    # Most users don't have access to the pdf repository so disable pdf.
    useflags = my_config['useflags']
    if useflags and 'chrome_pdf' in useflags:
      useflags.remove('chrome_pdf')

    my_config['upload_symbols'] = False
    my_config['push_image'] = False

    # Default to starting with a fresh chroot on remote trybot runs.
    if remote_trybot:
      my_config['chroot_replace'] = True

  return copy_config


def GetManifestVersionsRepoUrl(internal_build, read_only=False, test=False):
  """Returns the url to the manifest versions repository.

  Args:
    internal_build: Whether to use the internal repo.
    read_only: Whether the URL may be read only.  If read_only is True,
      pushing changes (even with dryrun option) may not work.
    test: Whether we should use the corresponding test repositories. These
      should be used when staging experimental features.
  """
  if internal_build:
    url = (constants.GERRIT_INT_SSH_URL +
            constants.MANIFEST_VERSIONS_INT_SUFFIX)
  elif read_only:
    url = constants.GIT_HTTP_URL + constants.MANIFEST_VERSIONS_SUFFIX
  else:
    url = constants.GERRIT_SSH_URL + constants.MANIFEST_VERSIONS_SUFFIX

  if test:
    url += '-test'

  return url


def IsPFQType(b_type):
  """Returns True if this build type is a PFQ."""
  return b_type in (constants.PFQ_TYPE, constants.PALADIN_TYPE,
                    constants.CHROME_PFQ_TYPE)


def IsCQType(b_type):
  """Returns True if this build type is a Commit Queue."""
  return b_type in (constants.COMMIT_QUEUE_TYPE, constants.PALADIN_TYPE)


# List of usable cbuildbot configs; see add_config method.
config = {}


# Enumeration of valid settings; any/all config settings must be in this.
# All settings must be documented.

_settings = dict(

# name -- The name of the config.
  name=None,

# boards -- A list of boards to build.
  boards=None,

# paladin_builder_name -- Used by paladin logic. The name of the builder on the
#                         buildbot waterfall if it differs from the config name.
#                         If None is used, defaults to config name.
  paladin_builder_name=None,

# profile -- The profile of the variant to set up and build.
  profile=None,

# master -- This bot pushes changes to the overlays.
  master=False,

# important -- Master bot uses important bots to determine overall status.
#              i.e. if master bot succeeds and other important slaves succeed
#              then the master will uprev packages.  This should align
#              with info vs. closer except for the master and options.tests.
  important=False,

# internal -- Whether this is an internal build config.
  internal=False,

# branch -- Whether this is a branched build config. Used for pfq logic.
  branch=False,

# useflags -- emerge use flags to use while setting up the board, building
#             packages, making images, etc.
  useflags=None,

# chromeos_official -- Set the variable CHROMEOS_OFFICIAL for the build.
#                      Known to affect parallel_emerge, cros_set_lsb_release,
#                      and chromeos_version.sh. See bug chromium-os:14649
  chromeos_official=False,

# usepkg_setup_board -- Use binary packages for setup_board. (emerge --usepkg)
  usepkg_setup_board=True,

# usepkg_build_packages -- Use binary packages for build_packages.
  usepkg_build_packages=True,

# nowithdebug -- Pass the --nowithdebug flag to build_packages (sets the
#                -DNDEBUG compiler flag).
  nowithdebug=False,

# latest_toolchain -- Use the newest ebuilds for all the toolchain packages.
  latest_toolchain=False,

# gcc_githash -- This is only valid when latest_toolchain is True.
# If you set this to a commit-ish, the gcc ebuild will use it to build the
# toolchain compiler.
  gcc_githash=None,

# chroot_replace -- wipe and replace chroot, but not source.
  chroot_replace=False,

# uprev -- Uprevs the local ebuilds to build new changes since last stable.
#          build.  If master then also pushes these changes on success.
  uprev=True,

# overlays -- Select what overlays to look at for revving and prebuilts. This
#             can be any constants.VALID_OVERLAYS.
  overlays=constants.PUBLIC_OVERLAYS,

# push_overlays -- Select what overlays to push at. This should be a subset of
#                  overlays for the particular builder.  Must be None if
#                  not a master.  There should only be one master bot pushing
#                  changes to each overlay per branch.
  push_overlays=None,

# chrome_rev -- Uprev Chrome, values of 'tot', 'stable_release', or None.
  chrome_rev=None,

# chrome_tests -- Runs chrome testing binaries in a vm.
  chrome_tests=False,

# unittests -- Runs unittests for packages.
  unittests=True,

# quick_unit -- If unittests is true, only run the unit tests for packages which
#               have changed since the previous build.
  quick_unit=True,

# build_tests -- Builds autotest tests.  Must be True if vm_tests is set.
  build_tests=True,

# vm_tests -- Run vm test type defined in constants.
  vm_tests=constants.SIMPLE_AU_TEST_TYPE,

# hw_tests -- A list of autotest suites to run on remote hardware.
  hw_tests=[],

# upload_hw_test_artifacts -- If true, uploads artifacts for hw testing.
  upload_hw_test_artifacts=False,

# gs_path -- Google Storage path to offload files to.
#            None - No upload
#            GS_PATH_DEFAULT - 'gs://chromeos-archive/' + bot_id
#            value - Upload to explicit path
  gs_path=GS_PATH_DEFAULT,

# TODO(sosa): Deprecate binary.
# build_type -- Type of builder.  Check constants.BUILD_TYPE_DUMP_ORDER.
  build_type=constants.PFQ_TYPE,

  archive_build_debug=False,

# images -- List of images we want to build -- see build_image for more details.
  images=['test'],
  factory_install_netboot=False,


# push_image -- Do we push a final release image to chromeos-images.
  push_image=False,

# upload_symbols -- Do we upload debug symbols.
  upload_symbols=False,

# git_url -- git repository URL for our manifests.
#            External: http://git.chromium.org/git/chromiumos/manifest
#            Internal:
#                ssh://gerrit-int.chromium.org:29419/chromeos/manifest-internal
  git_url=constants.MANIFEST_URL,

# manifest_version -- Whether we are using the manifest_version repo that stores
#                     per-build manifests.
  manifest_version=False,

# TODO(sosa): Merge with overlays == both once unified waterfall launched.
# unified_manifest_version -- If True, publish manifests to both manifest
# version repositories.
  unified_manifest_version=False,

# use_lkgm -- Use the Last Known Good Manifest blessed by the pre-flight-queue
  use_lkgm=False,

# prebuilts -- Upload prebuilts for this build.
  prebuilts=True,

# use_sdk -- Use SDK as opposed to building the chroot from source.
  use_sdk=True,

# trybot_list -- List this config when user runs cbuildbot with --list option
#                without the --all flag.
  trybot_list=False,

# description -- The description string to print out for config when user runs
#                --list.
  description=None,

# binhost_bucket -- Upload prebuilts for this build to this bucket. If it equals
#                   None the default buckets are used.
  binhost_bucket=None,

# binhost_key -- Parameter --key for prebuilt.py. If it equals None the default
#                values are used, which depend on the build type.
  binhost_key=None,

# binhost_base_url -- Parameter --binhost-base-url for prebuilt.py. If it equals
#                     None default value is used.
  binhost_base_url=None,

# Upload dev installer prebuilts.
  dev_installer_prebuilts=False,

# git_sync -- Boolean that enables parameter --git-sync for prebuilt.py.
  git_sync=False,

# board_specific_configs -- A mapping from boards in the boards array to
#                           config objects. Only used for config groups (see
#                           the add_group method).
  board_specific_configs={},

# grouped -- Whether this config belongs to a config group.
  grouped=False,
)


class _config(dict):
  """Dictionary of explicit configuration settings for a cbuildbot config

  Each dictionary entry is in turn a dictionary of config_param->value.

  See _settings for details on known configurations, and their documentation.
  """

  _URLQUOTED_PARAMS = ('paladin_builder_name',)

  def derive(self, *inherits, **overrides):
    """Create a new config derived from this one.

    Args:
      inherits: Mapping instances to mixin.
      overrides: Settings to inject; see _settings for valid values.
    Returns:
      A new _config instance.
    """
    new_config = copy.deepcopy(self)
    for update_config in inherits:
      new_config.update(update_config)

    new_config.update(overrides)

    return copy.deepcopy(new_config)

  def add_config(self, name, *inherits, **overrides):
    """Derive and add the config to cbuildbots usable config targets

    Args:
      name: The name to label this configuration; this is what cbuildbot
            would see.
      inherits: See the docstring of derive.
      overrides: See the docstring of derive.
    Returns:
      See the docstring of derive.
    """
    overrides['name'] = name
    new_config = self.derive(*inherits, **overrides)

    # Derive directly from defaults so missing values are added.
    # Store a dictionary, rather than our derivative- this is
    # to ensure any far flung consumers of the config dictionary
    # aren't affected by recent refactorings.

    config_dict = _default.derive(self, *inherits, **overrides)
    config_dict.update((key, urllib.quote(config_dict[key]))
      for key in self._URLQUOTED_PARAMS if config_dict.get(key))

    config[name] = config_dict

    return new_config

  @classmethod
  def add_raw_config(cls, name, *inherits, **overrides):
    return cls().add_config(name, *inherits, **overrides)

  @classmethod
  def add_group(cls, name, *configs):
    """Create a new group of build configurations.

    Args:
      name: The name to label this configuration; this is what cbuildbot
            would see.
      configs: Configurations to build in this group. The first config in
               the group is considered the primary configuration and is used
               for syncing and creating the chroot.
    """
    board_specific_configs = {}
    for x in configs:
      for board in x['boards']:
        board_specific_configs[board] = _default.derive(x, grouped=True)
    return configs[0].add_config(name,
      boards=list(itertools.chain.from_iterable(x['boards'] for x in configs)),
      board_specific_configs=board_specific_configs
    )

_default = _config(**_settings)


# It is only safe to inherit prebuilts from generic boards, or from the
# same board without the variant. This rule helps keep inheritance trees
# sane.
generic_boards = set([
  'x86-generic', 'amd64-generic', 'tegra2'
])


# Arch-specific mixins.

arm = _config(
  # VM/tests are broken on arm.
  unittests=False,
  vm_tests=None,

  # The factory install image should be a netboot image on ARM.
  factory_install_netboot=True,
)

amd64 = _config()


# Builder-specific mixins

binary = _config(
  # Full builds that build fully from binaries.
  quick_unit=False,

  build_type=constants.BUILD_FROM_SOURCE_TYPE,
  archive_build_debug=True,
  images=['test', 'factory_test', 'factory_install'],
  git_sync=True,
)

full = _config(
  # Full builds are test builds to show that we can build from scratch,
  # so use settings to build from scratch, and archive the results.

  usepkg_setup_board=False,
  usepkg_build_packages=False,
  chroot_replace=True,

  quick_unit=False,

  build_type=constants.BUILD_FROM_SOURCE_TYPE,
  archive_build_debug=True,
  images=['base', 'test', 'factory_test', 'factory_install'],
  git_sync=True,
  trybot_list=True,
  description='Full Builds',
)

pfq = _config(
  build_type=constants.PFQ_TYPE,
  important=True,
  uprev=True,
  overlays=constants.PUBLIC_OVERLAYS,
  manifest_version=True,
  trybot_list=True,
)

paladin = _config(
  important=True,
  build_type=constants.PALADIN_TYPE,
  overlays=constants.PUBLIC_OVERLAYS,
  prebuilts=True,
  manifest_version=True,
  trybot_list=True,
  description='Commit Queue',
)

incremental = _config(
  build_type=constants.INCREMENTAL_TYPE,
  uprev=False,
  overlays=constants.PUBLIC_OVERLAYS,
  prebuilts=False,
)

# This builds with more source available.
internal = _config(
  internal=True,
  overlays=constants.BOTH_OVERLAYS,
  git_url=constants.MANIFEST_INT_URL,
)

# This adds Chrome branding.
official = _config(
  useflags=['chrome_internal', 'chrome_pdf'],
  chromeos_official=True,
)

SDK_TEST_BOARDS = ['amd64-generic', 'tegra2', 'x86-generic']

full.add_config('chromiumos-sdk',
  # The amd64-host has to be last as that is when the toolchains
  # are bundled up for inclusion in the sdk.
  boards=['x86-generic', 'arm-generic', 'amd64-generic', 'amd64-host'],
  build_type=constants.CHROOT_BUILDER_TYPE,
  use_sdk=False,
  trybot_list=True,
  description='Build the SDK and all the cross-compilers',
)

_config.add_raw_config('refresh-packages',
  boards=['x86-generic', 'arm-generic'],
  build_type=constants.REFRESH_PACKAGES_TYPE,
)

incremental.add_config('x86-generic-incremental',
  boards=['x86-generic'],
)

incremental.add_config('arm-tegra2-incremental',
  arm,
  boards=['tegra2'],
)

incremental.add_config('amd64-generic-incremental',
  amd64,
  boards=['amd64-generic'],
  # This builder runs on a VM, so it can't run VM tests.
  vm_tests=None,
)

paladin.add_config('x86-generic-paladin',
  boards=['x86-generic'],
  master=True,
  paladin_builder_name='x86 generic paladin',
  push_overlays=constants.PUBLIC_OVERLAYS,
)

paladin.add_config('arm-tegra2-paladin',
  arm,
  boards=['tegra2'],
  paladin_builder_name='tegra2 paladin',
)

paladin.add_config('amd64-generic-paladin',
  amd64,
  boards=['amd64-generic'],
  paladin_builder_name='amd64 generic paladin',
)

chromium_pfq = _config(
  build_type=constants.CHROME_PFQ_TYPE,
  important=True,
  chrome_tests=True,
  uprev=False,
  overlays=constants.PUBLIC_OVERLAYS,
  manifest_version=True,
  chrome_rev=constants.CHROME_REV_LATEST,
)

# TODO(davidjames): Convert this to an external config once the unified master
# logic is ready.
internal_chromium_pfq = internal.derive(chromium_pfq)

internal_chromium_pfq.add_config('x86-generic-chromium-pfq',
  boards=['x86-generic'],
  master=True,
  push_overlays=constants.PUBLIC_OVERLAYS,
  chrome_tests=False, # TODO(build-team): Use chrome tests
)

internal_chromium_pfq.add_config('tegra2-chromium-pfq',
  arm,
  boards=['tegra2'],
)

internal_chromium_pfq.add_config('daisy-chromium-pfq',
  arm,
  boards=['daisy'],
  important=False
)

internal_chromium_pfq.add_config('amd64-generic-chromium-pfq',
  amd64,
  boards=['amd64-generic'],
  # This builder runs on a VM, so it can't run VM tests.
  vm_tests=None,
)

chrome_pfq = internal_chromium_pfq.derive(
  official,
  important=False, # for now...
  chrome_tests=False, # TODO(build-team): Use chrome tests
  overlays=constants.BOTH_OVERLAYS,
  prebuilts=False,
  useflags=official['useflags'],
)

chrome_pfq.add_config('alex-chrome-pfq',
  boards=['x86-alex'],
)

chrome_pfq.add_config('alex32-chrome-pfq',
  boards=['x86-alex32'],
)

chrome_pfq.add_config('lumpy-chrome-pfq',
  boards=['lumpy'],
  # lumpy changes optimization relevant flags from x86-generic, so we should
  # ignore target pkgs built on parent/other boards.  Setup_board packages
  # are OK, since we aren't profiling them.
  usepkg_build_packages=False,
)

chromium_pfq_info = chromium_pfq.derive(
  chrome_rev=constants.CHROME_REV_TOT,
  use_lkgm=True,
  important=False,
  manifest_version=False,
  vm_tests=constants.SMOKE_SUITE_TEST_TYPE,
)

# TODO(petermayo): We may want to update the -chrome-pfq-i... as above.
chromium_pfq_info.add_config('x86-generic-tot-chrome-pfq-informational',
  boards=['x86-generic'],
  chrome_tests=False,
)

cpfq_arm = \
chromium_pfq_info.add_config('arm-generic-tot-chrome-pfq-informational',
  arm,
  boards=['arm-generic'],
)

cpfq_arm.add_config('arm-tegra2-tot-chrome-pfq-informational',
  boards=['tegra2'],
)

cpfq_arm.add_config('daisy-tot-chrome-pfq-informational',
  boards=['daisy'],
)

chromium_pfq_info.add_config('amd64-corei7-tot-chrome-pfq-informational',
  amd64,
  boards=['amd64-corei7'],
)

chromium_pfq_info.add_config('amd64-generic-tot-chrome-pfq-informational',
  amd64,
  boards=['amd64-generic'],
)

arm_generic_full = \
full.add_config('arm-generic-full', arm,
  boards=['arm-generic'],
)

arm_generic_full.add_config('arm-tegra2-full',
  boards=['tegra2'],
)

arm_generic_full.add_config('arm-tegra2-seaboard-full',
  boards=['tegra2_seaboard'],
)

x86_generic_full = \
full.add_config('x86-generic-full',
  boards=['x86-generic'],
  upload_hw_test_artifacts=True,
)

x86_generic_full.add_config('x86-pineview-full',
  boards=['x86-pineview'],
)

_toolchain = \
    full.derive(latest_toolchain=True, prebuilts=False,
                gcc_githash='gcc.gnu.org/branches/google/main',
                trybot_list=False,
                description='Toolchain',)

_toolchain.add_config('x86-generic-toolchain',
  boards=['x86-generic'],
)

_toolchain.add_config('arm-tegra2-seaboard-toolchain', arm,
  boards=['tegra2_seaboard'],
)

_toolchain_minor = \
    full.derive(latest_toolchain=True, prebuilts=False,
                gcc_githash='gcc.gnu.org/branches/google/gcc-4_6',
                trybot_list=False,
                description='Toolchain',)

_toolchain_minor.add_config('x86-generic-toolchain_minor',
  boards=['x86-generic'],
)

_toolchain_minor.add_config('arm-tegra2-seaboard-toolchain_minor', arm,
  boards=['tegra2_seaboard'],
)


full.add_config('amd64-generic-full',
  boards=['amd64-generic'],
)

_config.add_raw_config('x86-generic-asan',
  boards=['x86-generic'],
  profile='asan',
  prebuilts=False,
  useflags=['asan'],
  build_type=constants.BUILD_FROM_SOURCE_TYPE,
)

#
# Internal Builds
#

internal_pfq = internal.derive(pfq, overlays=constants.PRIVATE_OVERLAYS)
internal_pfq_branch = internal_pfq.derive(overlays=constants.BOTH_OVERLAYS,
                                          trybot_list=False, branch=True)
internal_paladin = internal.derive(paladin, overlays=constants.PRIVATE_OVERLAYS)
internal_incremental = internal.derive(incremental,
                                       overlays=constants.BOTH_OVERLAYS)

internal_pfq_branch.add_config('x86-alex-pre-flight-branch',
  master=True,
  push_overlays=constants.BOTH_OVERLAYS,
  boards=['x86-alex'],
)

internal_arm_paladin = internal_paladin.derive(arm)

internal_paladin.add_config('mario-paladin',
  master=True,
  push_overlays=constants.PRIVATE_OVERLAYS,
  boards=['x86-mario'],
  gs_path='gs://chromeos-x86-mario/pre-flight-master',
  paladin_builder_name='mario paladin',
)

internal_arm_paladin.add_config('daisy-paladin',
  boards=['daisy'],
  important=True,
  paladin_builder_name='daisy paladin',
)

internal_arm_paladin.add_config('kaen-paladin',
  boards=['tegra2_kaen'],
  paladin_builder_name='tegra2 kaen paladin',
)

internal_arm_paladin.add_config('ironhide-paladin',
  boards=['ironhide'],
  important=False,
  paladin_builder_name='ironhide paladin',
)

internal_paladin.add_config('zgb-paladin',
  boards=['x86-zgb'],
  important=False,
  paladin_builder_name='zgb paladin',
)

internal_paladin.add_config('alex-paladin',
  boards=['x86-alex'],
  paladin_builder_name='alex paladin',
)

internal_paladin.add_config('stumpy-paladin',
  boards=['stumpy'],
  paladin_builder_name='stumpy paladin',
)

internal_paladin.add_config('lumpy-paladin',
  boards=['lumpy'],
  paladin_builder_name='lumpy paladin',
)

internal_paladin.add_config('link-paladin',
  boards=['link'],
  paladin_builder_name='link paladin',
)

internal_paladin.add_config('kiev-paladin',
  boards=['kiev'],
  important=False,
  paladin_builder_name='kiev paladin',
)

# TODO(sosa): Test configs for unified paladin builds.
internal_paladin.add_config('unified-mario-paladin',
  master=True,
  push_overlays=constants.BOTH_OVERLAYS,
  overlays=constants.BOTH_OVERLAYS,
  boards=['x86-mario'],
  unified_manifest_version=True,
  paladin_builder_name='unified mario paladin',
  vm_tests=None,
  prebuilts=None,
)

paladin.add_config('unified-x86-generic-paladin',
  boards=['x86-generic'],
  unified_manifest_version=True,
  paladin_builder_name='unified x86-generic paladin',
  vm_tests=None,
  prebuilts=None,
)

internal_incremental.add_config('mario-incremental',
  boards=['x86-mario'],
)

_internal_toolchain = _toolchain.derive(internal, official,
  use_lkgm=True,
  useflags=['chrome_internal'],
  build_tests=True,
)

_internal_toolchain.add_config('x86-alex-toolchain',
  boards=['x86-alex'],
)

_internal_toolchain.add_config('arm-tegra2_kaen-toolchain',
  arm,
  boards=['tegra2_kaen'],
)

_internal_toolchain_minor = _toolchain_minor.derive(internal, official,
  use_lkgm=True,
  useflags=['chrome_internal'],
  build_tests=True,
)

_internal_toolchain_minor.add_config('x86-alex-toolchain_minor',
  boards=['x86-alex'],
)

_internal_toolchain_minor.add_config('arm-tegra2_kaen-toolchain_minor',
  arm,
  boards=['tegra2_kaen'],
)

_release = full.derive(official, internal,
  build_type=constants.CANARY_TYPE,
  build_tests=True,
  manifest_version=True,
  images=['base', 'test', 'factory_test', 'factory_install'],
  push_image=True,
  upload_symbols=True,
  nowithdebug=True,
  binhost_bucket='gs://chromeos-dev-installer',
  binhost_key='RELEASE_BINHOST',
  binhost_base_url=
    'https://commondatastorage.googleapis.com/chromeos-dev-installer',
  dev_installer_prebuilts=True,
  git_sync=False,
  vm_tests=constants.FULL_AU_TEST_TYPE,
  upload_hw_test_artifacts=True,
  trybot_list=True,
  description="Release Builds",
)

_release.add_config('x86-mario-release',
  boards=['x86-mario'],
  hw_tests=['bvt'],
)

_config.add_group('x86-alex-release-group',
  _release.add_config('x86-alex-release',
    boards=['x86-alex'],
    hw_tests=['bvt'],
  ),
  _release.add_config('x86-alex_he-release',
    boards=['x86-alex_he'],
    vm_tests=None,
    unittests=None,
    upload_hw_test_artifacts=False,
  ),
)

_config.add_group('x86-alex32-release-group',
  _release.add_config('x86-alex32-release',
    boards=['x86-alex32'],
  ),
  _release.add_config('x86-alex32_he-release',
    boards=['x86-alex32_he'],
    vm_tests=None,
    unittests=None,
    upload_hw_test_artifacts=False,
  ),
)

_config.add_group('x86-zgb-release-group',
  _release.add_config('x86-zgb-release',
    boards=['x86-zgb'],
    hw_tests=['bvt'],
  ),
  _release.add_config('x86-zgb_he-release',
    boards=['x86-zgb_he'],
    vm_tests=None,
    unittests=None,
    upload_hw_test_artifacts=False,
  ),
)

_config.add_group('x86-zgb32-release-group',
  _release.add_config('x86-zgb32-release',
    boards=['x86-zgb32'],
  ),
  _release.add_config('x86-zgb32_he-release',
    boards=['x86-zgb32_he'],
    vm_tests=None,
    unittests=None,
    upload_hw_test_artifacts=False,
  ),
)

_release.add_config('stumpy-release',
  boards=['stumpy'],
  hw_tests=['bvt'],
)

_release.add_config('lumpy-release',
  boards=['lumpy'],
  hw_tests=['bvt'],
)

_release.add_config('link-release',
  boards=['link'],
  prebuilts=False,
  vm_tests=None,
)

_release.add_config('kiev-release',
  boards=['kiev'],
)

_arm_release = _release.derive(arm)

_arm_release.add_config('arm-tegra2_seaboard-release',
  boards=['tegra2_seaboard'],
)

_arm_release.add_config('arm-tegra2_kaen-release',
  boards=['tegra2_kaen'],
  hw_tests=['kaen_bvt'],
)

_arm_release.add_config('arm-daisy-release',
  boards=['daisy'],
)

_arm_release.add_config('arm-ironhide-release',
  boards=['ironhide'],
)

# Factory and Firmware releases much inherit from these classes.  Modifications
# for these release builders should go here.

# Naming conventions also must be followed.  Factory and firmware branches must
# end in -factory or -firmware suffixes.

_factory_release = _release.derive(
  vm_tests=None,
)

_firmware_release = _release.derive(
  push_image=False,
  uprev=True,
  overlays=constants.BOTH_OVERLAYS,
  build_tests=False,
  unittests=False,
  vm_tests=None,
  prebuilts=False,
)

# Examples:
#_firmware_release.add_config('x86-mario-firmware',
#  boards=['x86-mario'],
#)

#_factory_release.add_config('x86-mario-factory',
#  boards=['x86-mario'],
#)

def _InjectDisplayPosition(config_source):
  """Add field to help buildbot masters order builders on the waterfall."""
  def _GetSortKey(items):
    my_config = items[1]
    # Allow configs to override the display_position.
    return (my_config.get('display_position', 1000000),
            BUILD_TYPE_DUMP_ORDER.index(my_config['build_type']),
            my_config['internal'], my_config['vm_tests'])

  source = sorted(config_source.iteritems(), key=_GetSortKey)
  return dict((name, dict(value.items() + [('display_position', idx)]))
              for idx, (name, value) in enumerate(source))


def main(argv=None):
  if not argv:
    argv = sys.argv[1:]

  usage = "usage: %prog [options]"
  parser = optparse.OptionParser(usage=usage)

  parser.add_option('-c', '--compare', action='store', type='string',
                    default=None, metavar='file_name',
                    help=('Compare current config against a saved on disk '
                          'serialized version of a config.'))
  parser.add_option('-d', '--dump', action='store_true', default=False,
                    help=('Dump the configs in JSON format.'))
  parser.add_option('--for-buildbot', action='store_true', default=False,
                    help="Include the display position in json data.")

  options = parser.parse_args(argv)[0]

  if options.compare and options.dump:
    parser.error('Cannot run with --load and --dump at the same time!')
  elif not options.compare and not options.dump:
    parser.print_help()
    sys.exit(0)

  convert = lambda x:x
  if options.for_buildbot:
    convert = _InjectDisplayPosition

  my_config = convert(config)

  if options.dump:
    print json.dumps(my_config)
  elif options.compare:
    with open(options.compare, 'rb') as f:
      original = convert(json.load(f))

    for key in sorted(set(my_config.keys() + original.keys())):
      obj1, obj2 = original.get(key), my_config.get(key)
      if obj1 == obj2:
        continue
      elif obj1 is None:
        print '%s: added to config\n' % (key,)
        continue
      elif obj2 is None:
        print '%s: removed from config\n' % (key,)
        continue

      print '%s:' % (key,)

      for subkey in sorted(set(obj1.keys() + obj2.keys())):
        sobj1, sobj2 = obj1.get(subkey), obj2.get(subkey)
        if sobj1 != sobj2:
          print ' %s: %r, %r' % (subkey, sobj1, sobj2)

      print


if __name__ == '__main__':
  main()
