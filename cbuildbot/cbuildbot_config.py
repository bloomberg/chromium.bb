#!/usr/bin/python2
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configuration options for various cbuildbot builders."""

# pylint: disable=bad-continuation

from __future__ import print_function

# Disable relative import warning from pylint.
# pylint: disable=W0403
import constants
import copy
import json

GS_PATH_DEFAULT = 'default' # Means gs://chromeos-image-archive/ + bot_id

# Contains the valid build config suffixes in the order that they are dumped.
CONFIG_TYPE_PRECQ = 'pre-cq'
CONFIG_TYPE_PALADIN = 'paladin'
CONFIG_TYPE_RELEASE = 'release'
CONFIG_TYPE_FULL = 'full'
CONFIG_TYPE_FIRMWARE = 'firmware'
CONFIG_TYPE_RELEASE_AFDO = 'release-afdo'

CONFIG_TYPE_DUMP_ORDER = (
    CONFIG_TYPE_PALADIN,
    constants.PRE_CQ_GROUP_CONFIG,
    CONFIG_TYPE_PRECQ,
    constants.PRE_CQ_LAUNCHER_CONFIG,
    'incremental',
    'telemetry',
    CONFIG_TYPE_FULL,
    'full-group',
    CONFIG_TYPE_RELEASE,
    'release-group',
    'release-afdo',
    'release-afdo-generate',
    'release-afdo-use',
    'sdk',
    'chromium-pfq',
    'chromium-pfq-informational',
    'chrome-perf',
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
    'test-ap',
    'test-ap-group',
    constants.BRANCH_UTIL_CONFIG,
    constants.PAYLOADS_TYPE,
)


def OverrideConfigForTrybot(build_config, options):
  """Apply trybot-specific configuration settings.

  Args:
    build_config: The build configuration dictionary to override.
      The dictionary is not modified.
    options: The options passed on the commandline.

  Returns:
    A build configuration dictionary with the overrides applied.
  """
  copy_config = copy.deepcopy(build_config)
  for my_config in [copy_config] + copy_config['child_configs']:
    # For all builds except PROJECT_SDK, force uprev. This is so patched in
    # changes are always built. PROEJCT_SDK_TYPE uses a minilayout, and uprev
    # doesn't work for minilayout (crbug.com/458661).
    if my_config['build_type'] != constants.PROJECT_SDK_TYPE:
      my_config['uprev'] = True
      if my_config['internal']:
        my_config['overlays'] = constants.BOTH_OVERLAYS

    # Most users don't have access to the internal repositories so disable
    # them so that we use the external chromium prebuilts.
    useflags = my_config['useflags']
    if not options.remote_trybot and useflags:
      for chrome_use in official_chrome['useflags']:
        if chrome_use in useflags:
          useflags.remove(chrome_use)

    # Use the local manifest which only requires elevated access if it's really
    # needed to build.
    if not options.remote_trybot:
      my_config['manifest'] = my_config['dev_manifest']

    my_config['push_image'] = False

    if my_config['build_type'] != constants.PAYLOADS_TYPE:
      my_config['paygen'] = False

    if options.hwtest:
      if not my_config['hw_tests']:
        my_config['hw_tests'] = HWTestConfig.DefaultList(
            num=constants.HWTEST_TRYBOT_NUM, pool=constants.HWTEST_TRYBOT_POOL,
            file_bugs=False)
      else:
        for hw_config in my_config['hw_tests']:
          hw_config.num = constants.HWTEST_TRYBOT_NUM
          hw_config.pool = constants.HWTEST_TRYBOT_POOL
          hw_config.file_bugs = False
          hw_config.priority = constants.HWTEST_DEFAULT_PRIORITY
      # TODO: Fix full_release_test.py/AUTest on trybots, crbug.com/390828.
      my_config['hw_tests'] = [hw_config for hw_config in my_config['hw_tests']
                               if hw_config.suite != constants.HWTEST_AU_SUITE]

    # Default to starting with a fresh chroot on remote trybot runs.
    if options.remote_trybot:
      my_config['chroot_replace'] = True

    # In trybots, we want to always run VM tests and all unit tests, so that
    # developers will get better testing for their changes.
    if (my_config['build_type'] == constants.PALADIN_TYPE and
        my_config['tests_supported'] and
        all(x not in _arm_boards for x in my_config['boards']) and
        all(x not in _brillo_boards for x in my_config['boards'])):
      my_config['vm_tests'] = [constants.SMOKE_SUITE_TEST_TYPE,
                               constants.SIMPLE_AU_TEST_TYPE,
                               constants.CROS_VM_TEST_TYPE]
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
  # pylint: disable=W0613
  if internal_build:
    url = constants.INTERNAL_GOB_URL + constants.MANIFEST_VERSIONS_INT_SUFFIX
  else:
    url = constants.EXTERNAL_GOB_URL + constants.MANIFEST_VERSIONS_SUFFIX

  if test:
    url += '-test'

  return url


def IsPFQType(b_type):
  """Returns True if this build type is a PFQ."""
  return b_type in (constants.PFQ_TYPE, constants.PALADIN_TYPE,
                    constants.CHROME_PFQ_TYPE)


def IsCQType(b_type):
  """Returns True if this build type is a Commit Queue."""
  return b_type == constants.PALADIN_TYPE


def IsCanaryType(b_type):
  """Returns True if this build type is a Canary."""
  return b_type == constants.CANARY_TYPE

def GetDefaultWaterfall(build_config):
  if not (build_config['important'] or build_config['master']):
    return None
  if build_config['branch']:
    return None

  b_type = build_config['build_type']
  if (
      IsPFQType(b_type) or
      IsCQType(b_type) or
      IsCanaryType(b_type) or
      b_type in (
        constants.PRE_CQ_LAUNCHER_TYPE,
      )):
    if build_config['internal']:
      return constants.WATERFALL_INTERNAL
    else:
      return constants.WATERFALL_EXTERNAL


# List of usable cbuildbot configs; see add_config method.
# TODO(mtennant): This is seriously buried in this file.  Move to top
# and rename something that stands out in a file where the word "config"
# is used everywhere.
config = {}


# pylint: disable=W0102
def GetCanariesForChromeLKGM(configs=config):
  """Grabs a list of builders that are important for the Chrome LKGM."""
  builders = []
  for build_name, conf in configs.iteritems():
    if (conf['build_type'] == constants.CANARY_TYPE and
        conf['critical_for_chrome'] and not conf['child_configs']):
      builders.append(build_name)

  return builders


def FindFullConfigsForBoard(board=None):
  """Returns full builder configs for a board.

  Args:
    board: The board to match. By default, match all boards.

  Returns:
    A tuple containing a list of matching external configs and a list of
    matching internal release configs for a board.
  """
  ext_cfgs = []
  int_cfgs = []

  for name, c in config.iteritems():
    if c['boards'] and (board is None or board in c['boards']):
      if name.endswith('-%s' % CONFIG_TYPE_RELEASE) and c['internal']:
        int_cfgs.append(copy.deepcopy(c))
      elif name.endswith('-%s' % CONFIG_TYPE_FULL) and not c['internal']:
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


def GetSlavesForMaster(master_config, options=None):
  """Gets the important slave builds corresponding to this master.

  A slave config is one that matches the master config in build_type,
  chrome_rev, and branch.  It also must be marked important.  For the
  full requirements see the logic in code below.

  The master itself is eligible to be a slave (of itself) if it has boards.

  Args:
    master_config: A build config for a master builder.
    options: The options passed on the commandline. This argument is optional,
             and only makes sense when called from cbuildbot.

  Returns:
    A list of build configs corresponding to the slaves for the master
      represented by master_config.

  Raises:
    AssertionError if the given config is not a master config or it does
      not have a manifest_version.
  """
  # This is confusing.  "config" really should be capitalized in this file.
  all_configs = config

  assert master_config['manifest_version']
  assert master_config['master']

  slave_configs = []
  if options is not None and options.remote_trybot:
    return slave_configs

  for build_config in all_configs.itervalues():
    if (build_config['important'] and
        build_config['manifest_version'] and
        (not build_config['master'] or build_config['boards']) and
        build_config['build_type'] == master_config['build_type'] and
        build_config['chrome_rev'] == master_config['chrome_rev'] and
        build_config['branch'] == master_config['branch']):
      slave_configs.append(build_config)

  return slave_configs


# Enumeration of valid settings; any/all config settings must be in this.
# All settings must be documented.

_settings = dict(

# name -- The name of the config.
  name=None,

# boards -- A list of boards to build.
# TODO(mtennant): Change default to [].  The unittests fail if any config
# entry does not overwrite this value to at least an empty list.
  boards=None,

# profile -- The profile of the variant to set up and build.
  profile=None,

# master -- This bot pushes changes to the overlays.
  master=False,

# important -- If False, this flag indicates that the CQ should not check
#              whether this bot passed or failed. Set this to False if you are
#              setting up a new bot. Once the bot is on the waterfall and is
#              consistently green, mark the builder as important=True.
  important=False,

# health_threshold -- An integer. If this builder fails this many
#                     times consecutively, send an alert email to the
#                     recipients health_alert_recipients. This does
#                     not apply to tryjobs. This feature is similar to
#                     the ERROR_WATERMARK feature of upload_symbols,
#                     and it may make sense to merge the features at
#                     some point.
  health_threshold=0,

# health_alert_recipients -- List of email addresses to send health alerts to
#                            for this builder. It supports automatic email
#                            address lookup for the following sheriff types:
#                                'tree': tree sheriffs
#                                'chrome': chrome gardeners
  health_alert_recipients=[],

# internal -- Whether this is an internal build config.
  internal=False,

# branch -- Whether this is a branched build config. Used for pfq logic.
  branch=False,

# manifest -- The name of the manifest to use. E.g., to use the buildtools
#             manifest, specify 'buildtools'.
  manifest=constants.DEFAULT_MANIFEST,

# dev_manifest -- The name of the manifest to use if we're building on a local
#                 trybot. This should only require elevated access if it's
#                 really needed to build this config.
  dev_manifest=constants.DEFAULT_MANIFEST,

# build_before_patching -- Applies only to paladin builders. If true, Sync to
#                          the manifest without applying any test patches, then
#                          do a fresh build in a new chroot. Then, apply the
#                          patches and build in the existing chroot.
  build_before_patching=False,

# do_not_apply_cq_patches -- Applies only to paladin builders. If True, Sync to
#                            the master manifest without applying any of the
#                            test patches, rather than running CommitQueueSync.
#                            This is basically ToT immediately prior to the
#                            current commit queue run.
  do_not_apply_cq_patches=False,

# sanity_check_slaves -- Applies only to master builders. List of the names of
#                        slave builders to be treated as sanity checkers. If
#                        only sanity check builders fail, then the master will
#                        ignore the failures. In a CQ run, if any of the sanity
#                        check builders fail and other builders fail as well,
#                        the master will treat the build as failed, but will
#                        not reset the ready bit of the tested patches.
  sanity_check_slaves=None,

# useflags -- emerge use flags to use while setting up the board, building
#             packages, making images, etc.
  useflags=[],

# chromeos_official -- Set the variable CHROMEOS_OFFICIAL for the build.
#                      Known to affect parallel_emerge, cros_set_lsb_release,
#                      and chromeos_version.sh. See bug chromium-os:14649
  chromeos_official=False,

# usepkg_toolchain -- Use binary packages for building the toolchain.
#                     (emerge --getbinpkg)
  usepkg_toolchain=True,

# usepkg_build_packages -- Use binary packages for build_packages and
#                          setup_board.
  usepkg_build_packages=True,

# build_packages_in_background -- If set, run BuildPackages in the background
#                                 and allow subsequent stages to run in
#                                 parallel with this one.
#
#                                 For each release group, the first builder
#                                 should be set to run in the foreground (to
#                                 build binary packages), and the remainder of
#                                 the builders should be set to run in parallel
#                                 (to install the binary packages.)
  build_packages_in_background=False,

# chrome_binhost_only -- Only use binaries in build_packages for Chrome itself.
  chrome_binhost_only=False,

# sync_chrome -- Does this profile need to sync chrome?  If None, we guess based
#                on other factors.  If True/False, we always do that.
  sync_chrome=None,

# latest_toolchain -- Use the newest ebuilds for all the toolchain packages.
  latest_toolchain=False,

# gcc_githash -- This is only valid when latest_toolchain is True.
# If you set this to a commit-ish, the gcc ebuild will use it to build the
# toolchain compiler.
  gcc_githash=None,

# board_replace -- wipe and replace the board inside the chroot.
  board_replace=False,

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

# compilecheck -- Exit the builder right after checking compilation.
# TODO(mtennant): Should be something like "compile_check_only".
  compilecheck=False,

# pre_cq -- Test CLs to verify they're ready for the commit queue.
  pre_cq=False,

# signer_tests -- Runs the tests that the signer would run.
  signer_tests=False,

# unittests -- Runs unittests for packages.
  unittests=True,

# quick_unit -- If unittests is true, only run the unit tests for packages which
#               have changed since the previous build.
  quick_unit=False,

# unittest_blacklist -- A list of the packages to blacklist from unittests.
  unittest_blacklist=[],

# build_tests -- Builds autotest tests.  Must be True if vm_tests is set.
  build_tests=True,

# afdo_generate -- Generates AFDO data. Will capture a profile of chrome
#                  using a hwtest to run a predetermined set of benchmarks.
  afdo_generate=False,

# afdo_generate_min -- Generates AFDO data, builds the minimum amount
#                      of artifacts and assumes a non-distributed builder
#                      (i.e.: the whole process in a single builder).
  afdo_generate_min=False,

# afdo_update_ebuild -- Update the Chrome ebuild with the AFDO profile info.
  afdo_update_ebuild=False,

# afdo_use -- Uses AFDO data. The Chrome build will be optimized using the
#             AFDO profile information found in the chrome ebuild file.
  afdo_use=False,

# vm_tests -- A list of vm tests to run.
  vm_tests=[constants.SMOKE_SUITE_TEST_TYPE,
            constants.SIMPLE_AU_TEST_TYPE],

# vm_test_runs -- The number of times to run the VMTest stage. If this is >1,
#                 then we will run the stage this many times, stopping if we
#                 encounter any failures.
  vm_test_runs=1,

# A list of HWTestConfig objects to run.
  hw_tests=[],

# upload_hw_test_artifacts -- If true, uploads artifacts for hw testing.
#                             Upload payloads for test image if the image is
#                             built. If not, dev image is used and then base
#                             image.
  upload_hw_test_artifacts=True,

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

# Whether the tests for the board we are building can be run on the builder.
# Normally, we wouldn't be able to run unit and VM tests form non-x86 boards.
  tests_supported=True,

# images -- List of images we want to build -- see build_image for more details.
  images=['test'],

# factory_install_netboot -- Whether to build a netboot image.
  factory_install_netboot=True,

# factory_toolkit -- Whether to build the factory toolkit.
  factory_toolkit=True,

# factory -- Whether to build factory packages in BuildPackages.
  factory=True,

# packages -- Tuple of specific packages we want to build.  Most configs won't
#             specify anything here and instead let build_packages calculate.
  packages=(),

# push_image -- Do we push a final release image to chromeos-images.
  push_image=False,

# upload_symbols -- Do we upload debug symbols.
  upload_symbols=False,

# hwqual -- Whether we upload a hwqual tarball.
  hwqual=False,

# paygen -- Run a stage that generates release payloads for signed images.
  paygen=False,

# paygen_skip_testing -- If the paygen stage runs, generate tests,
#                           and schedule auto-tests for them.
  paygen_skip_testing=False,

# paygen_skip_testing -- If the paygen stage runs, don't generate any delta
#                        payloads. This is only done if deltas are broken
#                        for a given board.
  paygen_skip_delta_payloads=False,

# cpe_export -- Run a stage that generates and uploads package CPE information.
  cpe_export=True,

# debug_symbols -- Run a stage that generates and uploads debug symbols.
  debug_symbols=True,

# separate_debug_symbols -- Do not package the debug symbols in the binary
# package. The debug symbols will be in an archive with the name cpv.debug.tbz2
# in /build/${BOARD}/packages and uploaded with the prebuilt.
  separate_debug_symbols=True,

# archive_build_debug -- Include *.debug files for debugging core files with
#                        gdb in debug.tgz. These are very large. This option
#                        only has an effect if debug_symbols and archive are
#                        set.
  archive_build_debug=False,

# archive -- Run a stage that archives build and test artifacts for developer
#            consumption.
  archive=True,

# manifest_repo_url -- git repository URL for our manifests.
#  External: https://chromium.googlesource.com/chromiumos/manifest
#  Internal: https://chrome-internal.googlesource.com/chromeos/manifest-internal
  manifest_repo_url=constants.MANIFEST_URL,

# manifest_version -- Whether we are using the manifest_version repo that stores
#                     per-build manifests.
  manifest_version=False,

# use_lkgm -- Use the Last Known Good Manifest blessed by Paladin.
  use_lkgm=False,

# If we use_lkgm -- What is the name of the manifest to look for?
  lkgm_manifest=constants.LKGM_MANIFEST,

# use_chrome_lkgm -- LKGM for Chrome OS generated for Chrome builds that are
# blessed from canary runs.
  use_chrome_lkgm=False,

# True if this build config is critical for the chrome_lkgm decision.
  critical_for_chrome=False,

# prebuilts -- Upload prebuilts for this build. Valid values are PUBLIC,
#              PRIVATE, or False.
  prebuilts=False,

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

# child_configs -- A list of the child config groups, if applicable. See the
#                  add_group method.
  child_configs=[],

# shared_user_password -- Set shared user password for "chronos" user in built
#                         images. Use "None" (default) to remove the shared
#                         user password. Note that test images will always set
#                         the password to "test0000".
  shared_user_password=None,

# grouped -- Whether this config belongs to a config group.
  grouped=False,

# disk_layout -- layout of build_image resulting image.
#                See scripts/build_library/legacy_disk_layout.json or
#                overlay-<board>/scripts/disk_layout.json for possible values.
  disk_layout=None,

# postsync_patch -- If enabled, run the PatchChanges stage.  Enabled by default.
#                   Can be overridden by the --nopatch flag.
  postsync_patch=True,

# postsync_rexec -- Reexec into the buildroot after syncing.  Enabled by
#                   default.
  postsync_reexec=True,

# create_delta_sysroot -- Create delta sysroot during ArchiveStage. Disabled by
#                          default.
  create_delta_sysroot=False,

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

# Build the Chrome SDK.
  chrome_sdk=False,

# If chrome_sdk is set to True, this determines whether we attempt to build
# Chrome itself with the generated SDK.
  chrome_sdk_build_chrome=True,

# If chrome_sdk is set to True, this determines whether we use goma to build
# chrome.
  chrome_sdk_goma=False,

# image_test -- Run image tests. This should only be set if 'base' is in
# "images".
  image_test=False,

# =============================================================================
# The documentation associated with the config.
  doc=None,

# =============================================================================
# Hints to Buildbot master UI

# buildbot_waterfall_name -- If set, tells buildbot what name to give to the
# corresponding builder on its waterfall.
  buildbot_waterfall_name=None,

# active_waterfall -- If not None, the name (in constants.CIDB_KNOWN_WATERFALLS)
#                     of the waterfall that this target should be active on.
  active_waterfall=None,
)


class _JSONEncoder(json.JSONEncoder):
  """Json Encoder that encodes objects as their dictionaries."""
  # pylint: disable=E0202
  def default(self, obj):
    return self.encode(obj.__dict__)


class HWTestConfig(object):
  """Config object for hardware tests suites.

  Members:
    suite: Name of the test suite to run.
    timeout: Number of seconds to wait before timing out waiting for
             results.
    pool: Pool to use for hw testing.
    blocking: Suites that set this true run sequentially; each must pass
              before the next begins.  Tests that set this false run in
              parallel after all blocking tests have passed.
    async: Fire-and-forget suite.
    warn_only: Failure on HW tests warns only (does not generate error).
    critical: Usually we consider structural failures here as OK.
    priority:  Priority at which tests in the suite will be scheduled in
               the hw lab.
    file_bugs: Should we file bugs if a test fails in a suite run.
    num: Maximum number of DUTs to use when scheduling tests in the hw lab.
    minimum_duts: minimum number of DUTs required for testing in the hw lab.
    retry: Whether we should retry tests that fail in a suite run.
    max_retries: Integer, maximum job retries allowed at suite level.
                 None for no max.
    suite_min_duts: Preferred minimum duts. Lab will prioritize on getting such
                    number of duts even if the suite is competing with
                    other suites that have higher priority.

  Some combinations of member settings are invalid:
    * A suite config may not specify both blocking and async.
    * A suite config may not specify both retry and async.
    * A suite config may not specify both warn_only and critical.
  """

  # This timeout is larger than it needs to be because of autotest overhead.
  # TODO(davidjames): Reduce this timeout once http://crbug.com/366141 is fixed.
  DEFAULT_HW_TEST_TIMEOUT = 60 * 220
  BRANCHED_HW_TEST_TIMEOUT = 10 * 60 * 60
  # Number of tests running in parallel in the AU suite.
  AU_TESTS_NUM = 2
  # Number of tests running in parallel in the asynchronous canary
  # test suite
  ASYNC_TEST_NUM = 2

  @classmethod
  def DefaultList(cls, **kwargs):
    """Returns a default list of HWTestConfig's for a build

    Args:
      *kwargs: overrides for the configs
    """
    # Set the number of machines for the au and qav suites. If we are
    # constrained in the number of duts in the lab, only give 1 dut to each.
    if (kwargs.get('num', constants.HWTEST_DEFAULT_NUM) >=
        constants.HWTEST_DEFAULT_NUM):
      au_dict = dict(num=cls.AU_TESTS_NUM)
      async_dict = dict(num=cls.ASYNC_TEST_NUM)
    else:
      au_dict = dict(num=1)
      async_dict = dict(num=1)

    au_kwargs = kwargs.copy()
    au_kwargs.update(au_dict)

    async_kwargs = kwargs.copy()
    async_kwargs.update(async_dict)
    async_kwargs['priority'] = constants.HWTEST_POST_BUILD_PRIORITY
    async_kwargs['retry'] = False
    async_kwargs['max_retries'] = None
    async_kwargs['async'] = True
    async_kwargs['suite_min_duts'] = 1

    # BVT + AU suite.
    return [cls(constants.HWTEST_BVT_SUITE, blocking=True, **kwargs),
            cls(constants.HWTEST_AU_SUITE, blocking=True, **au_kwargs),
            cls(constants.HWTEST_COMMIT_SUITE, **async_kwargs),
            cls(constants.HWTEST_CANARY_SUITE, **async_kwargs)]

  @classmethod
  def DefaultListCanary(cls, **kwargs):
    """Returns a default list of HWTestConfig's for a canary build.

    Args:
      *kwargs: overrides for the configs
    """
    # Set minimum_duts default to 4, which means that lab will check the
    # number of available duts to meet the minimum requirement before creating
    # the suite job for canary builds.
    kwargs.setdefault('minimum_duts', 4)
    kwargs.setdefault('file_bugs', True)
    return cls.DefaultList(**kwargs)

  @classmethod
  def AFDOList(cls, **kwargs):
    """Returns a default list of HWTestConfig's for a AFDO build.

    Args:
      *kwargs: overrides for the configs
    """
    afdo_dict = dict(pool=constants.HWTEST_SUITES_POOL,
                     timeout=120 * 60, num=1, async=True, retry=False,
                     max_retries=None)
    afdo_dict.update(kwargs)
    return [cls('perf_v2', **afdo_dict)]

  @classmethod
  def DefaultListNonCanary(cls, **kwargs):
    """Return a default list of HWTestConfig's for a non-canary build.

    Optional arguments may be overridden in `kwargs`, except that
    the `blocking` setting cannot be provided.
    """
    return [cls(constants.HWTEST_BVT_SUITE, **kwargs),
            cls(constants.HWTEST_COMMIT_SUITE, **kwargs)]

  @classmethod
  def DefaultListCQ(cls, **kwargs):
    """Return a default list of HWTestConfig's for a CQ build.

    Optional arguments may be overridden in `kwargs`, except that
    the `blocking` setting cannot be provided.
    """
    default_dict = dict(pool=constants.HWTEST_PALADIN_POOL, timeout=120 * 60,
                        file_bugs=False, priority=constants.HWTEST_CQ_PRIORITY,
                        minimum_duts=4)
    # Allows kwargs overrides to default_dict for cq.
    default_dict.update(kwargs)
    return cls.DefaultListNonCanary(**default_dict)

  @classmethod
  def DefaultListPFQ(cls, **kwargs):
    """Return a default list of HWTestConfig's for a PFQ build.

    Optional arguments may be overridden in `kwargs`, except that
    the `blocking` setting cannot be provided.
    """
    default_dict = dict(pool=constants.HWTEST_PFQ_POOL, file_bugs=True,
                        priority=constants.HWTEST_PFQ_PRIORITY,
                        retry=False, max_retries=None, minimum_duts=4)
    # Allows kwargs overrides to default_dict for pfq.
    default_dict.update(kwargs)
    return cls.DefaultListNonCanary(**default_dict)

  @classmethod
  def SharedPoolPFQ(cls, **kwargs):
    """Return a list of HWTestConfigs for PFQ which uses a shared pool.

    The returned suites will run in pool:critical by default, which is
    shared with other types of builders (canaries, cq). The first suite in the
    list is a blocking sanity suite that verifies the build will not break dut.
    """
    sanity_dict = dict(pool=constants.HWTEST_MACH_POOL,
                       file_bugs=True, priority=constants.HWTEST_PFQ_PRIORITY,
                       retry=False, max_retries=None)
    sanity_dict.update(kwargs)
    sanity_dict.update(dict(num=1, minimum_duts=1, suite_min_duts=1,
                            blocking=True))
    default_dict = dict(pool=constants.HWTEST_MACH_POOL,
                        suite_min_duts=3)
    default_dict.update(kwargs)
    suite_list = [cls(constants.HWTEST_SANITY_SUITE, **sanity_dict)]
    suite_list.extend(cls.DefaultListPFQ(**default_dict))
    return suite_list

  @classmethod
  def SharedPoolCQ(cls, **kwargs):
    """Return a list of HWTestConfigs for CQ which uses a shared pool.

    The returned suites will run in pool:critical by default, which is
    shared with other types of builder (canaries, pfq). The first suite in the
    list is a blocking sanity suite that verifies the build will not break dut.
    """
    sanity_dict = dict(pool=constants.HWTEST_MACH_POOL, timeout=120 * 60,
                       file_bugs=False, priority=constants.HWTEST_CQ_PRIORITY)
    sanity_dict.update(kwargs)
    sanity_dict.update(dict(num=1, minimum_duts=1, suite_min_duts=1,
                            blocking=True))
    default_dict = dict(pool=constants.HWTEST_MACH_POOL,
                        suite_min_duts=10)
    default_dict.update(kwargs)
    suite_list = [cls(constants.HWTEST_SANITY_SUITE, **sanity_dict)]
    suite_list.extend(cls.DefaultListCQ(**default_dict))
    return suite_list

  @classmethod
  def SharedPoolCanary(cls, **kwargs):
    """Return a list of HWTestConfigs for Canary which uses a shared pool.

    The returned suites will run in pool:critical by default, which is
    shared with CQs. The first suite in the list is a blocking sanity suite
    that verifies the build will not break dut.
    """
    sanity_dict = dict(pool=constants.HWTEST_MACH_POOL, file_bugs=True)
    sanity_dict.update(kwargs)
    sanity_dict.update(dict(num=1, minimum_duts=1, suite_min_duts=1,
                            blocking=True))
    default_dict = dict(pool=constants.HWTEST_MACH_POOL,
                        suite_min_duts=6)
    default_dict.update(kwargs)
    suite_list = [cls(constants.HWTEST_SANITY_SUITE, **sanity_dict)]
    suite_list.extend(cls.DefaultListCanary(**default_dict))
    return suite_list


  def __init__(self, suite, num=constants.HWTEST_DEFAULT_NUM,
               pool=constants.HWTEST_MACH_POOL, timeout=DEFAULT_HW_TEST_TIMEOUT,
               async=False, warn_only=False, critical=False, blocking=False,
               file_bugs=False, priority=constants.HWTEST_BUILD_PRIORITY,
               retry=True, max_retries=10, minimum_duts=0, suite_min_duts=0):
    """Constructor -- see members above."""
    assert not async or (not blocking and not retry)
    assert not warn_only or not critical
    self.suite = suite
    self.num = num
    self.pool = pool
    self.timeout = timeout
    self.blocking = blocking
    self.async = async
    self.warn_only = warn_only
    self.critical = critical
    self.file_bugs = file_bugs
    self.priority = priority
    self.retry = retry
    self.max_retries = max_retries
    self.minimum_duts = minimum_duts
    self.suite_min_duts = suite_min_duts

  def SetBranchedValues(self):
    """Changes the HW Test timeout/priority values to branched values."""
    self.timeout = max(HWTestConfig.BRANCHED_HW_TEST_TIMEOUT, self.timeout)

    # Set minimum_duts default to 0, which means that lab will not check the
    # number of available duts to meet the minimum requirement before creating
    # a suite job for branched build.
    self.minimum_duts = 0

    # Only reduce priority if it's lower.
    new_priority = constants.HWTEST_DEFAULT_PRIORITY
    if (constants.HWTEST_PRIORITIES_MAP[self.priority] >
        constants.HWTEST_PRIORITIES_MAP[new_priority]):
      self.priority = new_priority

  @property
  def timeout_mins(self):
    return int(self.timeout / 60)

  def __eq__(self, other):
    return self.__dict__ == other.__dict__


def AFDORecordTest(**kwargs):
  default_dict = dict(pool=constants.HWTEST_SUITES_POOL,
                      warn_only=True, num=1, file_bugs=True,
                      timeout=constants.AFDO_GENERATE_TIMEOUT)
  # Allows kwargs overrides to default_dict for cq.
  default_dict.update(kwargs)
  return HWTestConfig(constants.HWTEST_AFDO_SUITE, **default_dict)


_delete_key_sentinel = object()
def delete_key():
  """Used to remove the given key from inherited config.

  Usage:
    new_config = base_config.derive(foo=delete_key())
  """
  return _delete_key_sentinel


def delete_keys(keys):
  """Used to remove a set of keys from inherited config.

  Usage:
    new_config = base_config.derive(delete_keys(set_of_keys))
  """
  return {k: delete_key() for k in keys}


def append_useflags(useflags):
  """Used to append a set of useflags to existing useflags.

  Useflags that shadow prior use flags will cause the prior flag to be removed.
  (e.g. appending '-foo' to 'foo' will cause 'foo' to be removed)

  Usage:
    new_config = base_config.derive(useflags=append_useflags(['foo', '-bar'])

  Args:
    useflags: List of string useflags to append.
  """
  assert isinstance(useflags, (list, set))
  shadowed_useflags = {'-' + flag for flag in useflags
                       if not flag.startswith('-')}
  shadowed_useflags.update({flag[1:] for flag in useflags
                            if flag.startswith('-')})
  def handler(old_useflags):
    new_useflags = set(old_useflags or [])
    new_useflags.update(useflags)
    new_useflags.difference_update(shadowed_useflags)
    return sorted(list(new_useflags))

  return handler


# TODO(mtennant): Rename this BuildConfig?
class _config(dict):
  """Dictionary of explicit configuration settings for a cbuildbot config

  Each dictionary entry is in turn a dictionary of config_param->value.

  See _settings for details on known configurations, and their documentation.
  """

  def __getattr__(self, name):
    """Support attribute-like access to each dict entry."""
    if name in self:
      return self[name]

    # Super class (dict) has no __getattr__ method, so use __getattribute__.
    return super(_config, self).__getattribute__(name)

  def GetBotId(self, remote_trybot=False):
    """Get the 'bot id' of a particular bot.

    The bot id is used to specify the subdirectory where artifacts are stored
    in Google Storage. To avoid conflicts between remote trybots and regular
    bots, we add a 'trybot-' prefix to any remote trybot runs.

    Args:
      remote_trybot: Whether this run is a remote trybot run.
    """
    return 'trybot-%s' % self.name if remote_trybot else self.name

  def derive(self, *args, **kwargs):
    """Create a new config derived from this one.

    Note: If an override is callable, it will be called and passed the prior
    value for the given key (or None) to compute the new value.

    Args:
      args: Mapping instances to mixin.
      kwargs: Settings to inject; see _settings for valid values.

    Returns:
      A new _config instance.
    """
    inherits = list(args)
    inherits.append(kwargs)
    new_config = copy.deepcopy(self)

    for update_config in inherits:
      for k, v in update_config.iteritems():
        if callable(v):
          new_config[k] = v(new_config.get(k))
        else:
          new_config[k] = v

      keys_to_delete = [k for k in new_config if
                        new_config[k] is _delete_key_sentinel]

      for k in keys_to_delete:
        new_config.pop(k, None)

    return copy.deepcopy(new_config)

  def add_config(self, name, *args, **kwargs):
    """Derive and add the config to cbuildbot's usable config targets

    Args:
      name: The name to label this configuration; this is what cbuildbot
            would see.
      args: See the docstring of derive.
      kwargs: See the docstring of derive.

    Returns:
      See the docstring of derive.
    """
    inherits, overrides = args, kwargs
    overrides['name'] = name
    new_config = self.derive(*inherits, **overrides)

    # Derive directly from defaults so missing values are added.
    # Store a dictionary, rather than our derivative- this is
    # to ensure any far flung consumers of the config dictionary
    # aren't affected by recent refactorings.

    config_dict = default.derive(self, new_config)

    # TODO(mtennant): This is just confusing.  Some random _config object
    # (self) can add a new _config object to the global config dict.  Even if
    # self is itself not a part of the global config dict.
    config[name] = config_dict

    return new_config

  @classmethod
  def add_raw_config(cls, name, *args, **kwargs):
    return cls().add_config(name, *args, **kwargs)

  @classmethod
  def add_group(cls, name, *args, **kwargs):
    """Create a new group of build configurations.

    Args:
      name: The name to label this configuration; this is what cbuildbot
            would see.
      args: Configurations to build in this group. The first config in
            the group is considered the primary configuration and is used
            for syncing and creating the chroot.

    Returns:
      A new _config instance.
    """
    child_configs = [default.derive(x, grouped=True) for x in args]
    return args[0].add_config(name, child_configs=child_configs, **kwargs)

default = _config(**_settings)


# Arch-specific mixins.

# Config parameters for builders that do not run tests on the builder. Anything
# non-x86 tests will fall under this category.
non_testable_builder = _config(
  tests_supported=False,
  unittests=False,
  vm_tests=[],
)


# Builder-specific mixins

binary = _config(
  # Full builds that build fully from binaries.
  build_type=constants.BUILD_FROM_SOURCE_TYPE,
  archive_build_debug=True,
  images=['test', 'factory_install'],
  git_sync=True,
)

full = _config(
  # Full builds are test builds to show that we can build from scratch,
  # so use settings to build from scratch, and archive the results.

  usepkg_build_packages=False,
  chrome_sdk=True,
  chroot_replace=True,

  build_type=constants.BUILD_FROM_SOURCE_TYPE,
  archive_build_debug=True,
  images=['base', 'test', 'factory_install'],
  git_sync=True,
  trybot_list=True,
  description='Full Builds',
  image_test=True,
  doc='http://www.chromium.org/chromium-os/build/builder-overview#'
      'TOC-Continuous',
)

# Full builders with prebuilts.
full_prebuilts = full.derive(
  prebuilts=constants.PUBLIC,
)

pfq = _config(
  build_type=constants.PFQ_TYPE,
  important=True,
  uprev=True,
  overlays=constants.PUBLIC_OVERLAYS,
  manifest_version=True,
  trybot_list=True,
  doc='http://www.chromium.org/chromium-os/build/builder-overview#'
      'TOC-Chrome-PFQ',
)

paladin = _config(
  important=True,
  build_type=constants.PALADIN_TYPE,
  overlays=constants.PUBLIC_OVERLAYS,
  prebuilts=constants.PUBLIC,
  manifest_version=True,
  trybot_list=True,
  description='Commit Queue',
  upload_standalone_images=False,
  images=['test'],
  chrome_sdk=True,
  chrome_sdk_build_chrome=False,
  doc='http://www.chromium.org/chromium-os/build/builder-overview#TOC-CQ',
)

# Incremental builders are intended to test the developer workflow.
# For that reason, they don't uprev.
incremental = _config(
  build_type=constants.INCREMENTAL_TYPE,
  uprev=False,
  overlays=constants.PUBLIC_OVERLAYS,
  description='Incremental Builds',
  doc='http://www.chromium.org/chromium-os/build/builder-overview#'
      'TOC-Continuous',
)

# This builds with more source available.
internal = _config(
  internal=True,
  overlays=constants.BOTH_OVERLAYS,
  manifest_repo_url=constants.MANIFEST_INT_URL,
)

brillo = _config(
  sync_chrome=False,
  chrome_sdk=False,
  # TODO(gauravsh): crbug.com/356414 Start running tests on Brillo configs.
  vm_tests=[],
  hw_tests=[],
)

moblab = _config(
  vm_tests=[],
  hw_tests=[],
)

# Builds for the Project SDK.
project_sdk = _config(
  build_type=constants.PROJECT_SDK_TYPE,
  description='Produce Project SDK build artifacts.',

  profile='minimal',

  # These are test builds, they shouldn't break anything (yet).
  important=False,

  usepkg_build_packages=False,
  sync_chrome=False,
  chrome_sdk=False,
  uprev=False,

  # Use the SDK manifest published by the Canary master for most builds.
  lkgm_manifest=constants.LATEST_PROJECT_SDK_MANIFEST,
  use_lkgm=True,

  # Tests probably don't work yet.
  vm_tests=[],
  hw_tests=[],

  # Factory stuff not needed here.
  factory_install_netboot=False,
  factory_toolkit=False,
  factory=False,
)

_project_sdk_boards = frozenset([
    'panther_embedded',
])

beaglebone = brillo.derive(non_testable_builder, rootfs_verification=False)

# This adds Chrome branding.
official_chrome = _config(
  useflags=[constants.USE_CHROME_INTERNAL],
)

# This sets chromeos_official.
official = official_chrome.derive(
  chromeos_official=True,
)

_cros_sdk = full_prebuilts.add_config('chromiumos-sdk',
  # The amd64-host has to be last as that is when the toolchains
  # are bundled up for inclusion in the sdk.
  boards=('x86-generic', 'arm-generic', 'amd64-generic'),
  build_type=constants.CHROOT_BUILDER_TYPE,
  use_sdk=False,
  trybot_list=True,
  description='Build the SDK and all the cross-compilers',
  doc='http://www.chromium.org/chromium-os/build/builder-overview#'
      'TOC-Continuous',
)

asan = _config(
  chroot_replace=True,
  profile='asan',
  disk_layout='2gb-rootfs',
  # TODO(deymo): ASan builders generate bigger files, in particular a bigger
  # Chrome binary, that update_engine can't handle in delta payloads due to
  # memory limits. Remove the following line once crbug.com/329248 is fixed.
  vm_tests=[constants.SMOKE_SUITE_TEST_TYPE],
  doc='http://www.chromium.org/chromium-os/build/builder-overview#'
      'TOC-ChromiumOS-SDK',
)

telemetry = _config(
  build_type=constants.INCREMENTAL_TYPE,
  uprev=False,
  overlays=constants.PUBLIC_OVERLAYS,
  vm_tests=[constants.TELEMETRY_SUITE_TEST_TYPE],
  description='Telemetry Builds',
)

chromium_pfq = _config(
  build_type=constants.CHROME_PFQ_TYPE,
  important=True,
  uprev=False,
  overlays=constants.PUBLIC_OVERLAYS,
  manifest_version=True,
  chrome_rev=constants.CHROME_REV_LATEST,
  chrome_sdk=True,
  chroot_replace=True,
  description='Preflight Chromium Uprev & Build (public)',
)

# TODO(davidjames): Convert this to an external config once the unified master
# logic is ready.
internal_chromium_pfq = internal.derive(
  chromium_pfq,
  description='Preflight Chromium Uprev & Build (internal)',
  overlays=constants.BOTH_OVERLAYS,
  prebuilts=constants.PUBLIC,
)

internal_chromium_pfq.add_config('master-chromium-pfq',
  boards=[],
  master=True,
  push_overlays=constants.BOTH_OVERLAYS,
  afdo_update_ebuild=True,
  chrome_sdk=False,
)

chrome_pfq = internal_chromium_pfq.derive(
  official,
  important=True,
  overlays=constants.BOTH_OVERLAYS,
  description='Preflight Chrome Uprev & Build (internal)',
  prebuilts=constants.PRIVATE,
)

chrome_try = _config(
  build_type=constants.CHROME_PFQ_TYPE,
  chrome_rev=constants.CHROME_REV_TOT,
  use_lkgm=True,
  important=False,
  manifest_version=False,
)

chromium_info = chromium_pfq.derive(
  chrome_try,
  vm_tests=[constants.SMOKE_SUITE_TEST_TYPE],
  chrome_sdk=False,
  description='Informational Chromium Uprev & Build (public)',
)

telemetry_info = telemetry.derive(
  chrome_try,
)

chrome_info = chromium_info.derive(
  internal, official,
  description='Informational Chrome Uprev & Build (internal)',
)

chrome_perf = chrome_info.derive(
  description='Chrome Performance test bot',
  vm_tests=[],
  unittests=False,
  hw_tests=[HWTestConfig('perf_v2', pool=constants.HWTEST_CHROME_PERF_POOL,
                         timeout=90 * 60, critical=True, num=1)],
  use_chrome_lkgm=True,
  use_lkgm=False,
  useflags=append_useflags(['-cros-debug']),
)

# Base per-board configuration.
# Every board must appear in exactly 1 of the following sets.

_arm_internal_release_boards = frozenset([
  'daisy',
  'daisy_freon',
  'daisy_skate',
  'daisy_spring',
  'daisy_winter',
  'kayle',
  'nyan',
  'nyan_big',
  'nyan_blaze',
  'nyan_freon',
  'nyan_kitty',
  'peach_pi',
  'peach_pit',
  'purin',
  'storm',
  'rush',
  'rush_ryu',
  'veyron_brain',
  'veyron_danger',
  'veyron_gus',
  'veyron_jaq',
  'veyron_jerry',
  'veyron_mighty',
  'veyron_minnie',
  'veyron_pinky',
  'veyron_rialto',
  'veyron_speedy',
  'whirlwind',
])

_arm_external_boards = frozenset([
  'arm-generic',
  'arm-generic_freon',
  'arm64-generic',
])

_x86_internal_release_boards = frozenset([
  'auron',
  'auron_paine',
  'auron_yuna',
  'bayleybay',
  'banjo',
  'beltino',
  'bobcat',
  'butterfly',
  'butterfly_freon',
  'candy',
  'candy_freon',
  'cid',
  'cranky',
  'clapper',
  'clapper_freon',
  'enguarde',
  'enguarde_freon',
  'expresso',
  'expresso_freon',
  'falco',
  'falco_li',
  'glimmer',
  'glimmer_freon',
  'gnawty',
  'gnawty_freon',
  'guado',
  'jecht',
  'kip',
  'kip_freon',
  'lemmings',
  'leon',
  'link',
  'lulu',
  'lumpy',
  'lumpy_freon',
  'mccloud',
  'monroe',
  'panther',
  'panther_moblab',
  'parrot',
  'parrot_freon',
  'parrot_ivb',
  'parry',
  'peppy',
  'peppy_freon',
  'quawks',
  'quawks_freon',
  'rambi',
  'rambi_freon',
  'rikku',
  'samus',
  'slippy',
  'squawks',
  'squawks_freon',
  'stout',
  'stout_freon',
  'strago',
  'stumpy',
  'stumpy_freon',
  'stumpy_moblab',
  'swanky',
  'swanky_freon',
  'tidus',
  'tricky',
  'winky',
  'winky_freon',
  'wolf',
  'x86-alex',
  'x86-alex_he',
  'x86-mario',
  'x86-zgb',
  'x86-zgb_he',
  'zako',
])

_x86_external_boards = frozenset([
  'amd64-generic',
  'amd64-generic_freon',
  'gizmo',
  'x32-generic',
  'x86-generic',
  'x86-pineview',
])

_mips_internal_release_boards = frozenset([
  'urara',
])

_mips_external_boards = frozenset([
  'mipseb-n32-generic',
  'mipseb-n64-generic',
  'mipseb-o32-generic',
  'mipsel-n32-generic',
  'mipsel-n64-generic',
  'mipsel-o32-generic',
])

# Every board should be in only 1 of the above sets.
_distinct_board_sets = [
    _arm_internal_release_boards,
    _arm_external_boards,
    _x86_internal_release_boards,
    _x86_external_boards,
    _mips_internal_release_boards,
    _mips_external_boards,
]

_arm_full_boards = (_arm_internal_release_boards |
                    _arm_external_boards)
_x86_full_boards = (_x86_internal_release_boards |
                    _x86_external_boards)
_mips_full_boards = (_mips_internal_release_boards |
                     _mips_external_boards)

_arm_boards = _arm_full_boards
_x86_boards = _x86_full_boards
_mips_boards = _mips_full_boards

_all_release_boards = (
    _arm_internal_release_boards |
    _x86_internal_release_boards |
    _mips_internal_release_boards
)
_all_full_boards = (
    _arm_full_boards |
    _x86_full_boards |
    _mips_full_boards
)
_all_boards = (
    _x86_boards |
    _arm_boards |
    _mips_boards
)

# Every board should be in exactly one of the distinct board sets.
def _EnforceDistinctSets():
  for board in _all_boards:
    found = False
    for s in _distinct_board_sets:
      if board in s:
        if found:
          assert False, '%s in multiple board sets.' % board
        else:
          found = True
    if not found:
      assert False, '%s in no board sets' % board
  for s in _distinct_board_sets:
    for board in s - _all_boards:
      assert False, '%s in _distinct_board_sets but not in _all_boards' % board

_EnforceDistinctSets()

_arm_release_boards = _arm_internal_release_boards
_x86_release_boards = _x86_internal_release_boards
_mips_release_boards = _mips_internal_release_boards

_internal_boards = _all_release_boards

# Board can appear in 1 or more of the following sets.
_brillo_boards = frozenset([
  'cosmos',
  'gizmo',
  'kayle',
  'lemmings',
  'panther_embedded',
  'purin',
  'storm',
  'urara',
  'whirlwind',
])

_moblab_boards = frozenset([
  'stumpy_moblab',
  'panther_moblab',
])

_freon_boards = frozenset([
  'auron',
  'auron_paine',
  'auron_yuna',
  'butterfly_freon',
  'candy_freon',
  'cid',
  'clapper_freon',
  'enguarde_freon',
  'expresso_freon',
  'glimmer_freon',
  'guado',
  'gnawty_freon',
  'jecht',
  'kip_freon',
  'link',
  'lumpy_freon',
  'lulu',
  'mccloud',
  'monroe',
  'panther',
  'parrot_freon',
  'peppy',
  'peppy_freon',
  'rambi_freon',
  'strago',
  'stumpy_freon',
  'quawks_freon',
  'rikku',
  'stout_freon',
  'squawks_freon',
  'swanky_freon',
  'tidus',
  'tricky',
  'tricky_freon',
  'zako',
  'samus',
])

_minimal_profile_boards = frozenset([
  'bobcat',
])

_nofactory_boards = frozenset([
  'daisy_winter',
])

_toolchains_from_source = frozenset([
  'mipseb-n32-generic',
  'mipseb-n64-generic',
  'mipseb-o32-generic',
  'mipsel-n32-generic',
  'mipsel-n64-generic',
  'x32-generic',
])

# A base config for each board.
_base_configs = dict()

def _CreateBaseConfigs():
  for board in _all_boards:
    base = _config()
    if board in _internal_boards:
      base.update(internal)
      base.update(official_chrome)
      base.update(manifest=constants.OFFICIAL_MANIFEST)
    if board not in _x86_boards:
      base.update(non_testable_builder)
    if board in _brillo_boards:
      base.update(brillo)
    if board in _moblab_boards:
      base.update(moblab)
    if board in _minimal_profile_boards:
      base.update(profile='minimal')
    if board in _nofactory_boards:
      base.update(factory=False)
    if board in _freon_boards:
      base.update(vm_tests=[])
    if board in _toolchains_from_source:
      base.update(usepkg_toolchain=False)

    # TODO(akeshet) Eliminate or clean up this special case.
    # kayle board has a lot of kayle-specific config changes.
    if board == 'kayle':
      base.update(manifest='kayle.xml',
                  dev_manifest='kayle.xml',
                  factory_toolkit=False,
                  # TODO(namnguyen): Cannot build factory net install (no
                  # usbnet).
                  factory_install_netboot=False,
                  # TODO(namngyuyen) Cannot build dev or test images due to
                  # #436523.
                  images=['base'])

    board_config = base.derive(boards=[board])
    # Note: base configs should not specify a useflag list. Convert any useflags
    # that this base config has accrued (for instance, 'chrome_internal', via
    # official_chrome) into an append_useflags callable. This
    # is because the board base config is the last config to be derived from
    # when creating a board-specific config,
    if 'useflags' in board_config:
      board_config['useflags'] = append_useflags(board_config['useflags'])
    _base_configs[board] = board_config

_CreateBaseConfigs()

def _CreateConfigsForBoards(config_base, boards, name_suffix, **kwargs):
  """Create configs based on |config_base| for all boards in |boards|.

  Note: Existing configs will not be overwritten.

  Args:
    config_base: A _config instance to inherit from.
    boards: A set of boards to create configs for.
    name_suffix: A naming suffix. Configs will have names of the form
                 board-name_suffix.
    **kwargs: Additional keyword arguments to be used in add_config.
  """
  for board in boards:
    config_name = '%s-%s' % (board, name_suffix)
    if config_name not in config:
      base = _config()
      config_base.add_config(config_name, base, _base_configs[board], **kwargs)

_chromium_pfq_important_boards = frozenset([
  'arm-generic_freon',
  'arm-generic',
  'daisy',
  'mipsel-o32-generic',
  'x86-generic',
  ])

def _AddFullConfigs():
  """Add x86 and arm full configs."""
  external_overrides = delete_keys(internal)
  external_overrides.update(manifest=delete_key())
  external_overrides.update(
    useflags=append_useflags(['-%s' % constants.USE_CHROME_INTERNAL]))
  _CreateConfigsForBoards(full_prebuilts, _all_full_boards, CONFIG_TYPE_FULL,
                          **external_overrides)
  _CreateConfigsForBoards(chromium_info, _all_full_boards,
                          'tot-chromium-pfq-informational', important=False,
                          **external_overrides)
  # Create important configs, then non-important configs.
  _CreateConfigsForBoards(internal_chromium_pfq, _chromium_pfq_important_boards,
                          'chromium-pfq', **external_overrides)
  _CreateConfigsForBoards(internal_chromium_pfq, _all_full_boards,
                          'chromium-pfq', important=False,
                          **external_overrides)

_AddFullConfigs()


# These remaining chromium pfq configs have eccentricities that are easier to
# create manually.

internal_chromium_pfq.add_config('amd64-generic-chromium-pfq',
  _base_configs['amd64-generic'],
  disk_layout='2gb-rootfs',
)

internal_chromium_pfq.add_config('amd64-generic_freon-chromium-pfq',
  _base_configs['amd64-generic_freon'],
  disk_layout='2gb-rootfs',
  vm_tests=[],
)

_chrome_pfq_important_boards = frozenset([
  'peppy',
  'peppy_freon',
  'rush_ryu',
  'veyron_pinky',
  ])


# TODO(akeshet): Replace this with a config named x86-alex-chrome-pfq.
chrome_pfq.add_config('alex-chrome-pfq',
  _base_configs['x86-alex'],
)

chrome_pfq.add_config('lumpy-chrome-pfq',
  _base_configs['lumpy'],
  afdo_generate=True,
  hw_tests=[AFDORecordTest()] + HWTestConfig.SharedPoolPFQ(),
)

chrome_pfq.add_config('daisy_skate-chrome-pfq',
  _base_configs['daisy_skate'],
  hw_tests=HWTestConfig.SharedPoolPFQ(),
)

chrome_pfq.add_config('falco-chrome-pfq',
  _base_configs['falco'],
  hw_tests=HWTestConfig.SharedPoolPFQ(),
)

chrome_pfq.add_config('peach_pit-chrome-pfq',
  _base_configs['peach_pit'],
  hw_tests=HWTestConfig.SharedPoolPFQ(),
  important=False,
)

_telemetry_boards = frozenset([
    'amd64-generic',
    'arm-generic',
    'x86-generic',
])

_CreateConfigsForBoards(telemetry, _telemetry_boards, 'telemetry')

_toolchain_major = _cros_sdk.add_config('toolchain-major',
  latest_toolchain=True,
  prebuilts=False,
  trybot_list=False,
  gcc_githash='svn-mirror/google/main',
  description='Test next major toolchain revision',
)

_toolchain_minor = _cros_sdk.add_config('toolchain-minor',
  latest_toolchain=True,
  prebuilts=False,
  trybot_list=False,
  gcc_githash='svn-mirror/google/gcc-4_9',
  description='Test next minor toolchain revision',
)

incremental.add_config('x86-generic-asan',
  asan,
  boards=['x86-generic'],
  description='Build with Address Sanitizer (Clang)',
  trybot_list=True,
)

chromium_info.add_config('x86-generic-tot-asan-informational',
  asan,
  boards=['x86-generic'],
  description='Full build with Address Sanitizer (Clang) on TOT',
)

incremental.add_config('amd64-generic-asan',
  asan,
  boards=['amd64-generic'],
  description='Build with Address Sanitizer (Clang)',
  trybot_list=True,
)

chromium_info.add_config('amd64-generic-tot-asan-informational',
  asan,
  boards=['amd64-generic'],
  description='Build with Address Sanitizer (Clang) on TOT',
)

incremental_beaglebone = incremental.derive(beaglebone)
incremental_beaglebone.add_config('beaglebone-incremental',
  boards=['beaglebone'],
  trybot_list=True,
  description='Incremental Beaglebone Builder',
)

_config.add_raw_config('refresh-packages',
  boards=['x86-generic', 'arm-generic'],
  build_type=constants.REFRESH_PACKAGES_TYPE,
  description='Check upstream Gentoo for package updates',
)

incremental.add_config('x86-generic-incremental',
  _base_configs['x86-generic'],
)

incremental.add_config('daisy-incremental',
  _base_configs['daisy'],
  delete_keys(internal),
  manifest=delete_key(),
  useflags=append_useflags(['-chrome_internal']),
)

incremental.add_config('amd64-generic-incremental',
  _base_configs['amd64-generic'],
  # This builder runs on a VM, so it can't run VM tests.
  vm_tests=[],
)

incremental.add_config('x32-generic-incremental',
  _base_configs['x32-generic'],
  # This builder runs on a VM, so it can't run VM tests.
  vm_tests=[],
)

paladin.add_config('x86-generic-asan-paladin',
  _base_configs['x86-generic'],
  asan,
  description='Paladin build with Address Sanitizer (Clang)',
  important=False,
)

incremental.add_config('amd64-generic-asan-paladin',
  _base_configs['amd64-generic'],
  asan,
  description='Paladin build with Address Sanitizer (Clang)',
  important=False,
)

_chrome_perf_boards = frozenset([
  'daisy',
  'lumpy',
  'parrot',
])

_CreateConfigsForBoards(chrome_perf, _chrome_perf_boards, 'chrome-perf',
                        trybot_list=True)

chromium_info_x86 = \
chromium_info.add_config('x86-generic-tot-chrome-pfq-informational',
  boards=['x86-generic'],
)

chromium_info_daisy = \
chromium_info.add_config('daisy-tot-chrome-pfq-informational',
  non_testable_builder,
  boards=['daisy'],
)

chromium_info_amd64 = \
chromium_info.add_config('amd64-generic-tot-chrome-pfq-informational',
  boards=['amd64-generic'],
)

chromium_info.add_config('x32-generic-tot-chrome-pfq-informational',
  boards=['x32-generic'],
)

_CreateConfigsForBoards(telemetry_info, ['x86-generic', 'amd64-generic'],
                        'telem-chrome-pfq-informational')

chrome_info.add_config('alex-tot-chrome-pfq-informational',
  boards=['x86-alex'],
)

chrome_info.add_config('lumpy-tot-chrome-pfq-informational',
  boards=['lumpy'],
)

# WebRTC configurations.
chrome_info.add_config('alex-webrtc-chrome-pfq-informational',
  boards=['x86-alex'],
)
chrome_info.add_config('lumpy-webrtc-chrome-pfq-informational',
  boards=['lumpy'],
)
chrome_info.add_config('daisy-webrtc-chrome-pfq-informational',
  non_testable_builder,
  boards=['daisy'],
)
chromium_info_x86.add_config('x86-webrtc-chromium-pfq-informational',
  archive_build_debug=True,
)
chromium_info_amd64.add_config('amd64-webrtc-chromium-pfq-informational',
  archive_build_debug=True,
)
chromium_info_daisy.add_config('daisy-webrtc-chromium-pfq-informational',
  archive_build_debug=True,
)


#
# Internal Builds
#

internal_pfq = internal.derive(official_chrome, pfq,
  overlays=constants.BOTH_OVERLAYS,
  prebuilts=constants.PRIVATE,
)

# Because branch directories may be shared amongst builders on multiple
# branches, they must delete the chroot every time they run.
# They also potentially need to build [new] Chrome.
internal_pfq_branch = internal_pfq.derive(branch=True, chroot_replace=True,
                                          trybot_list=False, sync_chrome=True)

internal_paladin = internal.derive(official_chrome, paladin,
  manifest=constants.OFFICIAL_MANIFEST,
  overlays=constants.BOTH_OVERLAYS,
  prebuilts=constants.PRIVATE,
  vm_tests=[],
  description=paladin['description'] + ' (internal)',
)

# Used for paladin builders with nowithdebug flag (a.k.a -cros-debug)
internal_nowithdebug_paladin = internal_paladin.derive(
  useflags=append_useflags(['-cros-debug']),
  description=paladin['description'] + ' (internal, nowithdebug)',
  prebuilts=False,
)

_CreateConfigsForBoards(internal_nowithdebug_paladin,
  ['x86-generic', 'amd64-generic'],
  'nowithdebug-paladin',
  important=False,
)

internal_nowithdebug_paladin.add_config('x86-mario-nowithdebug-paladin',
  boards=['x86-mario'],
)

# Used for builders which build completely from source except Chrome.
full_compile_paladin = paladin.derive(
  board_replace=True,
  chrome_binhost_only=True,
  chrome_sdk=False,
  cpe_export=False,
  debug_symbols=False,
  prebuilts=False,
  unittests=False,
  upload_hw_test_artifacts=False,
  vm_tests=[],
)

_CreateConfigsForBoards(full_compile_paladin,
  ['falco', 'nyan'],
  'full-compile-paladin',
)

pre_cq = paladin.derive(
  build_type=constants.INCREMENTAL_TYPE,
  build_packages_in_background=True,
  pre_cq=True,
  archive=False,
  chrome_sdk=False,
  chroot_replace=True,
  debug_symbols=False,
  prebuilts=False,
  cpe_export=False,
  vm_tests=[constants.SMOKE_SUITE_TEST_TYPE],
  description='Verifies compilation, building an image, and vm/unit tests '
              'if supported.',
  doc='http://www.chromium.org/chromium-os/build/builder-overview#TOC-Pre-CQ',
  health_alert_recipients=['chromeos-infra-eng@grotations.appspotmail.com'],
)

# Pre-CQ targets that only check compilation and unit tests.
unittest_only_pre_cq = pre_cq.derive(
  description='Verifies compilation and unit tests only',
  compilecheck=True,
  vm_tests=[],
)

# Pre-CQ targets that don't run VMTests.
no_vmtest_pre_cq = pre_cq.derive(
  description='Verifies compilation, building an image, and unit tests '
              'if supported.',
  vm_tests=[],
)

# Pre-CQ targets that only check compilation.
compile_only_pre_cq = unittest_only_pre_cq.derive(
  description='Verifies compilation only',
  unittests=False,
)

internal_paladin.add_config(constants.BRANCH_UTIL_CONFIG,
  boards=[],
  # Disable postsync_patch to prevent conflicting patches from being applied -
  # e.g., patches from 'master' branch being applied to a branch.
  postsync_patch=False,
  # Disable postsync_reexec to continue running the 'master' branch chromite
  # for all stages, rather than the chromite in the branch buildroot.
  postsync_reexec=False,
  build_type=constants.CREATE_BRANCH_TYPE,
  description='Used for creating/deleting branches (TPMs only)',
)

# Internal incremental builders don't use official chrome because we want
# to test the developer workflow.
internal_incremental = internal.derive(
  incremental,
  overlays=constants.BOTH_OVERLAYS,
  description='Incremental Builds (internal)',
)

internal_pfq_branch.add_config('lumpy-pre-flight-branch',
  master=True,
  push_overlays=constants.BOTH_OVERLAYS,
  boards=['lumpy'],
  afdo_generate=True,
  afdo_update_ebuild=True,
  hw_tests=[AFDORecordTest()],
)

# A test-ap image is just a test image with a special profile enabled.
# Note that each board enabled for test-ap use has to have the testbed-ap
# profile linked to from its private overlay.
_test_ap = internal.derive(
  description='WiFi AP images used in testing',
  profile='testbed-ap',
  vm_tests=[],
)

_config.add_group('test-ap-group',
  _test_ap.add_config('stumpy-test-ap', boards=['stumpy']),
  _test_ap.add_config('panther-test-ap', boards=['panther']),
)

### Master paladin (CQ builder).

internal_paladin.add_config('master-paladin',
  boards=[],
  master=True,
  push_overlays=constants.BOTH_OVERLAYS,
  description='Commit Queue master (all others are slaves)',

  # This name should remain synced with with the name used in
  # build_internals/masters/master.chromeos/board_config.py.
  # TODO(mtennant): Fix this.  There should be some amount of auto-
  # configuration in the board_config.py code.
  health_threshold=3,
  health_alert_recipients=['chromeos-infra-eng@grotations.appspotmail.com',
                           'tree'],
  sanity_check_slaves=['wolf-tot-paladin'],
  trybot_list=False,
)

### Other paladins (CQ builders).
# These are slaves of the master paladin by virtue of matching
# in a few config values (e.g. 'build_type', 'branch', etc).  If
# they are not 'important' then they are ignored slaves.
# TODO(mtennant): This master-slave relationship should be specified
# here in the configuration, rather than GetSlavesForMaster().
# Something like the following:
# master_paladin = internal_paladin.add_config(...)
# master_paladin.AddSlave(internal_paladin.add_config(...))

# Old sanity check builder. This has been replaced by wolf-tot-paladin.
# TODO(dnj): Remove this once wolf-tot-paladin is removed from the waterfall.
internal_paladin.add_config('link-tot-paladin',
  boards=['link'],
  do_not_apply_cq_patches=True,
  prebuilts=False,
  important=False,
)

# Sanity check builder, part of the CQ but builds without the patches
# under test.
internal_paladin.add_config('wolf-tot-paladin',
  boards=['wolf'],
  do_not_apply_cq_patches=True,
  prebuilts=False,
  hw_tests=HWTestConfig.SharedPoolCQ(),
)

_paladin_boards = _all_boards

# List of paladin boards where the regular paladin config is important.
_paladin_important_boards = frozenset([
  'amd64-generic',
  'arm-generic',
  'auron',
  'beaglebone',
  'butterfly',
  'daisy',
  'daisy_spring',
  'falco',
  'gizmo',
  'kayle',
  'leon',
  'link',
  'lumpy',
  'mipsel-o32-generic',
  'monroe',
  'nyan',
  'panther',
  'panther_moblab',
  'parrot',
  'peach_pit',
  'peppy',
  'peppy_freon',
  'rambi',
  'rush_ryu',
  'samus',
  'storm',
  'stout',
  'stumpy',
  'stumpy_moblab',
  'urara',
  'veyron_pinky',
  'wolf',
  'x86-alex',
  'x86-generic',
  'x86-mario',
  'x86-zgb',
])

_paladin_simple_vmtest_boards = frozenset([
  'rambi',
  'x86-mario',
])

_paladin_devmode_vmtest_boards = frozenset([
  'parrot',
])

_paladin_cros_vmtest_boards = frozenset([
  'stout',
])

_paladin_smoke_vmtest_boards = frozenset([
  'amd64-generic',
  'x86-generic',
])

_paladin_default_vmtest_boards = frozenset([
  'x32-generic',
])

_paladin_hwtest_boards = frozenset([
  'daisy',
  'link',
  'lumpy',
  'peach_pit',
  'peppy',
  'stumpy',
  'wolf',
  'x86-alex',
  'x86-zgb',
])

_paladin_chroot_replace_boards = frozenset([
  'butterfly',
  'daisy_spring',
])

_paladin_separate_symbols = frozenset([
  'amd64-generic',
  'gizmo',
])

def _CreatePaladinConfigs():
  for board in _paladin_boards:
    assert board in _base_configs, '%s not in _base_configs' % board
    config_name = '%s-%s' % (board, constants.PALADIN_TYPE)
    customizations = _config()
    base_config = _base_configs[board]
    if board in _paladin_hwtest_boards:
      customizations.update(hw_tests=HWTestConfig.DefaultListCQ())
    if board not in _paladin_important_boards:
      customizations.update(important=False)
    if board in _paladin_chroot_replace_boards:
      customizations.update(chroot_replace=True)
    if board in _internal_boards:
      customizations = customizations.derive(
          internal, official_chrome,
          manifest=constants.OFFICIAL_MANIFEST)
    if board in _paladin_separate_symbols:
      customizations.update(separate_debug_symbols=True)

    if board not in _paladin_default_vmtest_boards:
      vm_tests = []
      if board in _paladin_simple_vmtest_boards:
        vm_tests.append(constants.SIMPLE_AU_TEST_TYPE)
      if board in _paladin_cros_vmtest_boards:
        vm_tests.append(constants.CROS_VM_TEST_TYPE)
      if board in _paladin_devmode_vmtest_boards:
        vm_tests.append(constants.DEV_MODE_TEST_TYPE)
      if board in _paladin_smoke_vmtest_boards:
        vm_tests.append(constants.SMOKE_SUITE_TEST_TYPE)
      customizations.update(vm_tests=vm_tests)

    if base_config.get('internal'):
      customizations.update(prebuilts=constants.PRIVATE,
                            description=paladin['description'] + ' (internal)')
    else:
      customizations.update(prebuilts=constants.PUBLIC)
    paladin.add_config(config_name,
                       customizations,
                       base_config)

_CreatePaladinConfigs()


internal_paladin.add_config('lumpy-incremental-paladin',
  boards=['lumpy'],
  build_before_patching=True,
  chroot_replace=False,
  prebuilts=False,
  compilecheck=True,
  unittests=False,
)

### Paladins (CQ builders) which do not run VM or Unit tests on the builder
### itself.
external_brillo_paladin = paladin.derive(brillo)

external_brillo_paladin.add_config('panther_embedded-minimal-paladin',
  boards=['panther_embedded'],
  profile='minimal',
  trybot_list=True,
)

internal_beaglebone_paladin = internal_paladin.derive(beaglebone)

internal_beaglebone_paladin.add_config('beaglebone-paladin',
  boards=['beaglebone'],
  trybot_list=True,
)

internal_beaglebone_paladin.add_config('beaglebone_servo-paladin',
  boards=['beaglebone_servo'],
  important=False,
)


def ShardHWTestsBetweenBuilders(*args):
  """Divide up the hardware tests between the given list of config names.

  Each of the config names must have the same hardware test suites, and the
  number of suites must be equal to the number of config names.

  Args:
    *args: A list of config names.
  """
  # List of config names.
  names = args
  # Verify sanity before sharding the HWTests.
  for name in names:
    assert len(config[name].hw_tests) == len(names), \
      '%s should have %d tests, but found %d' % (
          name, len(names), len(config[name].hw_tests))
  for name in names[1:]:
    for test1, test2 in zip(config[name].hw_tests, config[names[0]].hw_tests):
      assert test1.__dict__ == test2.__dict__, \
          '%s and %s have different hw_tests configured' % (names[0], name)

  # Assign each config the Nth HWTest.
  for i, name in enumerate(names):
    config[name]['hw_tests'] = [config[name].hw_tests[i]]

# Shard the bvt-inline and bvt-cq hw tests between similar builders.
# The first builder gets bvt-inline, and the second builder gets bvt-cq.
# bvt-cq takes longer, so it usually makes sense to give it the faster board.
ShardHWTestsBetweenBuilders('x86-zgb-paladin', 'x86-alex-paladin')
ShardHWTestsBetweenBuilders('wolf-paladin', 'peppy-paladin')
ShardHWTestsBetweenBuilders('daisy-paladin', 'peach_pit-paladin')
ShardHWTestsBetweenBuilders('lumpy-paladin', 'stumpy-paladin')

# Add a pre-cq config for every board.
_CreateConfigsForBoards(pre_cq, _all_boards, 'pre-cq')
_CreateConfigsForBoards(no_vmtest_pre_cq, _all_boards, 'no-vmtest-pre-cq')
_CreateConfigsForBoards(compile_only_pre_cq, _all_boards, 'compile-only-pre-cq')

# TODO(davidjames): Add peach_pit, nyan, and beaglebone to pre-cq.
# TODO(davidjames): Update daisy_spring to build images again.
_config.add_group('mixed-a-pre-cq',
  # daisy_spring w/kernel 3.8.
  config['daisy_spring-compile-only-pre-cq'],
  # lumpy w/kernel 3.8.
  config['lumpy-compile-only-pre-cq'],
)

_config.add_group('mixed-b-pre-cq',
  # arm64 w/kernel 3.14.
  config['rush_ryu-compile-only-pre-cq'],
  # samus w/kernel 3.14.
  config['samus-compile-only-pre-cq'],
)

_config.add_group('mixed-c-pre-cq',
  # brillo
  config['storm-compile-only-pre-cq'],
)

_config.add_group('external-mixed-pre-cq',
  config['x86-generic-no-vmtest-pre-cq'],
  config['amd64-generic-no-vmtest-pre-cq'],
)

# TODO (crbug.com/438839): pre-cq-group has been replaced by multiple
# configs. Remove this config when no active CL has been screened
# with this config.
_config.add_group(constants.PRE_CQ_GROUP_CONFIG,
  # amd64 w/kernel 3.10. This builder runs VMTest so it's going to be
  # the slowest one.
  config['rambi-pre-cq'],

  # daisy_spring w/kernel 3.8.
  config['daisy_spring-compile-only-pre-cq'],

  # brillo config. We set build_packages_in_background=False here, so
  # that subsequent boards (samus, lumpy, parrot) don't get launched until
  # after duck finishes BuildPackages.
  unittest_only_pre_cq.add_config('storm-pre-cq',
                                  _base_configs['storm'],
                                  build_packages_in_background=False),

  # samus w/kernel 3.14.
  config['samus-compile-only-pre-cq'],

  # lumpy w/kernel 3.8.
  config['lumpy-compile-only-pre-cq'],

  # arm64 w/kernel 3.4.
  config['rush_ryu-compile-only-pre-cq'],
)

internal_paladin.add_config('pre-cq-launcher',
  boards=[],
  build_type=constants.PRE_CQ_LAUNCHER_TYPE,
  description='Launcher for Pre-CQ builders',
  trybot_list=False,
  manifest_version=False,
  # Every Pre-CQ launch failure should send out an alert.
  health_threshold=1,
  health_alert_recipients=['chromeos-infra-eng@grotations.appspotmail.com',
                           'tree'],
)


internal_incremental.add_config('mario-incremental',
  boards=['x86-mario'],
)

_toolchain_major.add_config('internal-toolchain-major', internal, official,
  boards=('x86-alex', 'stumpy', 'daisy'),
  build_tests=True,
  description=_toolchain_major['description'] + ' (internal)',
)

_toolchain_minor.add_config('internal-toolchain-minor', internal, official,
  boards=('x86-alex', 'stumpy', 'daisy'),
  build_tests=True,
  description=_toolchain_minor['description'] + ' (internal)',
)

_release = full.derive(official, internal,
  build_type=constants.CANARY_TYPE,
  useflags=append_useflags(['-cros-debug', '-highdpi']),
  build_tests=True,
  afdo_use=True,
  manifest=constants.OFFICIAL_MANIFEST,
  manifest_version=True,
  images=['base', 'test', 'factory_install'],
  push_image=True,
  upload_symbols=True,
  binhost_bucket='gs://chromeos-dev-installer',
  binhost_key='RELEASE_BINHOST',
  binhost_base_url=
    'https://commondatastorage.googleapis.com/chromeos-dev-installer',
  dev_installer_prebuilts=True,
  git_sync=False,
  vm_tests=[constants.SMOKE_SUITE_TEST_TYPE, constants.DEV_MODE_TEST_TYPE,
            constants.CROS_VM_TEST_TYPE],
  hw_tests=HWTestConfig.SharedPoolCanary(),
  paygen=True,
  signer_tests=True,
  trybot_list=True,
  hwqual=True,
  description="Release Builds (canary) (internal)",
  chrome_sdk=True,
  image_test=True,
  doc='http://www.chromium.org/chromium-os/build/builder-overview#TOC-Canaries',
)

_grouped_config = _config(
  build_packages_in_background=True,
  chrome_sdk_build_chrome=False,
  unittests=None,
  vm_tests=[],
)

_grouped_variant_config = _grouped_config.derive(
  chrome_sdk=False,
)

_grouped_variant_release = _release.derive(_grouped_variant_config)

### Master release config.

_release.add_config('master-release',
  boards=[],
  master=True,
  sync_chrome=False,
  chrome_sdk=False,
  health_alert_recipients=['chromeos-infra-eng@grotations.appspotmail.com',
                           'tree'],
  afdo_use=False,
)

### Release config groups.

_config.add_group('x86-alex-release-group',
  _release.add_config('x86-alex-release',
    boards=['x86-alex'],
  ),
  _grouped_variant_release.add_config('x86-alex_he-release',
    boards=['x86-alex_he'],
    hw_tests=[],
    upload_hw_test_artifacts=False,
    paygen_skip_testing=True,
  ),
)

_config.add_group('x86-zgb-release-group',
  _release.add_config('x86-zgb-release',
    boards=['x86-zgb'],
  ),
  _grouped_variant_release.add_config('x86-zgb_he-release',
    boards=['x86-zgb_he'],
    hw_tests=[],
    upload_hw_test_artifacts=False,
    paygen_skip_testing=True,
  ),
)

_config.add_group('parrot-release-group',
  _release.add_config('parrot-release',
    boards=['parrot'],
    critical_for_chrome=True,
  ),
  _grouped_variant_release.add_config('parrot_ivb-release',
    boards=['parrot_ivb'],
  )
)

### Release AFDO configs.

release_afdo = _release.derive(
  trybot_list=False,
  hw_tests=HWTestConfig.DefaultList(pool=constants.HWTEST_SUITES_POOL,
                                    num=4) +
           HWTestConfig.AFDOList(),
  push_image=False,
  paygen=False,
  dev_installer_prebuilts=False,
)

# Now generate generic release-afdo configs if we haven't created anything more
# specific above already. release-afdo configs are builders that do AFDO profile
# collection and optimization in the same builder. Used by developers that
# want to measure performance changes caused by their changes.
def _AddAFDOConfigs():
  for board in _all_release_boards:
    if board in _x86_release_boards:
      base = {}
    else:
      base = non_testable_builder
    generate_config = _config(
        base,
        boards=(board,),
        afdo_generate_min=True,
        afdo_use=False,
        afdo_update_ebuild=True,
    )
    use_config = _config(
        base,
        boards=(board,),
        afdo_use=True,
    )

    config_name = '%s-%s' % (board, CONFIG_TYPE_RELEASE_AFDO)
    if config_name not in config:
      generate_config_name = '%s-%s-%s' % (board, CONFIG_TYPE_RELEASE_AFDO,
                                           'generate')
      use_config_name = '%s-%s-%s' % (board, CONFIG_TYPE_RELEASE_AFDO, 'use')
      _config.add_group(config_name,
                        release_afdo.add_config(generate_config_name,
                                                generate_config),
                        release_afdo.add_config(use_config_name, use_config))

_AddAFDOConfigs()

### Release configs.

# bayleybay-release does not enable vm_tests or unittests due to the compiler
# flags enabled for baytrail.
_release.add_config('bayleybay-release',
  boards=['bayleybay'],
  hw_tests=[],
  vm_tests=[],
  unittests=False,
)

_release.add_config('beltino-release',
  boards=['beltino'],
  hw_tests=[],
  vm_tests=[],
)

# bayleybay-release does not enable vm_tests or unittests due to the compiler
# flags enabled for baytrail.
_release.add_config('bobcat-release',
  boards=['bobcat'],
  hw_tests=[],
  profile='minimal',
  # This build doesn't generate signed images, so don't try to release them.
  paygen=False,
  signer_tests=False,
)

_release.add_config('link-release',
  boards=['link'],
  useflags=append_useflags(['highdpi']),
)

_release.add_config('quawks-release',
  boards=['quawks'],
  useflags=append_useflags(['highdpi']),
)

_release.add_config('quawks_freon-release',
  _base_configs['quawks_freon'],
  useflags=append_useflags(['highdpi']),
)

_release.add_config('samus-release',
  _base_configs['samus'],
  useflags=append_useflags(['highdpi']),
  important=True,
)

_release.add_config('swanky-release',
  boards=['swanky'],
  useflags=append_useflags(['highdpi']),
)

_release.add_config('swanky_freon-release',
  _base_configs['swanky_freon'],
  useflags=append_useflags(['highdpi']),
)

### Arm release configs.

_arm_release = _release.derive(non_testable_builder)

_critical_for_chrome_boards = frozenset([
    'daisy',
    'lumpy',
    'parrot',
])

_arm_release.add_config('peach_pi-release',
  boards=['peach_pi'],
  useflags=append_useflags(['highdpi']),
)

_arm_release.add_config('nyan-release',
  boards=['nyan'],
  useflags=append_useflags(['highdpi']),
)

_arm_release.add_config('nyan_big-release',
  boards=['nyan_big'],
  useflags=append_useflags(['highdpi']),
)

_arm_release.add_config('nyan_blaze-release',
  boards=['nyan_blaze'],
  useflags=append_useflags(['highdpi']),
)

_arm_release.add_config('veyron_rialto-release',
  _base_configs['veyron_rialto'],
  # rialto does not use Chrome.
  sync_chrome=False,
  chrome_sdk=False,
)

# Now generate generic release configs if we haven't created anything more
# specific above already.
def _AddReleaseConfigs():
  # We have to mark all autogenerated PFQs as not important so the master
  # does not wait for them.  http://crbug.com/386214
  # If you want an important PFQ, you'll have to declare it yourself.
  _CreateConfigsForBoards(
    chrome_info, _all_release_boards, 'tot-chrome-pfq-informational',
    important=False)
  _CreateConfigsForBoards(
    chrome_pfq, _chrome_pfq_important_boards, 'chrome-pfq')
  _CreateConfigsForBoards(
    chrome_pfq, _all_release_boards, 'chrome-pfq', important=False)
  _CreateConfigsForBoards(
    _release, _critical_for_chrome_boards, CONFIG_TYPE_RELEASE,
    critical_for_chrome=True)
  _CreateConfigsForBoards(_release, _all_release_boards, CONFIG_TYPE_RELEASE)


_AddReleaseConfigs()

_brillo_release = _release.derive(brillo,
  dev_installer_prebuilts=False,
  afdo_use=False,
  signer_tests=True,
  image_test=True,
)

_brillo_release.add_config('gizmo-release',
  boards=['gizmo'],

  # This build doesn't generate signed images, so don't try to release them.
  paygen=False,
  signer_tests=False,
)

_brillo_release.add_config('lemmings-release',
  boards=['lemmings'],

  # Hw Lab can't test, yet.
  paygen_skip_testing=True,

  # This build doesn't generate signed images, so don't try to release them.
  paygen=False,
  signer_tests=False,
)

_brillo_release.add_config('panther_embedded-minimal-release',
  boards=['panther_embedded'],
  profile='minimal',
  paygen=False,
  signer_tests=False,
)

# beaglebone build doesn't generate signed images, so don't try to release them.
_beaglebone_release = _brillo_release.derive(beaglebone, paygen=False,
                                             signer_tests=False,
                                             images=['base', 'test'])

_config.add_group('beaglebone-release-group',
  _beaglebone_release.add_config('beaglebone-release',
    boards=['beaglebone'],
  ),
  _beaglebone_release.add_config('beaglebone_servo-release',
    boards=['beaglebone_servo'],
  ).derive(_grouped_variant_config),
  important=True,
)

_non_testable_brillo_release = _brillo_release.derive(non_testable_builder)

_non_testable_brillo_release.add_config('kayle-release',
  _base_configs['kayle'],
  paygen=False,
  signer_tests=False,
)

_non_testable_brillo_release.add_config('cosmos-release',
  boards=['cosmos'],

  paygen_skip_testing=True,
  important=False,
  signer_tests=False,
)

_non_testable_brillo_release.add_config('storm-release',
  boards=['storm'],

  # Hw Lab can't test storm, yet.
  paygen_skip_testing=True,
  important=True,
  signer_tests=False
)

_non_testable_brillo_release.add_config('urara-release',
  boards=['urara'],
  paygen=False,
  signer_tests=False,
  important=False,
)

_release.add_config('mipsel-o32-generic-release',
  _base_configs['mipsel-o32-generic'],
  paygen_skip_delta_payloads=True,
  afdo_use=False,
  hw_tests=[],
)

_release.add_config('stumpy_moblab-release',
  moblab,
  boards=['stumpy_moblab'],
  images=['base', 'test'],
  paygen_skip_delta_payloads=True,
  # TODO: re-enable paygen testing when crbug.com/386473 is fixed.
  paygen_skip_testing=True,
  important=True,
  afdo_use=False,
  signer_tests=False,
  hw_tests=[HWTestConfig(constants.HWTEST_MOBLAB_SUITE, blocking=True, num=1,
                         timeout=120*60),
            HWTestConfig(constants.HWTEST_BVT_SUITE, blocking=True,
                         warn_only=True, num=1),
            HWTestConfig(constants.HWTEST_AU_SUITE, blocking=True,
                         warn_only=True, num=1)],
)

_release.add_config('panther_moblab-release',
  moblab,
  boards=['panther_moblab'],
  images=['base', 'test'],
  paygen_skip_delta_payloads=True,
  # TODO: re-enable paygen testing when crbug.com/386473 is fixed.
  paygen_skip_testing=True,
  important=False,
  afdo_use=False,
  signer_tests=False,
  hw_tests=[HWTestConfig(constants.HWTEST_BVT_SUITE, blocking=True,
                         warn_only=True, num=1),
            HWTestConfig(constants.HWTEST_AU_SUITE, blocking=True,
                         warn_only=True, num=1)],
)

_release.add_config('rush-release',
  non_testable_builder,
  boards=['rush'],
  hw_tests=[],
  # This build doesn't generate signed images, so don't try to release them.
  paygen=False,
  signer_tests=False,
)

_release.add_config('rush_ryu-release',
  non_testable_builder,
  boards=['rush_ryu'],
  useflags=append_useflags(['highdpi']),
  hw_tests=[],
)

_release.add_config('whirlwind-release',
  _base_configs['whirlwind'],
  important=True,
)

### Per-chipset release groups

def _AddGroupConfig(name, base_board, group_boards=(),
                    group_variant_boards=(), **kwargs):
  """Generate full & release group configs."""
  for group in ('release', 'full'):
    configs = []

    all_boards = [base_board] + list(group_boards) + list(group_variant_boards)
    desc = '%s; Group config (boards: %s)' % (
        config['%s-%s' % (base_board, group)].description,
        ', '.join(all_boards))

    for board in all_boards:
      if board in group_boards:
        subconfig = _grouped_config
      elif board in group_variant_boards:
        subconfig = _grouped_variant_config
      else:
        subconfig = {}
      board_config = '%s-%s' % (board, group)
      configs.append(config[board_config].derive(subconfig, **kwargs))

      config_name = '%s-%s-group' % (name, group)
      important = group == 'release' and kwargs.get('important', True)
    _config.add_group(config_name, *configs, description=desc,
                      important=important)

# pineview chipset boards
_AddGroupConfig('pineview', 'x86-mario', (
    'x86-alex',
    'x86-zgb',
), (
    'x86-alex_he',
    'x86-zgb_he',
))

# sandybridge chipset boards
_AddGroupConfig('sandybridge', 'parrot', (
    'lumpy',
    'butterfly',
    'stumpy',
))

# sandybridge chipset boards (freon variant)
_AddGroupConfig('sandybridge-freon', 'parrot_freon', (
    'lumpy_freon',
    'butterfly_freon',
    'stumpy_freon',
),
  important=False,
)

# ivybridge chipset boards
_AddGroupConfig('ivybridge', 'stout', (
), (
    'parrot_ivb',
))

# ivybridge chipset boards (freon variant)
_AddGroupConfig('ivybridge-freon', 'stout_freon', (
  'link',
),
  important=False,
)

# slippy-based haswell boards
# TODO(davidjames): Combine slippy and beltino into haswell canary, once we've
# optimized our builders more.
# slippy itself is deprecated in favor of the below boards, so we don't bother
# building it.
# TODO(dnj): Re-add peppy canary once builders are allocated.
_AddGroupConfig('slippy', 'falco', (
    'leon',
    'wolf',
), (
    'falco_li',
))

# beltino-based haswell boards
# beltino itself is deprecated in favor of the below boards, so we don't bother
# building it.

_AddGroupConfig('beltino-a', 'panther', (
    'mccloud',
))

_AddGroupConfig('beltino-b', 'monroe', (
    'tricky',
    'zako',
))

# rambi-based boards
_AddGroupConfig('rambi-a', 'rambi', (
    'clapper',
    'enguarde',
    'expresso',
))

_AddGroupConfig('rambi-b', 'glimmer', (
    'gnawty',
    'kip',
    'quawks',
))

_AddGroupConfig('rambi-c', 'squawks', (
    'swanky',
    'winky',
    'candy',
))

_AddGroupConfig('rambi-d', 'cranky', (
    'parry',
    'banjo',
    ),
    important=False,
)

# rambi-based boards (freon variant)
_AddGroupConfig('rambi-freon-a', 'rambi_freon', (
    'clapper_freon',
    'enguarde_freon',
    'expresso_freon',
),
  important=False,
)

_AddGroupConfig('rambi-freon-b', 'glimmer_freon', (
    'gnawty_freon',
    'kip_freon',
    'quawks_freon',
),
  important=False,
)

_AddGroupConfig('rambi-freon-c', 'squawks_freon', (
    'swanky_freon',
    'winky_freon',
    'candy_freon',
),
  important=False,
)

# daisy-based boards
_AddGroupConfig('daisy', 'daisy', (
    'daisy_spring',
    'daisy_skate',
))

# peach-based boards
_AddGroupConfig('peach', 'peach_pit', (
    'peach_pi',
))

# nyan-based boards
_AddGroupConfig('nyan', 'nyan', (
    'nyan_big',
    'nyan_blaze',
    'nyan_kitty',
))

# auron-based boards
_AddGroupConfig('auron', 'auron', (
    'auron_yuna',
    'auron_paine',
))

_AddGroupConfig('auron-b', 'lulu', (
    'cid',
    ),
    important=False
)

# veyron-based boards
_AddGroupConfig('veyron', 'veyron_pinky', (
    'veyron_jerry',
    'veyron_mighty',
    'veyron_speedy'
    ),
    important=False,
)

_AddGroupConfig('veyron-b', 'veyron_gus', (
    'veyron_jaq',
    'veyron_minnie',
    'veyron_rialto',
    ),
    important=False,
)

_AddGroupConfig('veyron-c', 'veyron_brain', (
    'veyron_danger',
    ),
    important=False,
)

# jecht-based boards
_AddGroupConfig('jecht', 'jecht', (
    'guado',
    'tidus',
    'rikku',
    ),
    important=False,
)

# Factory and Firmware releases much inherit from these classes.  Modifications
# for these release builders should go here.

# Naming conventions also must be followed.  Factory and firmware branches must
# end in -factory or -firmware suffixes.

_factory_release = _release.derive(
  upload_hw_test_artifacts=False,
  upload_symbols=False,
  hw_tests=[],
  chrome_sdk=False,
  description='Factory Builds',
  paygen=False,
  afdo_use=False,
)

_firmware = _config(
  images=[],
  factory_toolkit=False,
  packages=('virtual/chromeos-firmware',),
  usepkg_build_packages=True,
  sync_chrome=False,
  build_tests=False,
  chrome_sdk=False,
  unittests=False,
  vm_tests=[],
  hw_tests=[],
  dev_installer_prebuilts=False,
  upload_hw_test_artifacts=False,
  upload_symbols=False,
  signer_tests=False,
  trybot_list=False,
  paygen=False,
  image_test=False,
)

_firmware_release = _release.derive(_firmware,
  description='Firmware Canary',
  manifest=constants.DEFAULT_MANIFEST,
  afdo_use=False,
)

_depthcharge_release = _firmware_release.derive(
  useflags=append_useflags(['depthcharge']))

_depthcharge_full_internal = full.derive(
  internal,
  _firmware,
  useflags=append_useflags(['depthcharge']),
  description='Firmware Informational',
)

_firmware_boards = frozenset([
  'auron',
  'banjo',
  'bayleybay',
  'beltino',
  'butterfly',
  'candy',
  'clapper',
  'daisy',
  'daisy_skate',
  'daisy_spring',
  'enguarde',
  'expresso',
  'falco',
  'glimmer',
  'gnawty',
  'jecht',
  'kip',
  'leon',
  'link',
  'lumpy',
  'monroe',
  'panther',
  'parrot',
  'parry',
  'peach_pi',
  'peach_pit',
  'peppy',
  'quawks',
  'rambi',
  'rikku',
  'samus',
  'slippy',
  'squawks',
  'storm',
  'stout',
  'strago',
  'stumpy',
  'swanky',
  'winky',
  'wolf',
  'x86-mario',
  'zako',
])

_x86_depthcharge_firmware_boards = frozenset([
  'auron',
  'banjo',
  'bayleybay',
  'candy',
  'clapper',
  'enguarde',
  'expresso',
  'glimmer',
  'gnawty',
  'jecht',
  'kip',
  'leon',
  'link',
  'parry',
  'quawks',
  'rambi',
  'rikku',
  'samus',
  'squawks',
  'strago',
  'swanky',
  'winky',
  'zako',
])


def _AddFirmwareConfigs():
  """Add x86 and arm firmware configs."""
  for board in _firmware_boards:
    _firmware_release.add_config('%s-%s' % (board, CONFIG_TYPE_FIRMWARE),
        _base_configs[board],
    )

  for board in _x86_depthcharge_firmware_boards:
    _depthcharge_release.add_config(
        '%s-%s-%s' % (board, 'depthcharge', CONFIG_TYPE_FIRMWARE),
        _base_configs[board],
    )
    _depthcharge_full_internal.add_config(
        '%s-%s-%s-%s' % (board, 'depthcharge', CONFIG_TYPE_FULL,
                         CONFIG_TYPE_FIRMWARE),
        _base_configs[board],
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
  non_testable_builder,
  boards=['daisy'],
)

_payloads = internal.derive(
  build_type=constants.PAYLOADS_TYPE,
  description='Regenerate release payloads.',
  vm_tests=[],

  # Sync to the code used to do the build the first time.
  manifest_version=True,

  # This is the actual work we want to do.
  paygen=True,

  upload_hw_test_artifacts=False,
)

def _AddPayloadConfigs():
  """Create <board>-payloads configs for all payload generating boards.

  We create a config named 'board-payloads' for every board which has a
  config with 'paygen' True. The idea is that we have a build that generates
  payloads, we need to have a tryjob to re-attempt them on failure.
  """
  payload_boards = set()

  def _search_config_and_children(search_config):
    # If paygen is enabled, add it's boards to our list of payload boards.
    if search_config['paygen']:
      for board in search_config['boards']:
        payload_boards.add(board)

    # Recurse on any child configs.
    for child in search_config['child_configs']:
      _search_config_and_children(child)

  # Search all configs for boards that generate payloads.
  for _, search_config in config.iteritems():
    _search_config_and_children(search_config)

  # Generate a payloads trybot config for every board that generates payloads.
  for board in payload_boards:
    name = '%s-payloads' % board
    _payloads.add_config(name, boards=[board])

_AddPayloadConfigs()

def _AddProjectSdkConfigs():
  for board in _project_sdk_boards:
    name = '%s-project-sdk' % board
    project_sdk.add_config(name, boards=[board])

_AddProjectSdkConfigs()

def GetDisplayPosition(config_name, type_order=CONFIG_TYPE_DUMP_ORDER):
  """Given a config_name, return display position specified by suffix_order.

  Args:
    config_name: Name of config to look up.
    type_order: A tuple/list of config types in the order they are to be
                displayed.

  Returns:
    If |config_name| does not contain any of the suffixes, returns the index
    position after the last element of suffix_order.
  """
  for index, config_type in enumerate(type_order):
    if config_name.endswith('-' + config_type) or config_name == config_type:
      return index

  return len(type_order)


# This is a list of configs that should be included on the main waterfall, but
# aren't included by default (see IsDefaultMainWaterfall). This loosely
# corresponds to the set of experimental or self-standing configs.
_waterfall_config_map = {
    constants.WATERFALL_EXTERNAL: frozenset([
      # Experimental Paladins
      'amd64-generic_freon-paladin',

      # Incremental
      'amd64-generic-incremental',
      'daisy-incremental',
      'x86-generic-incremental',

      # Full
      'amd64-generic-full',
      'arm-generic-full',
      'daisy-full',
      'mipsel-o32-generic-full',

      # ASAN
      'amd64-generic-asan',
      'x86-generic-asan',

      # Utility
      'chromiumos-sdk',
      'refresh-packages',
    ]),

    constants.WATERFALL_INTERNAL: frozenset([
      # Experimental Paladins
      'daisy_skate-paladin',
      'nyan_freon-paladin',
      'whirlwind-paladin',

      # Experimental Canaries
      'auron-b-release-group',
      'ivybridge-freon-release-group',
      'jecht-release-group',
      'rambi-d-release-group',
      'rambi-freon-a-release-group',
      'rambi-freon-b-release-group',
      'rambi-freon-c-release-group',
      'sandybridge-freon-release-group',
      'veyron-b-release-group',
      'veyron-c-release-group',
      'veyron-release-group',

      # Experimental Canaries (Group)
      'bobcat-release',
      'cosmos-release',
      'daisy_winter-release',
      'gizmo-release',
      'kayle-release',
      'lemmings-release',
      'nyan_freon-release',
      'panther_embedded-minimal-release',
      'panther_moblab-release',
      'peppy-release',
      'rush_ryu-release',
      'strago-release',
      'urara-release',

      # Experimental PFQs.
      'peach_pit-chrome-pfq',

      # Incremental Builders.
      'mario-incremental',

      # Firmware Builders.
      'link-depthcharge-full-firmware',

      # SDK Builders.
      'panther_embedded-project-sdk',

      # Toolchain Builders.
      'internal-toolchain-major',
      'internal-toolchain-minor',
    ]),
}

def _SetupWaterfalls():
  for name, c in config.iteritems():
    c['active_waterfall'] = GetDefaultWaterfall(c)

  # Apply manual configs.
  for waterfall, names in _waterfall_config_map.iteritems():
    for name in names:
      config[name]['active_waterfall'] = waterfall

_SetupWaterfalls()
