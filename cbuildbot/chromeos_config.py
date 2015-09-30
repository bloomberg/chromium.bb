# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configuration options for various cbuildbot builders."""

from __future__ import print_function

import copy

from chromite.cbuildbot import config_lib
from chromite.cbuildbot import constants
from chromite.lib import factory


# Set to 'True' if this is a release branch. This updates the '-release' builder
# configuration to the shape used by the release waterfall.
IS_RELEASE_BRANCH = False


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
    # Force uprev. This is so patched in changes are always
    # built.
    my_config['uprev'] = True
    if my_config['internal']:
      my_config['overlays'] = constants.BOTH_OVERLAYS

    # Use the local manifest which only requires elevated access if it's really
    # needed to build.
    if not options.remote_trybot:
      my_config['manifest'] = my_config['dev_manifest']

    my_config['push_image'] = False

    if my_config['build_type'] != constants.PAYLOADS_TYPE:
      my_config['paygen'] = False

    if options.hwtest and my_config['hw_tests_override'] is not None:
      my_config['hw_tests'] = my_config['hw_tests_override']

    # Default to starting with a fresh chroot on remote trybot runs.
    if options.remote_trybot:
      my_config['chroot_replace'] = True

    # In trybots, we want to always run VM tests and all unit tests, so that
    # developers will get better testing for their changes.
    if my_config['vm_tests_override'] is not None:
      my_config['vm_tests'] = my_config['vm_tests_override']

  return copy_config


def GetDefaultWaterfall(build_config):
  if not (build_config['important'] or build_config['master']):
    return None
  if build_config['branch']:
    return None
  b_type = build_config['build_type']

  if config_lib.IsCanaryType(b_type):
    # If this is a canary build, it may fall on different waterfalls:
    # - If we're building for a release branch, it belongs on a release
    #   waterfall.
    # - Otherwise, it belongs on the internal waterfall.
    if IS_RELEASE_BRANCH:
      return constants.WATERFALL_RELEASE
    else:
      return constants.WATERFALL_INTERNAL
  elif config_lib.IsCQType(b_type):
    # A Paladin can appear on the public or internal waterfall depending on its
    # 'internal' status.
    return (constants.WATERFALL_INTERNAL if build_config['internal'] else
            constants.WATERFALL_EXTERNAL)
  elif config_lib.IsPFQType(b_type) or b_type == constants.PRE_CQ_LAUNCHER_TYPE:
    # These builder types belong on the internal waterfall.
    return constants.WATERFALL_INTERNAL
  else:
    # No default active waterfall.
    return None


class HWTestList(object):
  """Container for methods to generate HWTest lists."""

  @classmethod
  def DefaultList(cls, **kwargs):
    """Returns a default list of HWTestConfig's for a build

    Args:
      *kwargs: overrides for the configs
    """
    # Number of tests running in parallel in the AU suite.
    AU_TESTS_NUM = 2
    # Number of tests running in parallel in the asynchronous canary
    # test suite
    ASYNC_TEST_NUM = 2

    # Set the number of machines for the au and qav suites. If we are
    # constrained in the number of duts in the lab, only give 1 dut to each.
    if (kwargs.get('num', constants.HWTEST_DEFAULT_NUM) >=
        constants.HWTEST_DEFAULT_NUM):
      au_dict = dict(num=AU_TESTS_NUM)
      async_dict = dict(num=ASYNC_TEST_NUM)
    else:
      au_dict = dict(num=1)
      async_dict = dict(num=1)

    au_kwargs = kwargs.copy()
    au_kwargs.update(au_dict)

    async_kwargs = kwargs.copy()
    async_kwargs.update(async_dict)
    async_kwargs['priority'] = constants.HWTEST_POST_BUILD_PRIORITY
    async_kwargs['async'] = True
    async_kwargs['suite_min_duts'] = 1

    # BVT + AU suite.
    return [config_lib.HWTestConfig(constants.HWTEST_BVT_SUITE,
                                    blocking=True, **kwargs),
            config_lib.HWTestConfig(constants.HWTEST_AU_SUITE,
                                    blocking=True, **au_kwargs),
            config_lib.HWTestConfig(constants.HWTEST_COMMIT_SUITE,
                                    **async_kwargs),
            config_lib.HWTestConfig(constants.HWTEST_CANARY_SUITE,
                                    **async_kwargs)]

  @classmethod
  def DefaultListCanary(cls, **kwargs):
    """Returns a default list of config_lib.HWTestConfig's for a canary build.

    Args:
      *kwargs: overrides for the configs
    """
    # Set minimum_duts default to 4, which means that lab will check the
    # number of available duts to meet the minimum requirement before creating
    # the suite job for canary builds.
    kwargs.setdefault('minimum_duts', 4)
    kwargs.setdefault('file_bugs', True)
    return HWTestList.DefaultList(**kwargs)

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
    return [config_lib.HWTestConfig('perf_v2', **afdo_dict)]

  @classmethod
  def DefaultListNonCanary(cls, **kwargs):
    """Return a default list of HWTestConfig's for a non-canary build.

    Optional arguments may be overridden in `kwargs`, except that
    the `blocking` setting cannot be provided.
    """
    return [config_lib.HWTestConfig(constants.HWTEST_BVT_SUITE, **kwargs),
            config_lib.HWTestConfig(constants.HWTEST_COMMIT_SUITE, **kwargs)]

  @classmethod
  def DefaultListCQ(cls, **kwargs):
    """Return a default list of HWTestConfig's for a CQ build.

    Optional arguments may be overridden in `kwargs`, except that
    the `blocking` setting cannot be provided.
    """
    default_dict = dict(pool=constants.HWTEST_PALADIN_POOL, timeout=120 * 60,
                        file_bugs=False, priority=constants.HWTEST_CQ_PRIORITY,
                        minimum_duts=4, offload_failures_only=True)
    # Allows kwargs overrides to default_dict for cq.
    default_dict.update(kwargs)
    return HWTestList.DefaultListNonCanary(**default_dict)

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
    return HWTestList.DefaultListNonCanary(**default_dict)

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
    suite_list = [config_lib.HWTestConfig(constants.HWTEST_SANITY_SUITE,
                                          **sanity_dict)]
    suite_list.extend(HWTestList.DefaultListPFQ(**default_dict))
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
    suite_list = [config_lib.HWTestConfig(constants.HWTEST_SANITY_SUITE,
                                          **sanity_dict)]
    suite_list.extend(HWTestList.DefaultListCQ(**default_dict))
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
    suite_list = [config_lib.HWTestConfig(constants.HWTEST_SANITY_SUITE,
                                          **sanity_dict)]
    suite_list.extend(HWTestList.DefaultListCanary(**default_dict))
    return suite_list

  @classmethod
  def AFDORecordTest(cls, **kwargs):
    default_dict = dict(pool=constants.HWTEST_MACH_POOL,
                        warn_only=True, num=1, file_bugs=True,
                        timeout=constants.AFDO_GENERATE_TIMEOUT,
                        priority=constants.HWTEST_PFQ_PRIORITY)
    # Allows kwargs overrides to default_dict for cq.
    default_dict.update(kwargs)
    return config_lib.HWTestConfig(constants.HWTEST_AFDO_SUITE, **default_dict)

  @classmethod
  def WiFiCellPoolPreCQ(cls, **kwargs):
    """Return a list of HWTestConfigs which run wifi tests.

    This should be used by the ChromeOS WiFi team to ensure changes pass the
    wifi tests as a pre-cq sanity check.
    """
    default_dict = dict(pool=constants.HWTEST_WIFICELL_PRE_CQ_POOL,
                        blocking=True, file_bugs=False,
                        priority=constants.HWTEST_DEFAULT_PRIORITY,
                        retry=False, max_retries=None, minimum_duts=1)
    default_dict.update(kwargs)
    suite_list = [config_lib.HWTestConfig(constants.WIFICELL_PRE_CQ,
                                          **default_dict)]
    return suite_list

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


TRADITIONAL_VM_TESTS_SUPPORTED = [constants.SMOKE_SUITE_TEST_TYPE,
                                  constants.SIMPLE_AU_TEST_TYPE,
                                  constants.CROS_VM_TEST_TYPE]

#
# Define assorted constants describing various sets of boards.
#

# Base per-board configuration.
# Every board must appear in exactly 1 of the following sets.

_arm_internal_release_boards = frozenset([
    'arkham',
    'beaglebone',
    'beaglebone_servo',
    'daisy',
    'daisy_skate',
    'daisy_spring',
    'daisy_winter',
    'kayle',
    'nyan',
    'nyan_big',
    'nyan_blaze',
    'nyan_freon',
    'nyan_kitty',
    'oak',
    'peach_pi',
    'peach_pit',
    'purin',
    'smaug',
    'storm',
    'rush',
    'rush_ryu',
    'veyron_brain',
    'veyron_danger',
    'veyron_gus',
    'veyron_jaq',
    'veyron_jerry',
    'veyron_mickey',
    'veyron_mighty',
    'veyron_minnie',
    'veyron_pinky',
    'veyron_rialto',
    'veyron_romy',
    'veyron_shark',
    'veyron_speedy',
    'veyron_thea',
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
    'buddy',
    'butterfly',
    'candy',
    'celes',
    'cid',
    'clapper',
    'cranky',
    'cyan',
    'enguarde',
    'expresso',
    'falco',
    'falco_li',
    'gandof',
    'glados',
    'glimmer',
    'gnawty',
    'guado',
    'guado_moblab',
    'heli',
    'jecht',
    'kip',
    'kunimitsu',
    'lakitu',
    'lakitu_mobbuild',
    'lakitu_next',
    'leon',
    'link',
    'lulu',
    'lumpy',
    'mccloud',
    'monroe',
    'ninja',
    'orco',
    'panther',
    'panther_embedded',
    'panther_moblab',
    'parrot',
    'parrot_ivb',
    'parry',
    'peppy',
    'quawks',
    'rambi',
    'rikku',
    'samus',
    'slippy',
    'squawks',
    'stout',
    'strago',
    'stumpy',
    'stumpy_moblab',
    'sumo',
    'swanky',
    'tidus',
    'tricky',
    'ultima',
    'winky',
    'wizpig',
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
    'x86-generic_freon',
])

# Every board should be in only 1 of the above sets.
_distinct_board_sets = [
    _arm_internal_release_boards,
    _arm_external_boards,
    _x86_internal_release_boards,
    _x86_external_boards,
]

_arm_full_boards = (_arm_internal_release_boards |
                    _arm_external_boards)
_x86_full_boards = (_x86_internal_release_boards |
                    _x86_external_boards)

_arm_boards = _arm_full_boards
_x86_boards = _x86_full_boards

_all_release_boards = (
    _arm_internal_release_boards |
    _x86_internal_release_boards
)
_all_full_boards = (
    _arm_full_boards |
    _x86_full_boards
)
_all_boards = (
    _x86_boards |
    _arm_boards
)

_arm_release_boards = _arm_internal_release_boards
_x86_release_boards = _x86_internal_release_boards

_internal_boards = _all_release_boards

# Board can appear in 1 or more of the following sets.
_brillo_boards = frozenset([
    'arkham',
    'gizmo',
    'kayle',
    'lakitu',
    'lakitu_mobbuild',
    'lakitu_next',
    'panther_embedded',
    'purin',
    'storm',
    'whirlwind',
])

_moblab_boards = frozenset([
    'stumpy_moblab',
    'panther_moblab',
    'guado_moblab',
])

_minimal_profile_boards = frozenset([
    'bobcat',
])

_nofactory_boards = frozenset([
    'daisy_winter',
    'smaug',
])

_toolchains_from_source = frozenset([
    'x32-generic',
])

_noimagetest_boards = frozenset([
    'lakitu',
    'lakitu_mobbuild',
    'lakitu_next',
])

_nohwqual_boards = frozenset([
    'kayle',
    'lakitu',
    'lakitu_mobbuild',
    'lakitu_next',
])

_norootfs_verification_boards = frozenset([
])

_base_layout_boards = frozenset([
    'lakitu',
    'lakitu_mobbuild',
    'lakitu_next',
])

_no_unittest_boards = frozenset((
))

_upload_gce_images_boards = frozenset([
    'lakitu',
    'lakitu_mobbuild',
    'lakitu_next',
])

_no_vmtest_boards = _arm_boards | _brillo_boards


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
        'oak-full',
        'x86-generic-full',

        # ASAN
        'amd64-generic-asan',
        'x86-generic-asan',

        # Utility
        'chromiumos-sdk',
        'refresh-packages',

        # LLVM
        'amd64-generic-llvm',
    ]),

    constants.WATERFALL_INTERNAL: frozenset([
        # Experimental Paladins.
        'lakitu_next-paladin',
        'panther_moblab-paladin',
        'stumpy_moblab-paladin',

        # Experimental Canaries (Group)
        'storm-release-group',
        'strago-release-group',
        'veyron-c-release-group',

        # Experimental Canaries
        'bobcat-release',
        'daisy_winter-release',
        'kayle-release',
        'lakitu_next-release',
        'nyan_freon-release',
        'panther_moblab-release',
        'rush_ryu-release',
        'smaug-release',
        'guado_moblab-release',

        # Incremental Builders.
        'mario-incremental',
        'lakitu-incremental',
        'lakitu_next-incremental',

        # Firmware Builders.
        'link-depthcharge-full-firmware',

        # Toolchain Builders.
        'internal-toolchain-major',
        'internal-toolchain-minor',
    ]),

    constants.WATERFALL_RELEASE: frozenset([
    ]),
}


@factory.CachedFunctionCall
def GetConfig():
  # Chrome OS site parameters.
  site_params = config_lib.DefaultSiteParameters()

  # Helpers for constructing Chrome OS site parameters.
  manifest_project = 'chromiumos/manifest'
  manifest_int_project = 'chromeos/manifest-internal'
  external_remote = 'cros'
  internal_remote = 'cros-internal'
  kayle_internal_remote = 'kayle-cros-internal'
  chromium_remote = 'chromium'
  chrome_remote = 'chrome'
  aosp_remote = 'aosp'
  weave_remote = 'weave'

  # Gerrit instance site parameters.
  site_params.update(
      config_lib.GerritInstanceParameters('EXTERNAL', 'chromium'))
  site_params.update(
      config_lib.GerritInstanceParameters('INTERNAL', 'chrome-internal'))
  site_params.update(
      config_lib.GerritInstanceParameters('AOSP', 'android'))
  site_params.update(
      config_lib.GerritInstanceParameters('WEAVE', 'weave'))

  site_params.update(
      # Parameters to define which manifests to use.
      MANIFEST_PROJECT=manifest_project,
      MANIFEST_INT_PROJECT=manifest_int_project,
      MANIFEST_PROJECTS=(manifest_project, manifest_int_project),
      MANIFEST_URL='%s/%s' % (
          site_params['EXTERNAL_GOB_URL'], manifest_project
      ),
      MANIFEST_INT_URL='%s/%s' % (
          site_params['INTERNAL_GERRIT_URL'], manifest_int_project
      ),

      # CrOS remotes specified in the manifests.
      EXTERNAL_REMOTE=external_remote,
      INTERNAL_REMOTE=internal_remote,
      GOB_REMOTES={
          site_params['EXTERNAL_GOB_INSTANCE']: external_remote,
          site_params['INTERNAL_GOB_INSTANCE']: internal_remote
      },
      KAYLE_INTERNAL_REMOTE=kayle_internal_remote,
      CHROMIUM_REMOTE=chromium_remote,
      CHROME_REMOTE=chrome_remote,
      AOSP_REMOTE=aosp_remote,
      WEAVE_REMOTE=weave_remote,

      # Only remotes listed in CROS_REMOTES are considered branchable.
      # CROS_REMOTES and BRANCHABLE_PROJECTS must be kept in sync.
      GERRIT_HOSTS={
          external_remote: site_params['EXTERNAL_GERRIT_HOST'],
          internal_remote: site_params['INTERNAL_GERRIT_HOST'],
          aosp_remote: site_params['AOSP_GERRIT_HOST'],
          weave_remote: site_params['WEAVE_GERRIT_HOST']
      },
      CROS_REMOTES={
          external_remote: site_params['EXTERNAL_GOB_URL'],
          internal_remote: site_params['INTERNAL_GOB_URL'],
          kayle_internal_remote: site_params['INTERNAL_GOB_URL'],
          aosp_remote: site_params['AOSP_GOB_URL'],
          weave_remote: site_params['WEAVE_GOB_URL']
      },
      GIT_REMOTES={
          chromium_remote: site_params['EXTERNAL_GOB_URL'],
          chrome_remote: site_params['INTERNAL_GOB_URL'],
          external_remote: site_params['EXTERNAL_GOB_URL'],
          internal_remote: site_params['INTERNAL_GOB_URL'],
          kayle_internal_remote: site_params['INTERNAL_GOB_URL'],
          aosp_remote: site_params['AOSP_GOB_URL'],
          weave_remote: site_params['WEAVE_GOB_URL']
      },

      # Prefix to distinguish internal and external changes. This is used
      # when a user specifies a patch with "-g", when generating a key for
      # a patch to use in our PatchCache, and when displaying a custom
      # string for the patch.
      CHANGE_PREFIX={
          external_remote: site_params['EXTERNAL_CHANGE_PREFIX'],
          internal_remote: site_params['INTERNAL_CHANGE_PREFIX'],
      },

      # List of remotes that are okay to include in the external manifest.
      EXTERNAL_REMOTES=(
          external_remote, chromium_remote
      ),

      # Mapping 'remote name' -> regexp that matches names of repositories on
      # that remote that can be branched when creating CrOS branch.
      # Branching script will actually create a new git ref when branching
      # these projects. It won't attempt to create a git ref for other projects
      # that may be mentioned in a manifest. If a remote is missing from this
      # dictionary, all projects on that remote are considered to not be
      # branchable.
      BRANCHABLE_PROJECTS={
          external_remote: r'chromiumos/(.+)',
          internal_remote: r'chromeos/(.+)',
          kayle_internal_remote: r'chromeos/(.+)'
      },

      # Additional parameters used to filter manifests, create modified
      # manifests, and to branch manifests.
      MANIFEST_VERSIONS_GOB_URL=(
          '%s/chromiumos/manifest-versions' % site_params['EXTERNAL_GOB_URL']
      ),
      MANIFEST_VERSIONS_INT_GOB_URL=(
          '%s/chromeos/manifest-versions' % site_params['INTERNAL_GOB_URL']
      ),
      MANIFEST_VERSIONS_GOB_URL_TEST=(
          '%s/chromiumos/manifest-versions-test' % (
              site_params['EXTERNAL_GOB_URL']
          )
      ),
      MANIFEST_VERSIONS_INT_GOB_URL_TEST=(
          '%s/chromeos/manifest-versions-test' % site_params['INTERNAL_GOB_URL']
      ),
      MANIFEST_VERSIONS_GS_URL='gs://chromeos-manifest-versions',

      # Standard directories under buildroot for cloning these repos.
      EXTERNAL_MANIFEST_VERSIONS_PATH='manifest-versions',
      INTERNAL_MANIFEST_VERSIONS_PATH='manifest-versions-internal',

      # URL of the repo project.
      REPO_URL='%s/external/repo' % site_params['EXTERNAL_GOB_URL']
  )

  # Site specific adjustments for default BuildConfig values.
  defaults = config_lib.DefaultSettings()

  # Git repository URL for our manifests.
  #  https://chromium.googlesource.com/chromiumos/manifest
  #  https://chrome-internal.googlesource.com/chromeos/manifest-internal
  defaults['manifest_repo_url'] = site_params['MANIFEST_URL']

  # Site configuration.
  site_config = config_lib.SiteConfig(defaults=defaults,
                                      site_params=site_params)

  default_hw_tests_override = config_lib.BuildConfig(
      hw_tests_override=HWTestList.DefaultList(
          num=constants.HWTEST_TRYBOT_NUM, pool=constants.HWTEST_TRYBOT_POOL,
          file_bugs=False),
  )

  # Arch-specific mixins.

  # Config parameters for builders that do not run tests on the builder.
  no_unittest_builder = config_lib.BuildConfig(
      unittests=False,
  )

  no_vmtest_builder = config_lib.BuildConfig(
      vm_tests=[],
      vm_tests_override=None,
  )

  no_hwtest_builder = config_lib.BuildConfig(
      hw_tests=[],
      hw_tests_override=[],
  )

  # Builder-specific mixins

  config_lib.BuildConfig(
      # Full builds that build fully from binaries.
      build_type=constants.BUILD_FROM_SOURCE_TYPE,
      archive_build_debug=True,
      images=['test', 'factory_install'],
      git_sync=True,
  )

  full = site_config.AddTemplate(
      'full',
      default_hw_tests_override,
      # Full builds are test builds to show that we can build from scratch,
      # so use settings to build from scratch, and archive the results.
      usepkg_build_packages=False,
      chrome_sdk=True,

      build_type=constants.BUILD_FROM_SOURCE_TYPE,
      archive_build_debug=True,
      images=['base', 'recovery', 'test', 'factory_install'],
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

  pfq = config_lib.BuildConfig(
      build_type=constants.PFQ_TYPE,
      important=True,
      uprev=True,
      overlays=constants.PUBLIC_OVERLAYS,
      manifest_version=True,
      trybot_list=True,
      doc='http://www.chromium.org/chromium-os/build/builder-overview#'
          'TOC-Chrome-PFQ',
      vm_tests=[constants.SMOKE_SUITE_TEST_TYPE,
                constants.SIMPLE_AU_TEST_TYPE],
      vm_tests_override=TRADITIONAL_VM_TESTS_SUPPORTED,
  )

  paladin = site_config.AddTemplate(
      'paladin',
      default_hw_tests_override,
      chroot_replace=False,
      important=True,
      build_type=constants.PALADIN_TYPE,
      overlays=constants.PUBLIC_OVERLAYS,
      prebuilts=constants.PUBLIC,
      manifest_version=True,
      trybot_list=True,
      description='Commit Queue',
      upload_standalone_images=False,
      images=['base', 'test'],
      image_test=True,
      chrome_sdk=True,
      chrome_sdk_build_chrome=False,
      doc='http://www.chromium.org/chromium-os/build/builder-overview#TOC-CQ',

      vm_tests=[constants.SMOKE_SUITE_TEST_TYPE],
      vm_tests_override=TRADITIONAL_VM_TESTS_SUPPORTED,
  )

  # Incremental builders are intended to test the developer workflow.
  # For that reason, they don't uprev.
  incremental = site_config.AddTemplate(
      'incremental',
      default_hw_tests_override,
      build_type=constants.INCREMENTAL_TYPE,
      chroot_replace=False,
      uprev=False,
      overlays=constants.PUBLIC_OVERLAYS,
      description='Incremental Builds',
      doc='http://www.chromium.org/chromium-os/build/builder-overview#'
          'TOC-Continuous',
  )

  # This builds with more source available.
  internal = config_lib.BuildConfig(
      internal=True,
      overlays=constants.BOTH_OVERLAYS,
      manifest_repo_url=site_params['MANIFEST_INT_URL'],
  )

  brillo = config_lib.BuildConfig(
      sync_chrome=False,
      chrome_sdk=False,
      afdo_use=False,
      dev_installer_prebuilts=False,
      # TODO(gauravsh): crbug.com/356414 Start running tests on Brillo configs.
      vm_tests=[],
      hw_tests=[],
  )

  moblab = config_lib.BuildConfig(
      image_test=False,
      vm_tests=[],
  )

  beaglebone = brillo.derive(image_test=False, rootfs_verification=False)

  # This adds Chrome branding.
  official_chrome = config_lib.BuildConfig(
      useflags=[constants.USE_CHROME_INTERNAL],
  )

  # This sets chromeos_official.
  official = official_chrome.derive(
      chromeos_official=True,
  )

  _cros_sdk = site_config.AddConfigWithoutTemplate(
      'chromiumos-sdk',
      full_prebuilts,
      no_hwtest_builder,
      # The amd64-host has to be last as that is when the toolchains
      # are bundled up for inclusion in the sdk.
      boards=[
          'x86-generic', 'arm-generic', 'amd64-generic'
      ],
      build_type=constants.CHROOT_BUILDER_TYPE,
      builder_class_name='sdk_builders.ChrootSdkBuilder',
      use_sdk=False,
      trybot_list=True,
      description='Build the SDK and all the cross-compilers',
      doc='http://www.chromium.org/chromium-os/build/builder-overview#'
          'TOC-Continuous',
  )

  asan = site_config.AddTemplate(
      'asan',
      default_hw_tests_override,
      profile='asan',
      disk_layout='2gb-rootfs',
      # TODO(deymo): ASan builders generate bigger files, in particular a bigger
      # Chrome binary, that update_engine can't handle in delta payloads due to
      # memory limits. Remove the following lines once crbug.com/329248 is
      # fixed.
      vm_tests=[constants.SMOKE_SUITE_TEST_TYPE],
      vm_tests_override=None,
      doc='http://www.chromium.org/chromium-os/build/builder-overview#'
          'TOC-ASAN',
  )

  llvm = site_config.AddTemplate(
      'llvm',
      default_hw_tests_override,
      profile='llvm',
      description='Build with LLVM',
  )

  telemetry = site_config.AddTemplate(
      'telemetry',
      default_hw_tests_override,
      build_type=constants.INCREMENTAL_TYPE,
      uprev=False,
      overlays=constants.PUBLIC_OVERLAYS,
      vm_tests=[constants.TELEMETRY_SUITE_TEST_TYPE],
      description='Telemetry Builds',
  )

  chromium_pfq = site_config.AddTemplate(
      'chromium-pfq',
      default_hw_tests_override,
      build_type=constants.CHROME_PFQ_TYPE,
      important=True,
      uprev=False,
      overlays=constants.PUBLIC_OVERLAYS,
      manifest_version=True,
      chrome_rev=constants.CHROME_REV_LATEST,
      chrome_sdk=True,
      description='Preflight Chromium Uprev & Build (public)',
      vm_tests=[constants.SMOKE_SUITE_TEST_TYPE,
                constants.SIMPLE_AU_TEST_TYPE],
      vm_tests_override=None,
  )

  # TODO(davidjames): Convert this to an external config once the unified master
  # logic is ready.
  internal_chromium_pfq = internal.derive(
      chromium_pfq,
      description='Preflight Chromium Uprev & Build (internal)',
      overlays=constants.BOTH_OVERLAYS,
      prebuilts=constants.PUBLIC,
      doc='http://www.chromium.org/chromium-os/build/builder-overview#'
          'TOC-Chrome-PFQ',
  )

  site_config.AddConfig(
      internal_chromium_pfq, 'master-chromium-pfq',
      boards=[],
      master=True,
      binhost_test=True,
      push_overlays=constants.BOTH_OVERLAYS,
      afdo_update_ebuild=True,
      chrome_sdk=False,
      health_alert_recipients=['chromeos-infra-eng@grotations.appspotmail.com',
                               'tree',
                               'chrome'],
  )

  chrome_pfq = site_config.AddTemplate(
      'chrome-pfq',
      internal_chromium_pfq,
      official,
      important=True,
      overlays=constants.BOTH_OVERLAYS,
      description='Preflight Chrome Uprev & Build (internal)',
      prebuilts=constants.PRIVATE,
  )

  chrome_try = config_lib.BuildConfig(
      build_type=constants.CHROME_PFQ_TYPE,
      chrome_rev=constants.CHROME_REV_TOT,
      use_lkgm=True,
      important=False,
      manifest_version=False,
  )

  chromium_info = site_config.AddTemplate(
      'chromium-pfq-informational',
      chromium_pfq,
      chrome_try,
      chrome_sdk=False,
      description='Informational Chromium Uprev & Build (public)',
  )

  chrome_info = site_config.AddTemplate(
      'chrome-pfq-informational',
      chromium_info,
      internal, official,
      description='Informational Chrome Uprev & Build (internal)',
  )

  chrome_perf = site_config.AddTemplate(
      'chrome-perf',
      chrome_info,
      no_unittest_builder,
      no_vmtest_builder,
      description='Chrome Performance test bot',
      hw_tests=[config_lib.HWTestConfig(
          'perf_v2', pool=constants.HWTEST_CHROME_PERF_POOL,
          timeout=90 * 60, critical=True, num=1)],
      use_chrome_lkgm=True,
      use_lkgm=False,
      useflags=append_useflags(['-cros-debug']),
  )


  # A base config for each board.
  _base_configs = dict()

  def _CreateBaseConfigs():
    for board in _all_boards:
      base = config_lib.BuildConfig()

      if board in _internal_boards:
        base.update(internal)
        base.update(official_chrome)
        base.update(manifest=constants.OFFICIAL_MANIFEST)
      if board in _brillo_boards:
        base.update(brillo)
      if board in _moblab_boards:
        base.update(moblab)
      if board in _minimal_profile_boards:
        base.update(profile='minimal')
      if board in _nofactory_boards:
        base.update(factory=False)
        base.update(factory_toolkit=False)
        base.update(factory_install_netboot=False)
      if board in _toolchains_from_source:
        base.update(usepkg_toolchain=False)
      if board in _noimagetest_boards:
        base.update(image_test=False)
      if board in _nohwqual_boards:
        base.update(hwqual=False)
      if board in _norootfs_verification_boards:
        base.update(rootfs_verification=False)
      if board in _base_layout_boards:
        base.update(disk_layout='base')
      if board in _no_unittest_boards:
        base.update(no_unittest_builder)
      if board in _no_vmtest_boards:
        base.update(no_vmtest_builder)
      if board in _upload_gce_images_boards:
        base.update(upload_gce_images=True)

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
      # Note: base configs should not specify a useflag list. Convert any
      # useflags that this base config has accrued (for instance,
      # 'chrome_internal', via official_chrome) into an append_useflags
      # callable. This is because the board base config is the last config to be
      # derived from when creating a board-specific config,
      if 'useflags' in board_config:
        board_config['useflags'] = append_useflags(board_config['useflags'])
      _base_configs[board] = board_config

  _CreateBaseConfigs()

  def _CreateConfigsForBoards(config_base, boards, name_suffix, **kwargs):
    """Create configs based on |config_base| for all boards in |boards|.

    Note: Existing configs will not be overwritten.

    Args:
      config_base: A BuildConfig instance to inherit from.
      boards: A set of boards to create configs for.
      name_suffix: A naming suffix. Configs will have names of the form
                   board-name_suffix.
      **kwargs: Additional keyword arguments to be used in AddConfig.
    """
    for board in boards:
      config_name = '%s-%s' % (board, name_suffix)
      if config_name not in site_config:
        base = config_lib.BuildConfig()
        config = site_config.AddConfig(config_base, config_name, base,
                                       _base_configs[board], **kwargs)
        if board in _nofactory_boards:
          try:
            config.get('images', []).remove('factory_install')
          except ValueError:
            pass


  _chromium_pfq_important_boards = frozenset([
      'arm-generic_freon',
      'arm-generic',
      'daisy',
      'veyron_minnie',
      'x86-generic',
  ])

  def _AddFullConfigs():
    """Add x86 and arm full configs."""
    external_overrides = config_lib.BuildConfig.delete_keys(internal)
    external_overrides.update(manifest=config_lib.BuildConfig.delete_key())
    external_overrides.update(
        useflags=append_useflags(['-%s' % constants.USE_CHROME_INTERNAL]))
    _CreateConfigsForBoards(full_prebuilts, _all_full_boards,
                            config_lib.CONFIG_TYPE_FULL,
                            **external_overrides)
    _CreateConfigsForBoards(chromium_info, _all_full_boards,
                            'tot-chromium-pfq-informational', important=False,
                            **external_overrides)
    # Create important configs, then non-important configs.
    _CreateConfigsForBoards(
        internal_chromium_pfq, _chromium_pfq_important_boards,
        'chromium-pfq', **external_overrides)
    _CreateConfigsForBoards(internal_chromium_pfq, _all_full_boards,
                            'chromium-pfq', important=False,
                            **external_overrides)

  _AddFullConfigs()


  # These remaining chromium pfq configs have eccentricities that are easier to
  # create manually.

  site_config.AddConfig(
      internal_chromium_pfq, 'amd64-generic-chromium-pfq',
      _base_configs['amd64-generic'],
      disk_layout='2gb-rootfs',
  )

  site_config.AddConfig(
      internal_chromium_pfq, 'amd64-generic_freon-chromium-pfq',
      _base_configs['amd64-generic_freon'],
      disk_layout='2gb-rootfs',
      vm_tests=[],
  )

  site_config.AddConfig(
      internal_chromium_pfq, 'x86-generic_freon-chromium-pfq',
      _base_configs['x86-generic_freon'],
      vm_tests=[],
  )

  _chrome_pfq_important_boards = frozenset([
      'peppy',
      'rush_ryu',
      'veyron_pinky',
      'nyan',
  ])


  # TODO(akeshet): Replace this with a config named x86-alex-chrome-pfq.
  site_config.AddConfig(
      chrome_pfq, 'alex-chrome-pfq',
      _base_configs['x86-alex'],
  )

  site_config.AddConfig(
      chrome_pfq, 'lumpy-chrome-pfq',
      _base_configs['lumpy'],
      afdo_generate=True,
      hw_tests=[HWTestList.AFDORecordTest()] + HWTestList.SharedPoolPFQ(),
  )

  site_config.AddConfig(
      chrome_pfq, 'daisy_skate-chrome-pfq',
      _base_configs['daisy_skate'],
      hw_tests=HWTestList.SharedPoolPFQ(),
  )

  site_config.AddConfig(
      chrome_pfq, 'falco-chrome-pfq',
      _base_configs['falco'],
      hw_tests=HWTestList.SharedPoolPFQ(),
  )

  site_config.AddConfig(
      chrome_pfq, 'peach_pit-chrome-pfq',
      _base_configs['peach_pit'],
      hw_tests=HWTestList.SharedPoolPFQ(),
  )

  site_config.AddConfig(
      chrome_pfq, 'tricky-chrome-pfq',
      _base_configs['tricky'],
      hw_tests=HWTestList.SharedPoolPFQ(),
  )

  _telemetry_boards = frozenset([
      'amd64-generic',
      'arm-generic',
      'x86-generic',
  ])

  _CreateConfigsForBoards(telemetry, _telemetry_boards, 'telemetry')

  _toolchain_major = site_config.AddConfigWithoutTemplate(
      'toolchain-major',
      _cros_sdk,
      latest_toolchain=True,
      prebuilts=False,
      trybot_list=False,
      gcc_githash='svn-mirror/google/main',
      description='Test next major toolchain revision',
  )

  _toolchain_minor = site_config.AddConfigWithoutTemplate(
      'toolchain-minor',
      _cros_sdk,
      latest_toolchain=True,
      prebuilts=False,
      trybot_list=False,
      gcc_githash='svn-mirror/google/gcc-4_9',
      description='Test next minor toolchain revision',
  )

  site_config.AddConfig(
      llvm,
      'amd64-generic-llvm',
      incremental,
      boards=['amd64-generic'],
      chroot_replace=True,
      description='Build with LLVM',
      trybot_list=True,
  )

  site_config.AddConfig(
      asan,
      'x86-generic-asan',
      incremental,
      boards=['x86-generic'],
      chroot_replace=True,
      description='Build with Address Sanitizer (Clang)',
      trybot_list=True,
  )

  tot_asan_info = site_config.AddTemplate(
      'tot-asan-informational',
      chromium_info,
      asan,
      description='Build TOT Chrome with Address Sanitizer (Clang)',
  )

  site_config.AddConfig(
      tot_asan_info,
      'x86-generic-tot-asan-informational',
      boards=['x86-generic'],
  )

  site_config.AddConfig(
      asan,
      'amd64-generic-asan',
      incremental,
      boards=['amd64-generic'],
      description='Build with Address Sanitizer (Clang)',
      trybot_list=True,
  )


  site_config.AddConfig(
      tot_asan_info, 'amd64-generic-tot-asan-informational',
      boards=['amd64-generic'],
  )

  incremental_beaglebone = incremental.derive(beaglebone)
  site_config.AddConfig(
      incremental_beaglebone, 'beaglebone-incremental',
      boards=['beaglebone'],
      trybot_list=True,
      description='Incremental Beaglebone Builder',
  )

  site_config.AddConfigWithoutTemplate(
      'refresh-packages',
      no_vmtest_builder,
      no_hwtest_builder,
      boards=['x86-generic', 'arm-generic'],
      builder_class_name='misc_builders.RefreshPackagesBuilder',
      description='Check upstream Gentoo for package updates',
  )

  site_config.AddConfig(
      incremental, 'x86-generic-incremental',
      _base_configs['x86-generic'],
  )

  site_config.AddConfig(
      incremental, 'daisy-incremental',
      _base_configs['daisy'],
      config_lib.BuildConfig.delete_keys(internal),
      manifest=config_lib.BuildConfig.delete_key(),
      useflags=append_useflags(['-chrome_internal']),
  )

  site_config.AddConfig(
      incremental, 'amd64-generic-incremental',
      _base_configs['amd64-generic'],
      # This builder runs on a VM, so it can't run VM tests.
      vm_tests=[],
  )

  site_config.AddConfig(
      incremental, 'x32-generic-incremental',
      _base_configs['x32-generic'],
      # This builder runs on a VM, so it can't run VM tests.
      vm_tests=[],
  )

  site_config.AddConfig(
      paladin, 'x86-generic-asan-paladin',
      _base_configs['x86-generic'],
      asan,
      description='Paladin build with Address Sanitizer (Clang)',
      important=False,
  )

  site_config.AddConfig(
      paladin, 'amd64-generic-asan-paladin',
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


  _CreateConfigsForBoards(chromium_info,
                          ['x86-generic', 'amd64-generic'],
                          'telem-chromium-pfq-informational',
                          **telemetry.derive(chrome_try))

  #
  # Internal Builds
  #

  internal_pfq = internal.derive(
      official_chrome, pfq,
      overlays=constants.BOTH_OVERLAYS,
      prebuilts=constants.PRIVATE,
  )

  # Because branch directories may be shared amongst builders on multiple
  # branches, they must delete the chroot every time they run.
  # They also potentially need to build [new] Chrome.
  internal_pfq_branch = site_config.AddTemplate(
      'pre-flight-branch',
      internal_pfq,
      branch=True,
      trybot_list=False,
      sync_chrome=True,
      active_waterfall=constants.WATERFALL_RELEASE)

  internal_paladin = internal.derive(
      official_chrome, paladin,
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

  _CreateConfigsForBoards(
      internal_nowithdebug_paladin,
      ['x86-generic', 'amd64-generic'],
      'nowithdebug-paladin',
      important=False,
  )

  site_config.AddConfig(
      internal_nowithdebug_paladin,
      'x86-mario-nowithdebug-paladin',
      boards=['x86-mario'])

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

  # falco is the only board that has the -clang-clean CFLAG right now,
  # so it's important that falco stays as a full-compile builder.
  # TODO(yunlian): Add -clang-clean to more boards.
  # See https://chromium-review.googlesource.com/#/c/275862/
  _CreateConfigsForBoards(
      full_compile_paladin,
      ['falco', 'nyan'],
      'full-compile-paladin',
  )

  pre_cq = site_config.AddTemplate(
      'pre-cq',
      paladin,
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
      vm_tests_override=None,
      description='Verifies compilation, building an image, and vm/unit tests '
                  'if supported.',
      doc='http://www.chromium.org/chromium-os/build/builder-overview#'
          'TOC-Pre-CQ',
      health_alert_recipients=['chromeos-infra-eng@grotations.appspotmail.com'],
      health_threshold=3,
  )

  # Pre-CQ targets that only check compilation and unit tests.
  unittest_only_pre_cq = pre_cq.derive(
      no_vmtest_builder,
      description='Verifies compilation and unit tests only',
      compilecheck=True,
  )

  # Pre-CQ targets that don't run VMTests.
  no_vmtest_pre_cq = site_config.AddTemplate(
      'no-vmtest-pre-cq',
      pre_cq,
      no_vmtest_builder,
      description='Verifies compilation, building an image, and unit tests '
                  'if supported.',
  )

  # Pre-CQ targets that only check compilation.
  compile_only_pre_cq = site_config.AddTemplate(
      'compile-only-pre-cq',
      unittest_only_pre_cq,
      description='Verifies compilation only',
      unittests=False,
  )

  site_config.AddConfigWithoutTemplate(
      constants.BRANCH_UTIL_CONFIG,
      internal_paladin,
      no_vmtest_builder,
      no_hwtest_builder,
      boards=[],
      # Disable postsync_patch to prevent conflicting patches from being applied
      # - e.g., patches from 'master' branch being applied to a branch.
      postsync_patch=False,
      # Disable postsync_reexec to continue running the 'master' branch chromite
      # for all stages, rather than the chromite in the branch buildroot.
      postsync_reexec=False,
      # Need to reset the paladin build_type we inherited.
      build_type=None,
      builder_class_name='release_builders.CreateBranchBuilder',
      description='Used for creating/deleting branches (TPMs only)',
  )

  # Internal incremental builders don't use official chrome because we want
  # to test the developer workflow.
  internal_incremental = internal.derive(
      incremental,
      overlays=constants.BOTH_OVERLAYS,
      description='Incremental Builds (internal)',
  )

  site_config.AddConfig(
      internal_pfq_branch, 'lumpy-pre-flight-branch',
      master=True,
      push_overlays=constants.BOTH_OVERLAYS,
      boards=['lumpy'],
      afdo_generate=True,
      afdo_update_ebuild=True,
      hw_tests=[HWTestList.AFDORecordTest()],
  )

  # A test-ap image is just a test image with a special profile enabled.
  # Note that each board enabled for test-ap use has to have the testbed-ap
  # profile linked to from its private overlay.
  _test_ap = site_config.AddTemplate(
      'test-ap',
      internal,
      default_hw_tests_override,
      description='WiFi AP images used in testing',
      profile='testbed-ap',
      vm_tests=[],
  )

  site_config.AddGroup(
      'test-ap-group',
      site_config.AddConfig(_test_ap, 'stumpy-test-ap', boards=['stumpy']),
      site_config.AddConfig(_test_ap, 'panther-test-ap', boards=['panther']),
  )

  ### Master paladin (CQ builder).

  site_config.AddConfig(
      internal_paladin, 'master-paladin',
      boards=[],
      master=True,
      binhost_test=True,
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
  # master_paladin = site_config.AddConfig(internal_paladin, ...)
  # master_paladin.AddSlave(site_config.AddConfig(internal_paladin, ...))

  # Sanity check builder, part of the CQ but builds without the patches
  # under test.
  site_config.AddConfig(
      internal_paladin, 'wolf-tot-paladin',
      boards=['wolf'],
      do_not_apply_cq_patches=True,
      prebuilts=False,
      hw_tests=HWTestList.SharedPoolCQ(),
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
      'daisy_skate',
      'daisy_spring',
      'nyan_freon',
      'falco',
      'gizmo',
      'guado_moblab',
      'kayle',
      'lakitu',
      'lakitu_mobbuild',
      'leon',
      'link',
      'lumpy',
      'monroe',
      'nyan',
      'oak',
      'panther',
      'parrot',
      'peach_pit',
      'peppy',
      'rambi',
      'rush_ryu',
      'samus',
      'smaug',
      'storm',
      'stout',
      'strago',
      'stumpy',
      'tricky',
      'veyron_pinky',
      'whirlwind',
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
      'daisy_skate',
      'link',
      'lumpy',
      'peach_pit',
      'peppy',
      'stumpy',
      'wolf',
      'x86-alex',
      'x86-zgb',
  ])

  _paladin_moblab_hwtest_boards = frozenset([
      'guado_moblab',
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
      customizations = config_lib.BuildConfig()
      base_config = _base_configs[board]
      if board in _paladin_hwtest_boards:
        customizations.update(hw_tests=HWTestList.DefaultListCQ())
      if board in _paladin_moblab_hwtest_boards:
        customizations.update(
            hw_tests=[
                config_lib.HWTestConfig(
                    constants.HWTEST_MOBLAB_QUICK_SUITE,
                    blocking=True, num=1, timeout=120*60,
                    pool=constants.HWTEST_PALADIN_POOL)
            ])
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

        if paladin.vm_tests_override is not None:
          # Make sure any new tests are also in override.
          override = paladin.vm_tests_override[:]
          for test in vm_tests:
            if test not in override:
              override.append(test)

          customizations.update(vm_tests_override=override)

      if base_config.get('internal'):
        customizations.update(
            prebuilts=constants.PRIVATE,
            description=paladin['description'] + ' (internal)')
      else:
        customizations.update(prebuilts=constants.PUBLIC)
      site_config.AddConfig(
          paladin, config_name,
          customizations,
          base_config)


  _CreatePaladinConfigs()


  site_config.AddConfig(
      internal_paladin, 'lumpy-incremental-paladin',
      boards=['lumpy'],
      build_before_patching=True,
      prebuilts=False,
      compilecheck=True,
      unittests=False,
  )

  ### Paladins (CQ builders) which do not run VM or Unit tests on the builder
  ### itself.
  external_brillo_paladin = paladin.derive(brillo)

  site_config.AddConfig(
      external_brillo_paladin, 'panther_embedded-minimal-paladin',
      boards=['panther_embedded'],
      profile='minimal',
      trybot_list=True,
  )

  internal_beaglebone_paladin = internal_paladin.derive(beaglebone)

  site_config.AddConfig(
      internal_beaglebone_paladin, 'beaglebone-paladin',
      boards=['beaglebone'],
      trybot_list=True,
  )

  site_config.AddConfig(
      internal_beaglebone_paladin, 'beaglebone_servo-paladin',
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
      assert len(site_config[name].hw_tests) == len(names), \
        '%s should have %d tests, but found %d' % (
            name, len(names), len(site_config[name].hw_tests))
    for name in names[1:]:
      for test1, test2 in zip(site_config[name].hw_tests,
                              site_config[names[0]].hw_tests):
        assert test1.__dict__ == test2.__dict__, \
            '%s and %s have different hw_tests configured' % (names[0], name)

    # Assign each config the Nth HWTest.
    for i, name in enumerate(names):
      site_config[name]['hw_tests'] = [site_config[name].hw_tests[i]]

  # Shard the bvt-inline and bvt-cq hw tests between similar builders.
  # The first builder gets bvt-inline, and the second builder gets bvt-cq.
  # bvt-cq takes longer, so it usually makes sense to give it the faster board.
  ShardHWTestsBetweenBuilders('x86-zgb-paladin', 'x86-alex-paladin')
  ShardHWTestsBetweenBuilders('wolf-paladin', 'peppy-paladin')
  ShardHWTestsBetweenBuilders('daisy_skate-paladin', 'peach_pit-paladin')
  ShardHWTestsBetweenBuilders('lumpy-paladin', 'stumpy-paladin')

  # Add a pre-cq config for every board.
  _CreateConfigsForBoards(pre_cq, _all_boards, 'pre-cq')
  # Override 'lakitu-pre-cq' - it's in _brillo_boards, but should run vmtests.
  site_config.AddConfig(
      pre_cq, 'lakitu-pre-cq',
      _base_configs['lakitu'],
      vm_tests=[constants.SMOKE_SUITE_TEST_TYPE, constants.GCE_VM_TEST_TYPE],
  )

  _CreateConfigsForBoards(no_vmtest_pre_cq, _all_boards, 'no-vmtest-pre-cq')
  _CreateConfigsForBoards(
      compile_only_pre_cq, _all_boards, 'compile-only-pre-cq')

  site_config.AddConfig(
      pre_cq, constants.BINHOST_PRE_CQ,
      no_vmtest_pre_cq,
      internal,
      boards=[],
      binhost_test=True,
  )

  # TODO(davidjames): Add peach_pit, nyan, and beaglebone to pre-cq.
  # TODO(davidjames): Update daisy_spring to build images again.
  site_config.AddGroup(
      'mixed-a-pre-cq',
      # daisy_spring w/kernel 3.8.
      site_config['daisy_spring-compile-only-pre-cq'],
      # lumpy w/kernel 3.8.
      site_config['lumpy-compile-only-pre-cq'],
  )

  site_config.AddGroup(
      'mixed-b-pre-cq',
      # arm64 w/kernel 3.14.
      site_config['rush_ryu-compile-only-pre-cq'],
      # samus w/kernel 3.14.
      site_config['samus-compile-only-pre-cq'],
  )

  site_config.AddGroup(
      'mixed-c-pre-cq',
      # brillo
      site_config['storm-compile-only-pre-cq'],
  )

  site_config.AddGroup(
      'external-mixed-pre-cq',
      site_config['x86-generic-no-vmtest-pre-cq'],
      site_config['amd64-generic-no-vmtest-pre-cq'],
  )

  site_config.AddGroup(
      'kernel-3_14-a-pre-cq',
      site_config['x86-generic-no-vmtest-pre-cq'],
      site_config['arm-generic-no-vmtest-pre-cq']
  )

  site_config.AddGroup(
      'kernel-3_14-b-pre-cq',
      site_config['storm-no-vmtest-pre-cq'],
  )

  site_config.AddGroup(
      'kernel-3_14-c-pre-cq',
      site_config['veyron_pinky-no-vmtest-pre-cq'],
      site_config['rush_ryu-no-vmtest-pre-cq']
  )

  site_config.AddConfigWithoutTemplate(
      'pre-cq-launcher',
      internal_paladin,
      no_vmtest_builder,
      no_hwtest_builder,
      boards=[],
      build_type=constants.PRE_CQ_LAUNCHER_TYPE,
      description='Launcher for Pre-CQ builders',
      trybot_list=False,
      manifest_version=False,
      # Every Pre-CQ launch failure should send out an alert.
      health_threshold=1,
      health_alert_recipients=['chromeos-infra-eng@grotations.appspotmail.com',
                               'tree'],
      doc='http://www.chromium.org/chromium-os/build/builder-overview#'
          'TOC-Pre-CQ',
  )


  site_config.AddConfig(
      internal_incremental, 'mario-incremental',
      boards=['x86-mario'],
  )

  site_config.AddConfig(
      internal_incremental, 'lakitu-incremental',
      _base_configs['lakitu'],
      vm_tests=[constants.SMOKE_SUITE_TEST_TYPE, constants.GCE_VM_TEST_TYPE],
  )

  site_config.AddConfig(
      internal_incremental, 'lakitu_next-incremental',
      _base_configs['lakitu_next'],
      vm_tests=[constants.SMOKE_SUITE_TEST_TYPE, constants.GCE_VM_TEST_TYPE],
  )

  site_config.AddConfigWithoutTemplate(
      'internal-toolchain-major',
      _toolchain_major, internal, official,
      boards=['x86-alex', 'stumpy', 'daisy', 'lakitu'],
      build_tests=True,
      description=_toolchain_major['description'] + ' (internal)',
  )

  site_config.AddConfigWithoutTemplate(
      'internal-toolchain-minor',
      _toolchain_minor, internal, official,
      boards=['x86-alex', 'stumpy', 'daisy', 'lakitu'],
      build_tests=True,
      description=_toolchain_minor['description'] + ' (internal)',
  )

  _release = site_config.AddTemplate(
      'release',
      full,
      official,
      internal,
      default_hw_tests_override,
      build_type=constants.CANARY_TYPE,
      useflags=append_useflags(['-cros-debug']),
      build_tests=True,
      afdo_use=True,
      manifest=constants.OFFICIAL_MANIFEST,
      manifest_version=True,
      images=['base', 'recovery', 'test', 'factory_install'],
      sign_types=['recovery'],
      push_image=True,
      upload_symbols=True,
      binhost_bucket='gs://chromeos-dev-installer',
      binhost_key='RELEASE_BINHOST',
      binhost_base_url='https://commondatastorage.googleapis.com/'
                       'chromeos-dev-installer',
      dev_installer_prebuilts=True,
      git_sync=False,
      vm_tests=[constants.SMOKE_SUITE_TEST_TYPE,
                constants.DEV_MODE_TEST_TYPE,
                constants.CROS_VM_TEST_TYPE],
      hw_tests=HWTestList.SharedPoolCanary(),
      paygen=True,
      signer_tests=True,
      trybot_list=True,
      hwqual=True,
      description="Release Builds (canary) (internal)",
      chrome_sdk=True,
      image_test=True,
      doc='http://www.chromium.org/chromium-os/build/builder-overview#'
          'TOC-Canaries',
  )

  _grouped_config = config_lib.BuildConfig(
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

  site_config.AddConfig(
      _release, 'master-release',
      boards=[],
      master=True,
      sync_chrome=False,
      chrome_sdk=False,
      health_alert_recipients=['chromeos-infra-eng@grotations.appspotmail.com',
                               'tree'],
      afdo_use=False,
      branch_util_test=True,
  )

  ### Release config groups.

  site_config.AddGroup(
      'x86-alex-release-group',
      site_config.AddConfig(
          _release, 'x86-alex-release',
          boards=['x86-alex'],
      ),
      site_config.AddConfig(
          _grouped_variant_release, 'x86-alex_he-release',
          boards=['x86-alex_he'],
          hw_tests=[],
          upload_hw_test_artifacts=False,
          paygen_skip_testing=True,
      ),
  )

  site_config.AddGroup(
      'x86-zgb-release-group',
      site_config.AddConfig(
          _release, 'x86-zgb-release',
          boards=['x86-zgb'],
      ),
      site_config.AddConfig(
          _grouped_variant_release, 'x86-zgb_he-release',
          boards=['x86-zgb_he'],
          hw_tests=[],
          upload_hw_test_artifacts=False,
          paygen_skip_testing=True,
      ),
  )

  ### Release AFDO configs.

  release_afdo = _release.derive(
      trybot_list=False,
      hw_tests=(
          HWTestList.DefaultList(pool=constants.HWTEST_SUITES_POOL, num=4) +
          HWTestList.AFDOList()
      ),
      push_image=False,
      paygen=False,
      dev_installer_prebuilts=False,
  )

  release_afdo_generate = site_config.AddTemplate(
      config_lib.CONFIG_TYPE_RELEASE_AFDO + '-generate',
      release_afdo,
      afdo_generate_min=True,
      afdo_use=False,
      afdo_update_ebuild=True,

      hw_tests=[HWTestList.AFDORecordTest()],
      hw_tests_override=[HWTestList.AFDORecordTest(
          num=constants.HWTEST_TRYBOT_NUM,
          pool=constants.HWTEST_TRYBOT_POOL,
          file_bugs=False,
          priority=constants.HWTEST_DEFAULT_PRIORITY,
      )],
  )

  release_afdo_use = site_config.AddTemplate(
      config_lib.CONFIG_TYPE_RELEASE_AFDO + '-use',
      release_afdo,
      afdo_use=True,
  )

  # Now generate generic release-afdo configs if we haven't created anything
  # more specific above already. release-afdo configs are builders that do AFDO
  # profile collection and optimization in the same builder. Used by developers
  # that want to measure performance changes caused by their changes.
  def _AddAFDOConfigs():
    for board in _all_release_boards:
      base = _base_configs[board]

      config_name = '%s-%s' % (board, config_lib.CONFIG_TYPE_RELEASE_AFDO)
      if config_name in site_config:
        continue

      generate_config_name = (
          '%s-%s-%s' % (board,
                        config_lib.CONFIG_TYPE_RELEASE_AFDO,
                        'generate'))
      use_config_name = '%s-%s-%s' % (board,
                                      config_lib.CONFIG_TYPE_RELEASE_AFDO,
                                      'use')

      # We can't use AFDO data if afdo_use is disabled for this board.
      if not base.get('afdo_use', True):
        continue

      site_config.AddGroup(
          config_name,
          site_config.AddConfig(
              release_afdo_generate, generate_config_name, base
          ),
          site_config.AddConfig(
              release_afdo_use, use_config_name, base
          ),
      )

  _AddAFDOConfigs()

  ### Release configs.

  _critical_for_chrome_boards = frozenset([
      'daisy',
      'lumpy',
      'parrot',
  ])

  # bayleybay-release does not enable vm_tests or unittests due to the compiler
  # flags enabled for baytrail.
  site_config.AddConfig(
      _release, 'bayleybay-release',
      boards=['bayleybay'],
      hw_tests=[],
      vm_tests=[],
      unittests=False,
  )

  site_config.AddConfig(
      _release, 'beltino-release',
      boards=['beltino'],
      hw_tests=[],
      vm_tests=[],
  )

  # bobcat-release does not enable vm_tests or unittests due to the compiler
  # flags enabled for baytrail.
  site_config.AddConfig(
      _release, 'bobcat-release',
      boards=['bobcat'],
      hw_tests=[],
      profile='minimal',
      # This build doesn't generate signed images, so don't try to release them.
      paygen=False,
      signer_tests=False,
  )

  site_config.AddConfig(
      _release, 'gizmo-release',
      _base_configs['gizmo'],
      important=True,
      paygen=False,
      signer_tests=False,
  )

  site_config.AddConfig(
      _release, 'samus-release',
      _base_configs['samus'],
      important=True,
  )

  ### Arm release configs.

  site_config.AddConfig(
      _release, 'veyron_rialto-release',
      _base_configs['veyron_rialto'],
      # rialto does not use Chrome.
      sync_chrome=False,
      chrome_sdk=False,
  )

  site_config.AddConfig(
      _release, 'smaug-release',
      _base_configs['smaug'],
      images=['base', 'recovery', 'test'],
      sign_types=['nv_lp0_firmware'],
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
        _release, _critical_for_chrome_boards, config_lib.CONFIG_TYPE_RELEASE,
        critical_for_chrome=True)
    _CreateConfigsForBoards(
        _release, _all_release_boards, config_lib.CONFIG_TYPE_RELEASE)

  _AddReleaseConfigs()

  site_config.AddConfig(
      _release, 'panther_embedded-minimal-release',
      _base_configs['panther_embedded'],
      profile='minimal',
      important=True,
      paygen=False,
      signer_tests=False,
  )

  # beaglebone build doesn't generate signed images, so don't try to release
  # them.
  _beaglebone_release = _release.derive(beaglebone, paygen=False,
                                        signer_tests=False,
                                        images=['base', 'test'])

  site_config.AddGroup(
      'beaglebone-release-group',
      site_config.AddConfig(
          _beaglebone_release, 'beaglebone-release',
          boards=['beaglebone'],
      ),
      site_config.AddConfig(
          _beaglebone_release, 'beaglebone_servo-release',
          boards=['beaglebone_servo'],
          payload_image='base'
      ).derive(_grouped_variant_config),
      important=True,
  )

  site_config.AddConfig(
      _release, 'kayle-release',
      _base_configs['kayle'],
      paygen=False,
      signer_tests=False,
  )

  site_config.AddConfig(
      _release, 'storm-release',
      _base_configs['storm'],

      # Hw Lab can't test storm, yet.
      paygen_skip_testing=True,
      signer_tests=False,
  )

  moblab_release = site_config.AddTemplate(
      'moblab-release',
      _release,
      description='Moblab release builders',
      images=['base', 'recovery', 'test'],
      paygen_skip_delta_payloads=True,
      # TODO: re-enable paygen testing when crbug.com/386473 is fixed.
      paygen_skip_testing=True,
      important=False,
      afdo_use=False,
      signer_tests=False,
      hw_tests=[
          config_lib.HWTestConfig(constants.HWTEST_MOBLAB_SUITE, blocking=True,
                                  num=1, timeout=120*60),
          config_lib.HWTestConfig(constants.HWTEST_BVT_SUITE, blocking=True,
                                  warn_only=True, num=1),
          config_lib.HWTestConfig(constants.HWTEST_AU_SUITE, blocking=True,
                                  warn_only=True, num=1)],
  )

  site_config.AddConfig(
      moblab_release, 'stumpy_moblab-release',
      _base_configs['stumpy_moblab'],
  )

  site_config.AddConfig(
      moblab_release, 'guado_moblab-release',
      _base_configs['guado_moblab'],
  )

  site_config.AddConfig(
      moblab_release, 'panther_moblab-release',
      _base_configs['panther_moblab'],
  )

  site_config.AddConfig(
      _release, 'rush-release',
      _base_configs['rush'],
      hw_tests=[],
      # This build doesn't generate signed images, so don't try to release them.
      paygen=False,
      signer_tests=False,
  )

  site_config.AddConfig(
      _release, 'rush_ryu-release',
      _base_configs['rush_ryu'],
      images=['base', 'test', 'factory_install'],
      dev_installer_prebuilts=False,
      paygen=False,
      signer_tests=False,
      push_image=False,
      hw_tests=[],
  )

  site_config.AddConfig(
      _release, 'veyron_mickey-release',
      _base_configs['veyron_mickey'],
      hw_tests=[],
      vm_tests=[],
  )

  site_config.AddConfig(
      _release, 'veyron_romy-release',
      _base_configs['veyron_romy'],
      hw_tests=[],
      vm_tests=[],
  )

  site_config.AddConfig(
      _release, 'whirlwind-release',
      _base_configs['whirlwind'],
      dev_installer_prebuilts=True,
  )

  site_config.AddConfig(
      _release, 'lakitu-release',
      _base_configs['lakitu'],
      vm_tests=[constants.SMOKE_SUITE_TEST_TYPE, constants.GCE_VM_TEST_TYPE],
      sign_types=['base'],
      important=True,
  )

  site_config.AddConfig(
      _release, 'lakitu_mobbuild-release',
      _base_configs['lakitu_mobbuild'],
      vm_tests=[constants.SMOKE_SUITE_TEST_TYPE, constants.GCE_VM_TEST_TYPE],
      signer_tests=False,
      important=True,
  )

  site_config.AddConfig(
      _release, 'lakitu_next-release',
      _base_configs['lakitu_next'],
      vm_tests=[constants.SMOKE_SUITE_TEST_TYPE, constants.GCE_VM_TEST_TYPE],
      signer_tests=False,
  )

  _wificell_pre_cq = site_config.AddTemplate(
      'wificell-pre-cq',
      pre_cq,
      unittests=False,
      hw_tests=HWTestList.WiFiCellPoolPreCQ(),
      hw_tests_override=HWTestList.WiFiCellPoolPreCQ(),
      archive=True,
      image_test=False,
      description='WiFi tests acting as pre-cq for WiFi related changes',
  )

  site_config.AddGroup(
      'mixed-wificell-pre-cq',
      site_config.AddConfig(
          _wificell_pre_cq,
          'winky-wificell-pre-cq',
          _base_configs['winky']),
      site_config.AddConfig(
          _wificell_pre_cq,
          'veyron_speedy-wificell-pre-cq',
          _base_configs['veyron_speedy']),
      site_config.AddConfig(
          _wificell_pre_cq,
          'veyron_jerry-wificell-pre-cq',
          _base_configs['veyron_jerry']),
      site_config.AddConfig(
          _wificell_pre_cq,
          'daisy-wificell-pre-cq',
          _base_configs['daisy']),
  )

  ### Per-chipset release groups

  def _AddGroupConfig(name, base_board, group_boards=None,
                      group_variant_boards=None, **kwargs):
    """Generate full & release group configs."""
    def _boards_list(x):
      # Make sure _boards_list is a valid list (not None or tuple)
      return [] if x is None else list(x)

    group_boards = _boards_list(group_boards)
    group_variant_boards = _boards_list(group_variant_boards)

    for group in ('release', 'full'):
      configs = []

      all_boards = [base_board] + group_boards + group_variant_boards
      desc = '%s; Group config (boards: %s)' % (
          site_config['%s-%s' % (base_board, group)].description,
          ', '.join(all_boards))

      for board in all_boards:
        if board in group_boards:
          subconfig = _grouped_config
        elif board in group_variant_boards:
          subconfig = _grouped_variant_config
        else:
          subconfig = {}
        board_config = '%s-%s' % (board, group)
        configs.append(site_config[board_config].derive(subconfig, **kwargs))

        config_name = '%s-%s-group' % (name, group)
        important = group == 'release' and kwargs.get('important', True)
      site_config.AddGroup(
          config_name, *configs, description=desc,
          important=important
      )

  # pineview chipset boards
  _AddGroupConfig(
      'pineview', 'x86-mario', (
          'x86-alex',
          'x86-zgb',
      ), (
          'x86-alex_he',
          'x86-zgb_he',
      )
  )

  # sandybridge chipset boards
  _AddGroupConfig(
      'sandybridge', 'parrot', (
          'lumpy',
          'butterfly',
          'stumpy',
      )
  )

  # ivybridge chipset boards
  _AddGroupConfig(
      'ivybridge', 'stout', (
          'link',
      ), (
          'parrot_ivb',
      )
  )

  # slippy-based haswell boards
  # TODO(davidjames): Combine slippy and beltino into haswell canary, once we've
  # optimized our builders more.
  # slippy itself is deprecated in favor of the below boards, so we don't bother
  # building it.
  # TODO(dnj): Re-add peppy canary once builders are allocated.
  _AddGroupConfig(
      'slippy', 'peppy', (
          'falco',
          'leon',
          'wolf',
      ), (
          'falco_li',
      )
  )

  # beltino-based haswell boards
  # beltino itself is deprecated in favor of the below boards, so we don't
  # bother building it.

  _AddGroupConfig(
      'beltino-a', 'panther', (
          'mccloud',
      )
  )

  _AddGroupConfig(
      'beltino-b', 'monroe', (
          'tricky',
          'zako',
      )
  )

  # rambi-based boards
  _AddGroupConfig(
      'rambi-a', 'rambi', (
          'clapper',
          'enguarde',
          'expresso',
      )
  )

  _AddGroupConfig(
      'rambi-b', 'glimmer', (
          'gnawty',
          'kip',
          'quawks',
      )
  )

  _AddGroupConfig(
      'rambi-c', 'squawks', (
          'swanky',
          'winky',
          'candy',
      )
  )

  _AddGroupConfig(
      'rambi-d', 'banjo', (
          'ninja',
          'sumo',
      ),
  )

  _AddGroupConfig(
      'rambi-e', 'orco', (
          'heli',
          'wizpig',
      ),
  )

  # daisy-based boards
  _AddGroupConfig(
      'daisy', 'daisy', (
          'daisy_spring',
          'daisy_skate',
      ),
  )

  # peach-based boards
  _AddGroupConfig(
      'peach', 'peach_pit', (
          'peach_pi',
      )
  )

  # nyan-based boards
  _AddGroupConfig(
      'nyan', 'nyan', (
          'nyan_big',
          'nyan_blaze',
          'nyan_kitty',
      )
  )

  # auron-based boards
  _AddGroupConfig(
      'auron', 'auron', (
          'auron_yuna',
          'auron_paine',
      )
  )

  _AddGroupConfig(
      'auron-b', 'lulu', (
          'gandof',
          'buddy',
      ),
  )

  # veyron-based boards
  _AddGroupConfig(
      'veyron', 'veyron_pinky', (
          'veyron_jerry',
          'veyron_mighty',
          'veyron_speedy'
      ),
  )

  _AddGroupConfig(
      'veyron-b', 'veyron_gus', (
          'veyron_jaq',
          'veyron_minnie',
          'veyron_rialto',
      ),
  )

  _AddGroupConfig(
      'veyron-c', 'veyron_brain', (
          'veyron_danger',
          'veyron_thea',
          'veyron_shark',
      ),
      important=False,
  )

  _AddGroupConfig(
      'veyron-d', 'veyron_mickey', (
          'veyron_romy',
      ),
  )

  # jecht-based boards
  _AddGroupConfig(
      'jecht', 'jecht', (
          'guado',
          'tidus',
          'rikku',
      )
  )

  # strago-based boards
  _AddGroupConfig(
      'strago', 'strago', (
          'cyan',
          'celes',
          'ultima',
      ),
      important=False,
  )

  # oak-based boards
  _AddGroupConfig(
      'oak', 'oak', (
      )
  )

  # glados-based boards
  _AddGroupConfig(
      'glados', 'glados', (
      ),
  )

  # storm-based boards
  _AddGroupConfig(
      'storm', 'storm', (
          'arkham',
          'whirlwind',
      ),
      important=False,
  )

  # kunimitsu-based boards
  _AddGroupConfig(
      'kunimitsu', 'kunimitsu', (
      ),
  )

  # Factory and Firmware releases much inherit from these classes.
  # Modifications for these release builders should go here.

  # Naming conventions also must be followed. Factory and firmware branches
  # must end in -factory or -firmware suffixes.

  _factory_release = site_config.AddTemplate(
      'factory',
      _release,
      upload_hw_test_artifacts=False,
      upload_symbols=False,
      hw_tests=[],
      chrome_sdk=False,
      description='Factory Builds',
      paygen=False,
      afdo_use=False,
      sign_types=['factory'],
  )

  _firmware = config_lib.BuildConfig(
      no_vmtest_builder,
      images=[],
      factory_toolkit=False,
      packages=['virtual/chromeos-firmware', 'chromeos-base/autotest-all'],
      usepkg_build_packages=True,
      sync_chrome=False,
      build_tests=True,
      chrome_sdk=False,
      unittests=False,
      hw_tests=[],
      dev_installer_prebuilts=False,
      upload_hw_test_artifacts=True,
      upload_symbols=False,
      useflags=['chromeless_tty'],
      signer_tests=False,
      trybot_list=False,
      paygen=False,
      image_test=False,
      sign_types=['firmware'],
  )

  _firmware_release = site_config.AddTemplate(
      'firmware',
      _release,
      _firmware,
      description='Firmware Canary',
      manifest=constants.DEFAULT_MANIFEST,
      afdo_use=False,
  )

  _depthcharge_release = site_config.AddTemplate(
      'depthcharge-firmware',
      _firmware_release,
      useflags=append_useflags(['depthcharge']))

  _depthcharge_full_internal = site_config.AddTemplate(
      'depthcharge-full-firmware',
      full,
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
      'cyan',
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
      'ninja',
      'orco',
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
      'smaug',
      'squawks',
      'storm',
      'stout',
      'strago',
      'stumpy',
      'sumo',
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
      'cyan',
      'enguarde',
      'expresso',
      'glados',
      'glimmer',
      'gnawty',
      'heli',
      'jecht',
      'kip',
      'kunimitsu',
      'leon',
      'link',
      'ninja',
      'orco',
      'parry',
      'quawks',
      'rambi',
      'rikku',
      'samus',
      'squawks',
      'strago',
      'sumo',
      'swanky',
      'winky',
      'zako',
  ])


  def _AddFirmwareConfigs():
    """Add x86 and arm firmware configs."""
    for board in _firmware_boards:
      site_config.AddConfig(
          _firmware_release,
          '%s-%s' % (board, config_lib.CONFIG_TYPE_FIRMWARE),
          _base_configs[board],
          no_vmtest_builder,
      )

    for board in _x86_depthcharge_firmware_boards:
      site_config.AddConfig(
          _depthcharge_release,
          '%s-%s-%s' % (board, 'depthcharge', config_lib.CONFIG_TYPE_FIRMWARE),
          _base_configs[board],
          no_vmtest_builder,
      )
      site_config.AddConfig(
          _depthcharge_full_internal,
          '%s-%s-%s-%s' % (board, 'depthcharge', config_lib.CONFIG_TYPE_FULL,
                           config_lib.CONFIG_TYPE_FIRMWARE),
          _base_configs[board],
          no_vmtest_builder,
      )

  _AddFirmwareConfigs()


  # This is an example factory branch configuration.
  # Modify it to match your factory branch.
  site_config.AddConfig(
      _factory_release, 'x86-mario-factory',
      boards=['x86-mario'],
  )

  _payloads = site_config.AddTemplate(
      'payloads',
      internal,
      no_vmtest_builder,
      no_unittest_builder,
      no_hwtest_builder,
      build_type=constants.PAYLOADS_TYPE,
      builder_class_name='release_builders.GeneratePayloadsBuilder',
      description='Regenerate release payloads.',

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
    for _, search_config in site_config.iteritems():
      _search_config_and_children(search_config)

    # Generate a payloads trybot config for every board that generates payloads.
    for board in payload_boards:
      name = '%s-payloads' % board
      site_config.AddConfig(_payloads, name, boards=[board])

  _AddPayloadConfigs()

  # Add special builders to help with cbuidlbot development/testing.
  site_config.Add(
      'sync-test-cbuildbot',
      no_hwtest_builder,
      boards=[],
      builder_class_name='test_builders.ManifestVersionedSyncBuilder',
      chroot_replace=True,
  )

  def _SetupWaterfalls():
    for name, c in site_config.iteritems():
      if not c.get('active_waterfall'):
        c['active_waterfall'] = GetDefaultWaterfall(c)

    # Apply manual configs.
    for waterfall, names in _waterfall_config_map.iteritems():
      for name in names:
        site_config[name]['active_waterfall'] = waterfall

  _SetupWaterfalls()


  def _InsertHwTestsOverrideDefaults(build):
    """Insert default hw_tests values for a given build.

    Also updates child builds.

    Args:
      build: BuildConfig instance to modify in place.
    """
    for child in build['child_configs']:
      _InsertHwTestsOverrideDefaults(child)

    if build['hw_tests_override'] is not None:
      # Explicitly set, no need to insert defaults.
      return

    if not build['hw_tests']:
      build['hw_tests_override'] = HWTestList.DefaultList(
          num=constants.HWTEST_TRYBOT_NUM, pool=constants.HWTEST_TRYBOT_POOL,
          file_bugs=False)
    else:
      # Copy over base tests.
      build['hw_tests_override'] = [copy.copy(x) for x in build['hw_tests']]

      # Adjust for manual test environment.
      for hw_config in build['hw_tests_override']:
        hw_config.num = constants.HWTEST_TRYBOT_NUM
        hw_config.pool = constants.HWTEST_TRYBOT_POOL
        hw_config.file_bugs = False
        hw_config.priority = constants.HWTEST_DEFAULT_PRIORITY

    # TODO: Fix full_release_test.py/AUTest on trybots, crbug.com/390828.
    build['hw_tests_override'] = [
        hw_config for hw_config in build['hw_tests_override']
        if hw_config.suite != constants.HWTEST_AU_SUITE]

  for build in site_config.itervalues():
    _InsertHwTestsOverrideDefaults(build)

  return site_config
