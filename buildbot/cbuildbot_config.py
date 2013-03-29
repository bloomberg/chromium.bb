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

# Contains the valid build config suffixes in the order that they are dumped.
CONFIG_TYPE_RELEASE = 'release'
CONFIG_TYPE_FULL = 'full'
CONFIG_TYPE_FIRMWARE = 'firmware'

CONFIG_TYPE_DUMP_ORDER = (
    'paladin',
    'incremental',
    CONFIG_TYPE_FULL,
    CONFIG_TYPE_RELEASE,
    'release-group',
    'sdk',
    'chromium-pfq',
    'chrome-perf',
    'chrome-pgo',
    'chrome-pfq',
    'chrome-pfq-informational',
    'pre-flight-branch',
    'factory',
    CONFIG_TYPE_FIRMWARE,
    'toolchain-major',
    'toolchain-minor',
    'asan',
    'asan-informational',
    'refresh-packages',
)

DEFAULT_HW_TESTS = ['bvt']
DEFAULT_HW_TEST_TIMEOUT = 8400

def OverrideConfigForTrybot(build_config, options):
  """Apply trybot-specific configuration settings.

  Args:
    build_config:  The build configuration dictionary to override.
      The dictionary is not modified.
    options: The options passed on the commandline.
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
    if (not options.remote_trybot and useflags and
        constants.USE_CHROME_PDF in useflags):
      useflags.remove(constants.USE_CHROME_PDF)

    my_config['upload_symbols'] = False
    my_config['push_image'] = False
    my_config['hw_tests_pool'] = constants.HWTEST_TRYBOT_POOL
    my_config['hw_tests_num'] = constants.HWTEST_TRYBOT_NUM
    my_config['hw_tests_file_bugs'] = False

    if options.hwtest:
      if not my_config['hw_tests']:
        my_config['hw_tests'] = DEFAULT_HW_TESTS
      my_config['hw_tests_timeout'] = DEFAULT_HW_TEST_TIMEOUT

    # Default to starting with a fresh chroot on remote trybot runs.
    if options.remote_trybot:
      my_config['chroot_replace'] = True

    # In trybots, we want to always run VM tests and all unit tests, so that
    # developers will get better testing for their changes.
    if (my_config['build_type'] == constants.PALADIN_TYPE
        and not my_config['arm']):
      my_config['vm_tests'] = constants.SIMPLE_AU_TEST_TYPE
      my_config['quick_unit'] = False

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

# pylint: disable=W0102
def GetSlavesForMaster(master_config, configs=config):
  """Gets the important builds corresponding to a master builder.

  Given a master builder, find all corresponding slaves that
  are important to me.  These are those builders that share the same
  build_type and manifest_version url.
  """
  assert not master_config['unified_manifest_version']
  assert master_config['manifest_version']
  assert master_config['master']
  builders = []
  build_type = master_config['build_type']
  branch_config = master_config['branch']
  chrome_rev = master_config['chrome_rev']
  for build_name, conf in configs.iteritems():
    if (conf['important'] and conf['manifest_version'] and
        not conf['unified_manifest_version'] and
        conf['build_type'] == build_type and
        conf['chrome_rev'] == chrome_rev and
        conf['branch'] == branch_config):
      builders.append(build_name)

  return builders


# pylint: disable=W0102
def GetCanariesForChromeLKGM(configs=config):
  """Grabs a list of builders that are important for the Chrome LKGM."""
  builders = []
  for build_name, conf in configs.iteritems():
    if (conf['build_type'] == constants.CANARY_TYPE and
        conf['critical_for_chrome']):
      builders.append(build_name)

  return builders


def FindFullConfigsForBoard(board):
  """Returns full builder configs for a board.

  Returns:
    A tuple containing a list of matching external configs and a list of
    matching internal configs for a board.
  """
  ext_cfgs = []
  int_cfgs = []

  for name, c in config.iteritems():
    if c['boards'] and board in c['boards']:
      if name == '-'.join((board, CONFIG_TYPE_RELEASE)) and c['internal']:
        int_cfgs.append(copy.deepcopy(c))
      elif name == '-'.join((board, CONFIG_TYPE_FULL)) and not c['internal']:
        ext_cfgs.append(copy.deepcopy(c))

  return ext_cfgs, int_cfgs


def FindCanonicalConfigForBoard(board):
  """Get the canonical cbuildbot builder config for a board."""
  ext_cfgs, int_cfgs = FindFullConfigsForBoard(board)
  # If both external and internal builds exist for this board, prefer the
  # internal one.
  both = int_cfgs + ext_cfgs
  if not both:
    raise ValueError('Invalid board specified: %s.' % board)
  return both[0]


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

# manifest -- The name of the manifest to use. E.g., to use the buildtools
#             manifest, specify 'buildtools'.
  manifest=constants.DEFAULT_MANIFEST,

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

# sync_chrome -- Does this profile need to sync chrome?  If None, we guess based
#                on other factors.  If True/False, we always do that.
  sync_chrome=None,

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
#          Note that we uprev on just about every bot config because it gives us
#          a more deterministic build system (the tradeoff being that some bots
#          build from source more frequently than if they never did an uprev).
#          This way the release/factory/etc... builders will pick up changes
#          that devs pushed before it runs, but after the correspoding PFQ bot
#          ran (which is what creates+uploads binpkgs).  The incremental bots
#          are about the only ones that don't uprev because they mimic the flow
#          a developer goes through on their own local systems.
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

# signer_tests -- Runs the tests that the signer would run.
  signer_tests=False,

# unittests -- Runs unittests for packages.
  unittests=True,

# quick_unit -- If unittests is true, only run the unit tests for packages which
#               have changed since the previous build.
  quick_unit=True,

# build_tests -- Builds autotest tests.  Must be True if vm_tests is set.
  build_tests=True,

# vm_tests -- Run vm test type defined in constants.
  vm_tests=constants.SIMPLE_AU_TEST_TYPE,

# hw_copy_perf_results: If set to True, copy test results back from GS and send
# them to the perf dashboard.
  hw_copy_perf_results=False,

# hw_tests_timeout -- Usually, 2 hours and twenty minutes.
  hw_tests_timeout=DEFAULT_HW_TEST_TIMEOUT,

# hw_tests_pool -- Pool to use for hw testing.
  hw_tests_pool=constants.HWTEST_MACH_POOL,

# hw_tests -- A list of autotest suites to run on remote hardware.
  hw_tests=[],
# async_hw_tests -- A list of autotest suites to kick off and forget about.
  async_hw_tests=[],

# hw_tests_critical -- Usually we consider structural failures here as OK.
  hw_tests_critical=False,

# Maximum number of devices to use when scheduling tests in the hw lab.
  hw_tests_num=constants.HWTEST_DEFAULT_NUM,

# Should we file bugs if a test fails in a suite run.
  hw_tests_file_bugs=True,

# upload_hw_test_artifacts -- If true, uploads artifacts for hw testing.
  upload_hw_test_artifacts=False,

# upload_standalone_images -- If true, uploads individual image tarballs.
  upload_standalone_images=True,

# gs_path -- Google Storage path to offload files to.
#            None - No upload
#            GS_PATH_DEFAULT - 'gs://chromeos-image-archive/' + bot_id
#            value - Upload to explicit path
  gs_path=GS_PATH_DEFAULT,

# TODO(sosa): Deprecate binary.
# build_type -- Type of builder.  Check constants.VALID_BUILD_TYPES.
  build_type=constants.PFQ_TYPE,

# arm -- Whether the board we are building is arm-based.
  arm=False,

  archive_build_debug=False,

# images -- List of images we want to build -- see build_image for more details.
  images=['test'],
  factory_install_netboot=True,

# packages -- Tuple of specific packages we want to build.  Most configs won't
#             specify anything here and instead let build_packages calculate.
  packages=(),

# push_image -- Do we push a final release image to chromeos-images.
  push_image=False,

# upload_symbols -- Do we upload debug symbols.
  upload_symbols=False,

# manifest_repo_url -- git repository URL for our manifests.
#   External: https://git.chromium.org/git/chromiumos/manifest
#   Internal: ssh://gerrit-int.chromium.org:29419/chromeos/manifest-internal
  manifest_repo_url=constants.MANIFEST_URL,

# manifest_version -- Whether we are using the manifest_version repo that stores
#                     per-build manifests.
  manifest_version=False,

# TODO(sosa): Merge with overlays == both once unified waterfall launched.
# unified_manifest_version -- If True, publish manifests to both manifest
# version repositories.
  unified_manifest_version=False,

# use_lkgm -- Use the Last Known Good Manifest blessed by Paladin.
  use_lkgm=False,

# use_chrome_lkgm -- LKGM for Chrome OS generated for Chrome builds that are
# blessed from canary runs.
  use_chrome_lkgm=False,

# True if this build config is critical for the chrome_lkgm decision.
  critical_for_chrome=False,

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

# git_sync -- Boolean that enables parameter --git-sync for upload_prebuilts.
  git_sync=False,

# board_specific_configs -- A mapping from boards in the boards array to
#                           config objects. Only used for config groups (see
#                           the add_group method).
  board_specific_configs={},

# grouped -- Whether this config belongs to a config group.
  grouped=False,

# disk_layout -- layout of build_image resulting image.
#                See scripts/build_library/legacy_disk_layout.json or
#                overlay-<board>/scripts/disk_layout.json for possible values.
  disk_layout=None,

# disk_vm_layout -- layout of image_to_vm.sh resulting image. See
#                   disk_layout for more info.
  disk_vm_layout=None,

# TODO(sosa): Collapse to one option.
# ====================== Dev installer prebuilts options =======================

# binhost_bucket -- Upload prebuilts for this build to this bucket. If it equals
#                   None the default buckets are used.
  binhost_bucket=None,

# binhost_key -- Parameter --key for upload_prebuilts. If it equals None, the
#                default values are used, which depend on the build type.
  binhost_key=None,

# binhost_base_url -- Parameter --binhost-base-url for upload_prebuilts. If it
#                     equals None, the default value is used.
  binhost_base_url=None,

# Upload dev installer prebuilts.
  dev_installer_prebuilts=False,

# Enable rootfs verification on the image.
  rootfs_verification=True,

# =============================================================================
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
  def add_group(cls, name, *configs, **group_overrides):
    """Create a new group of build configurations.

    Args:
      name: The name to label this configuration; this is what cbuildbot
            would see.
      configs: Configurations to build in this group. The first config in
               the group is considered the primary configuration and is used
               for syncing and creating the chroot.
      group_overrides: See the docstring of derive. Applies to entire group.
    """
    board_specific_configs = {}
    for x in configs:
      for my_board in x['boards']:
        board_specific_configs[my_board] = _default.derive(x, grouped=True)

    group_overrides.update(dict(
        boards=list(itertools.chain.from_iterable(
            x['boards'] for x in configs)),
        board_specific_configs=board_specific_configs))
    return configs[0].add_config(name, **group_overrides)

_default = _config(**_settings)


# It is only safe to inherit prebuilts from generic boards, or from the
# same board without the variant. This rule helps keep inheritance trees
# sane.
generic_boards = set([
  'x86-generic', 'amd64-generic', 'daisy'
])


# Arch-specific mixins.

arm = _config(
  arm=True,
  # VM/tests are broken on arm.
  unittests=False,
  vm_tests=None,
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
  unified_manifest_version=True,
  build_type=constants.PALADIN_TYPE,
  overlays=constants.PUBLIC_OVERLAYS,
  prebuilts=True,
  manifest_version=True,
  trybot_list=True,
  description='Commit Queue',
  hw_tests_timeout=50 * 60,
  hw_tests_file_bugs=False,
  upload_standalone_images=False,
)

incremental = _config(
  build_type=constants.INCREMENTAL_TYPE,
  uprev=False,
  overlays=constants.PUBLIC_OVERLAYS,
  prebuilts=False,
  description='Incremental Builds',
)

# This builds with more source available.
internal = _config(
  internal=True,
  overlays=constants.BOTH_OVERLAYS,
  manifest_repo_url=constants.MANIFEST_INT_URL,
)

# This adds Chrome branding, and removes highdpi resources by default to save
# space on the image.
official = _config(
  useflags=[constants.USE_CHROME_INTERNAL, constants.USE_CHROME_PDF,
            '-highdpi'],
  chromeos_official=True,
  prebuilts=False,
)

_cros_sdk = full.add_config('chromiumos-sdk',
  # The amd64-host has to be last as that is when the toolchains
  # are bundled up for inclusion in the sdk.
  boards=('x86-generic', 'arm-generic', 'amd64-generic'),
  build_type=constants.CHROOT_BUILDER_TYPE,
  use_sdk=False,
  trybot_list=True,
  description='Build the SDK and all the cross-compilers',
)

asan = _config(
  chroot_replace=True,
  profile='asan',
  useflags=['asan'],  # see profile for more
  disk_layout='2gb-rootfs',
  disk_vm_layout='2gb-rootfs-updatable',
)

_config.add_raw_config('refresh-packages',
  boards=['x86-generic', 'arm-generic'],
  build_type=constants.REFRESH_PACKAGES_TYPE,
  description='Check upstream Gentoo for package updates',
)

incremental.add_config('x86-generic-incremental',
  boards=['x86-generic'],
)

incremental.add_config('daisy-incremental',
  arm,
  boards=['daisy'],
)

incremental.add_config('amd64-generic-incremental',
  amd64,
  boards=['amd64-generic'],
  # This builder runs on a VM, so it can't run VM tests.
  vm_tests=None,
)

incremental.add_config('x32-generic-incremental',
  amd64,
  boards=['x32-generic'],
  # This builder runs on a VM, so it can't run VM tests.
  vm_tests=None,
)

paladin.add_config('x86-generic-paladin',
  boards=['x86-generic'],
  paladin_builder_name='x86 generic paladin',
)

paladin.add_config('amd64-generic-paladin',
  amd64,
  boards=['amd64-generic'],
  paladin_builder_name='amd64 generic paladin',
)

paladin.add_config('x32-generic-paladin',
  amd64,
  boards=['x32-generic'],
  paladin_builder_name='x32 generic paladin',
  important=False,
)

chromium_pfq = _config(
  build_type=constants.CHROME_PFQ_TYPE,
  important=True,
  uprev=False,
  overlays=constants.PUBLIC_OVERLAYS,
  manifest_version=True,
  chrome_rev=constants.CHROME_REV_LATEST,
  upload_hw_test_artifacts=True,
)

# TODO(davidjames): Convert this to an external config once the unified master
# logic is ready.
internal_chromium_pfq = internal.derive(
  chromium_pfq,
  description='Preflight Chromium Build (internal)',
)

internal_chromium_pfq.add_config('x86-generic-chromium-pfq',
  boards=['x86-generic'],
  master=True,
  push_overlays=constants.PUBLIC_OVERLAYS,
)

internal_chromium_pfq.add_config('daisy-chromium-pfq',
  arm,
  boards=['daisy'],
)

internal_chromium_pfq.add_config('amd64-generic-chromium-pfq',
  amd64,
  boards=['amd64-generic'],
)

chrome_pfq = internal_chromium_pfq.derive(
  official,
  important=True,
  overlays=constants.BOTH_OVERLAYS,
  description='Preflight Chrome build (internal)',
  upload_hw_test_artifacts=True,
)

chrome_pfq.add_config('alex-chrome-pfq',
  boards=['x86-alex'],
)

chrome_pfq.add_config('lumpy-chrome-pfq',
  boards=['lumpy'],
)

chrome_pgo = chrome_pfq.derive(
  useflags=official['useflags'] + [constants.USE_PGO_GENERATE],
  usepkg_build_packages=False,

  vm_tests=None,
  hw_tests=['PGO_record'],
  hw_tests_critical=True,
  hw_tests_pool=constants.HWTEST_CHROME_PFQ_POOL,
  hw_tests_num=1,
  hw_tests_timeout=90 * 60,
  disk_layout='2gb-rootfs',

  # TODO(petermayo): Remove once PGO is reliable again.
  important=False,
)

chrome_pgo.add_config('alex-chrome-pgo',
  boards=['x86-alex'],
)

chrome_pgo.add_config('lumpy-chrome-pgo',
  boards=['lumpy'],
)

chromium_info = chromium_pfq.derive(
  chrome_rev=constants.CHROME_REV_TOT,
  use_lkgm=True,
  important=False,
  manifest_version=False,
  vm_tests=constants.SMOKE_SUITE_TEST_TYPE,
)

chrome_info = chromium_info.derive(
  internal, official,
)

chrome_perf = chrome_info.derive(
  vm_tests=None,
  upload_hw_test_artifacts=True,

  hw_copy_perf_results=True,
  hw_tests=['pyauto_perf'],
  hw_tests_critical=True,
  hw_tests_pool=constants.HWTEST_CHROME_PERF_POOL,
  hw_tests_num=1,
  hw_tests_timeout=90 * 60,
  nowithdebug=True,
  use_chrome_lkgm=True,
  use_lkgm=False,


  # TODO(sosa): Really shouldn't be necessary in addition with
  # nowithdebug. One should imply the other.
  useflags=official['useflags'] + ['-chrome_debug'],
)

chrome_perf.add_config('lumpy-chrome-perf',
  boards=['lumpy'],
  trybot_list=True,
)

chromium_info_x86 = \
chromium_info.add_config('x86-generic-tot-chrome-pfq-informational',
  boards=['x86-generic'],
)

chromium_info_daisy = \
chromium_info.add_config('daisy-tot-chrome-pfq-informational',
  arm,
  boards=['daisy'],
)

chromium_info_amd64 = \
chromium_info.add_config('amd64-generic-tot-chrome-pfq-informational',
  amd64,
  boards=['amd64-generic'],
)

chromium_info.add_config('x32-generic-tot-chrome-pfq-informational',
  amd64,
  boards=['x32-generic'],
)

chrome_info.add_config('alex-tot-chrome-pfq-informational',
  boards=['x86-alex'],
)

chrome_info.add_config('lumpy-tot-chrome-pfq-informational',
  boards=['lumpy'],
)

chromium_info_x86.add_config('x86-webrtc-chrome-pfq-informational',
  archive_build_debug=True,
)
chromium_info_amd64.add_config('amd64-webrtc-chrome-pfq-informational',
  archive_build_debug=True,
)
chromium_info_daisy.add_config('daisy-webrtc-chrome-pfq-informational',
  archive_build_debug=True,
)

arm_generic_full = \
full.add_config('arm-generic-full', arm,
  boards=['arm-generic'],
)

arm_generic_full.add_config('daisy-full',
  boards=['daisy'],
)

x86_generic_full = \
full.add_config('x86-generic-full',
  boards=['x86-generic'],
  upload_hw_test_artifacts=True,
)

x86_generic_full.add_config('x86-pineview-full',
  boards=['x86-pineview'],
)

full.add_config('x86-mario-full',
  boards=['x86-mario'],
)

full.add_config('x86-alex-full',
  boards=['x86-alex'],
)

full.add_config('stumpy-full',
  boards=['stumpy'],
)

_toolchain_major = _cros_sdk.add_config('toolchain-major',
  latest_toolchain=True,
  gcc_githash='gcc.gnu.org/branches/google/main',
  description='Test next major toolchain revision',
)

_toolchain_minor = _cros_sdk.add_config('toolchain-minor',
  latest_toolchain=True,
  gcc_githash='gcc.gnu.org/branches/google/gcc-4_7-mobile',
  description='Test next minor toolchain revision',
)

full.add_config('amd64-generic-full',
  boards=['amd64-generic'],
)

full.add_config('x32-generic-full',
  boards=['x32-generic'],
)

incremental.add_config('x86-generic-asan',
  asan,
  boards=['x86-generic'],
  description='Build with Address Sanitizer (Clang)',
)

chromium_info.add_config('x86-generic-tot-asan-informational',
  asan,
  boards=['x86-generic'],
  description='Build with Address Sanitizer (Clang) on TOT',
)

incremental.add_config('amd64-generic-asan',
  amd64,
  asan,
  boards=['amd64-generic'],
  description='Build with Address Sanitizer (Clang)',
)

chromium_info.add_config('amd64-generic-tot-asan-informational',
  amd64,
  asan,
  boards=['amd64-generic'],
  description='Build with Address Sanitizer (Clang) on TOT',
)

incremental_arm = incremental.derive(arm)
incremental_arm.add_config('beaglebone-incremental',
  boards=['beaglebone'],
  images=['dev'],
  trybot_list=True,
  build_tests=False,
  rootfs_verification=False,
  description='Incremental Beaglebone Builder',
)

#
# Internal Builds
#

internal_pfq = internal.derive(pfq, overlays=constants.BOTH_OVERLAYS)
internal_pfq_branch = internal_pfq.derive(overlays=constants.BOTH_OVERLAYS,
                                          trybot_list=False, branch=True)
internal_paladin = internal.derive(paladin,
  overlays=constants.BOTH_OVERLAYS,
  vm_tests=None,
  hw_tests_pool=constants.HWTEST_PALADIN_POOL,
  description=paladin['description'] + ' (internal)',
)

internal_incremental = internal.derive(
  incremental,
  overlays=constants.BOTH_OVERLAYS,
  description='Incremental Builds (internal)',
)

internal_pfq_branch.add_config('x86-alex-pre-flight-branch',
  master=True,
  push_overlays=constants.BOTH_OVERLAYS,
  boards=['x86-alex'],
)

internal_arm_paladin = internal_paladin.derive(arm)

internal_paladin.add_config('mario-paladin',
  master=True,
  push_overlays=constants.BOTH_OVERLAYS,
  boards=['x86-mario'],
  gs_path='gs://chromeos-x86-mario/pre-flight-master',
  paladin_builder_name='mario paladin',
  vm_tests=constants.SIMPLE_AU_TEST_TYPE,
)

internal_arm_paladin.add_config('daisy-paladin',
  boards=['daisy'],
  paladin_builder_name='daisy paladin',
  upload_hw_test_artifacts=True,
)

internal_arm_paladin.add_config('spring-paladin',
  boards=['daisy_spring'],
  paladin_builder_name='daisy spring paladin',
  important=False,
)

internal_arm_paladin.add_config('peach-paladin',
  boards=['peach_pit'],
  paladin_builder_name='peach pit paladin',
  important=False,
)

internal_paladin.add_config('zgb-paladin',
  boards=['x86-zgb'],
  important=False,
  paladin_builder_name='zgb paladin',
)

internal_paladin.add_config('alex-paladin',
  boards=['x86-alex'],
  paladin_builder_name='alex paladin',
  hw_tests=DEFAULT_HW_TESTS,
  upload_hw_test_artifacts=True,
)

internal_paladin.add_config('fox-wtm2-paladin',
  boards=['fox_wtm2'],
  paladin_builder_name='fox paladin',
  vm_tests=None,
  important=False,
)

internal_paladin.add_config('stumpy-paladin',
  boards=['stumpy'],
  paladin_builder_name='stumpy paladin',
  upload_hw_test_artifacts=True,
)

internal_paladin.add_config('lumpy-paladin',
  boards=['lumpy'],
  paladin_builder_name='lumpy paladin',
  hw_tests=DEFAULT_HW_TESTS,
  upload_hw_test_artifacts=True,
)

internal_paladin.add_config('link-paladin',
  boards=['link'],
  paladin_builder_name='link paladin',
  quick_unit=False,
  upload_hw_test_artifacts=True,
)

internal_paladin.add_config('parrot-paladin',
  boards=['parrot'],
  paladin_builder_name='parrot paladin',
  quick_unit=False,
)

internal_paladin.add_config('stout-paladin',
  boards=['stout'],
  paladin_builder_name='stout paladin',
)

internal_paladin.add_config('butterfly-paladin',
  boards=['butterfly'],
  paladin_builder_name='butterfly paladin',
)

internal_incremental.add_config('mario-incremental',
  boards=['x86-mario'],
)

_toolchain_major.add_config('internal-toolchain-major', internal, official,
  boards=('x86-alex', 'stumpy', 'daisy'),
  use_lkgm=True,
  useflags=[constants.USE_CHROME_INTERNAL],
  build_tests=True,
  description=_toolchain_major['description'] + ' (internal)',
)

_toolchain_minor.add_config('internal-toolchain-minor', internal, official,
  boards=('x86-alex', 'stumpy', 'daisy'),
  use_lkgm=True,
  useflags=[constants.USE_CHROME_INTERNAL],
  build_tests=True,
  description=_toolchain_minor['description'] + ' (internal)',
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
  hw_tests=DEFAULT_HW_TESTS + [constants.HWTEST_AU_SUITE],
  upload_hw_test_artifacts=True,
  signer_tests=True,
  trybot_list=True,
  description="Release Builds (canary) (internal)",
)

_release.add_config('x86-mario-release',
  boards=['x86-mario'],
  master=True,
)

_config.add_group('x86-alex-release-group',
  _release.add_config('x86-alex-release',
    boards=['x86-alex'],
  ),
  _release.add_config('x86-alex_he-release',
    boards=['x86-alex_he'],
    vm_tests=None,
    unittests=None,
    hw_tests=[],
    upload_hw_test_artifacts=False,
  ),
  critical_for_chrome=True,
)

_config.add_group('x86-zgb-release-group',
  _release.add_config('x86-zgb-release',
    boards=['x86-zgb'],
  ),
  _release.add_config('x86-zgb_he-release',
    boards=['x86-zgb_he'],
    vm_tests=None,
    unittests=None,
    hw_tests=[],
    upload_hw_test_artifacts=False,
  ),
)

_release.add_config('stumpy-release',
  boards=['stumpy'],
)

_release.add_config('lumpy-release',
  boards=['lumpy'],
  critical_for_chrome=True,
)

_release.add_config('lumpy-pgo-release',
  boards=['lumpy'],
  hw_tests=DEFAULT_HW_TESTS,
  hw_tests_pool=constants.HWTEST_CHROME_PERF_POOL,
  hw_tests_num=4,
  useflags=official['useflags'] + [constants.USE_PGO_USE],
  push_image=False,
  dev_installer_prebuilts=False,
)

_release.add_config('link-release',
  boards=['link'],
  useflags=official['useflags'] + ['highdpi'],
  prebuilts=False,
)

_release.add_config('parrot-release',
  boards=['parrot'],
)

_release.add_config('stout-release',
  boards=['stout'],
  hw_tests_num=3,
)

_release.add_config('butterfly-release',
  boards=['butterfly'],
  useflags=official['useflags'] + ['oem_wallpaper'],
)

_release.add_config('fox-wtm2-release',
  boards=['fox_wtm2'],
  # Until these are configured and ready, disable them.
  signer_tests=False,
  vm_tests=None,
  hw_tests=[],
)

_arm_release = _release.derive(arm)

_arm_release.add_config('daisy-release',
  boards=['daisy'],
  hw_tests_num=4,
  critical_for_chrome=True,
)

_arm_release.add_config('spring-release',
  boards=['daisy_spring'],
  hw_tests=[],
)

# Factory and Firmware releases much inherit from these classes.  Modifications
# for these release builders should go here.

# Naming conventions also must be followed.  Factory and firmware branches must
# end in -factory or -firmware suffixes.

_factory_release = _release.derive(
  prebuilts=False,
  upload_hw_test_artifacts=False,
  upload_symbols=False,
  hw_tests=[],
  description='Factory Builds',
)

_firmware_release = _release.derive(
  images=[],
  packages=('virtual/chromeos-firmware',),
  usepkg_setup_board=True,
  usepkg_build_packages=True,
  sync_chrome=False,
  build_tests=False,
  unittests=False,
  vm_tests=None,
  hw_tests=[],
  prebuilts=False,
  dev_installer_prebuilts=False,
  upload_hw_test_artifacts=False,
  upload_symbols=False,
  signer_tests=False,
  trybot_list=False,
  description='Firmware Builds',
)

_depthcharge_release = _firmware_release.derive(useflags=['depthcharge'])

_x86_firmware_boards = (
  'butterfly',
  'link',
  'lumpy',
  'parrot',
  'stout',
  'stumpy',
  'x86-mario',
)

_x86_depthcharge_firmware_boards = (
  'link',
)

_arm_firmware_boards = (
  'daisy',
  'daisy_spring',
)

def _AddFirmwareConfigs():
  """Add x86 and arm firmware configs."""
  for board in _x86_firmware_boards:
    _firmware_release.add_config('%s-%s' % (board, CONFIG_TYPE_FIRMWARE),
      boards=[board],
    )

  for board in _x86_depthcharge_firmware_boards:
    _depthcharge_release.add_config(
        '%s-%s-%s' % (board, 'depthcharge', CONFIG_TYPE_FIRMWARE),
        boards=[board],
    )

  for board in _arm_firmware_boards:
    _firmware_release.add_config('%s-%s' % (board, CONFIG_TYPE_FIRMWARE),
      arm,
      boards=[board],
    )

_AddFirmwareConfigs()


# This is an example factory branch configuration for x86.
# Modify it to match your factory branch.
_factory_release.add_config('x86-mario-factory',
  boards=['x86-mario'],
)

# This is an example factory branch configuration for arm.
# Modify it to match your factory branch.
_factory_release.add_config('daisy-factory',
  arm,
  boards=['daisy'],
)


def _GetDisplayPosition(config_name, type_order=CONFIG_TYPE_DUMP_ORDER):
  """Given a config_name, return display position specified by suffix_order.

  Arguments:
    config_name: Name of config to look up.
    type_order: A tuple/list of config types in the order they are to be
                displayed.

  If config name does not contain any of the suffixes, returns the index
  position after the last element of suffix_order.
  """
  for index, config_type in enumerate(type_order):
    if config_name.endswith('-' + config_type) or config_name == config_type:
      return index

  return len(type_order)


def _InjectDisplayPosition(config_source):
  """Add field to help buildbot masters order builders on the waterfall."""
  def _GetSortKey(items):
    my_config = items[1]
    # Allow configs to override the display_position.
    return (my_config.get('display_position', 1000000),
            _GetDisplayPosition(my_config['name']),
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
