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


def GetDefaultWaterfall(build_config):
  if not (build_config['important'] or build_config['master']):
    return None
  if build_config['branch']:
    return None
  b_type = build_config['build_type']

  # Build types that, absent other cues, go on the internal waterfall.
  INTERNAL_TYPES = (constants.PRE_CQ_LAUNCHER_TYPE, constants.TOOLCHAIN_TYPE)

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
  elif config_lib.IsPFQType(b_type) or b_type in INTERNAL_TYPES:
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
    # Force au suite to run first.
    au_kwargs['priority'] = constants.HWTEST_CQ_PRIORITY

    async_kwargs = kwargs.copy()
    async_kwargs.update(async_dict)
    async_kwargs['priority'] = constants.HWTEST_POST_BUILD_PRIORITY
    async_kwargs['async'] = True
    async_kwargs['suite_min_duts'] = 1
    async_kwargs['timeout'] = config_lib.HWTestConfig.ASYNC_HW_TEST_TIMEOUT

    if IS_RELEASE_BRANCH:
      bvt_inline_kwargs = async_kwargs
    else:
      bvt_inline_kwargs = kwargs.copy()
      bvt_inline_kwargs['timeout'] = 120 * 60

    # BVT + AU suite.
    return [config_lib.HWTestConfig(constants.HWTEST_BVT_SUITE,
                                    **bvt_inline_kwargs),
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
    default_dict = dict(pool=constants.HWTEST_PALADIN_POOL,
                        timeout=config_lib.HWTestConfig.PALADIN_HW_TEST_TIMEOUT,
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
  def DefaultListAndroidPFQ(cls, **kwargs):
    """Return a default list of HWTestConfig's for a PFQ build.

    Optional arguments may be overridden in `kwargs`, except that
    the `blocking` setting cannot be provided.
    """
    default_dict = dict(file_bugs=True,
                        timeout=config_lib.HWTestConfig.ASYNC_HW_TEST_TIMEOUT,
                        priority=constants.HWTEST_PFQ_PRIORITY,
                        retry=True, max_retries=None, minimum_duts=4)
    # Allows kwargs overrides to default_dict for pfq.
    default_dict.update(kwargs)

    # TODO(crbug.com/610807): Disable the HWTests for now, since we are having
    # issues getting them to run and complete in time.
    # return [config_lib.HWTestConfig(constants.HWTEST_COMMIT_SUITE,
    #                                num=8, pool=constants.HWTEST_MACH_POOL,
    #                                **default_dict),
    return [config_lib.HWTestConfig(constants.HWTEST_ARC_COMMIT_SUITE,
                                    num=4, pool=constants.HWTEST_MACH_POOL,
                                    **default_dict)]

  @classmethod
  def SharedPoolAndroidPFQ(cls, **kwargs):
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
    default_dict = dict(suite_min_duts=3)
    default_dict.update(kwargs)
    suite_list = [config_lib.HWTestConfig(constants.HWTEST_SANITY_SUITE,
                                          **sanity_dict)]
    suite_list.extend(HWTestList.DefaultListAndroidPFQ(**default_dict))
    return suite_list

  @classmethod
  def SharedPoolCQ(cls, **kwargs):
    """Return a list of HWTestConfigs for CQ which uses a shared pool.

    The returned suites will run in pool:critical by default, which is
    shared with other types of builder (canaries, pfq). The first suite in the
    list is a blocking sanity suite that verifies the build will not break dut.
    """
    sanity_dict = dict(pool=constants.HWTEST_MACH_POOL,
                       timeout=config_lib.HWTestConfig.SHARED_HW_TEST_TIMEOUT,
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
                        file_bugs=False,
                        priority=constants.HWTEST_DEFAULT_PRIORITY,
                        retry=False, max_retries=None, minimum_duts=1)
    default_dict.update(kwargs)
    suite_list = [config_lib.HWTestConfig(constants.WIFICELL_PRE_CQ,
                                          **default_dict)]
    return suite_list

  @classmethod
  def AsanTest(cls, **kwargs):
    """Return a list of HWTESTConfigs which run asan tests."""
    default_dict = dict(pool=constants.HWTEST_MACH_POOL, file_bugs=False,
                        priority=constants.HWTEST_DEFAULT_PRIORITY)
    default_dict.update(kwargs)
    return [config_lib.HWTestConfig(constants.HWTEST_BVT_SUITE,
                                    **default_dict),
            config_lib.HWTestConfig(constants.HWTEST_COMMIT_SUITE,
                                    **default_dict)]
  @classmethod
  def ToolchainTest(cls, **kwargs):
    """Return a list of HWTESTConfigs which run toolchain correctness tests."""
    default_dict = dict(pool=constants.HWTEST_MACH_POOL, file_bugs=False,
                        priority=constants.HWTEST_DEFAULT_PRIORITY)
    default_dict.update(kwargs)
    return [config_lib.HWTestConfig(constants.HWTEST_BVT_SUITE,
                                    **default_dict),
            config_lib.HWTestConfig(constants.HWTEST_COMMIT_SUITE,
                                    **default_dict),
            config_lib.HWTestConfig(constants.HWTEST_TOOLCHAIN_SUITE,
                                    **default_dict),
            config_lib.HWTestConfig('paygen_au_canary',
                                    **default_dict),
            config_lib.HWTestConfig('security',
                                    **default_dict),
            config_lib.HWTestConfig('kernel_daily_regression',
                                    **default_dict),
            config_lib.HWTestConfig('kernel_daily_benchmarks',
                                    **default_dict)]



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


TRADITIONAL_VM_TESTS_SUPPORTED = [
    config_lib.VMTestConfig(constants.SMOKE_SUITE_TEST_TYPE),
    config_lib.VMTestConfig(constants.SIMPLE_AU_TEST_TYPE),
    config_lib.VMTestConfig(constants.CROS_VM_TEST_TYPE)]

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
    'elm',
    'elm-cheets',
    'gale',
    'gru',
    'kevin',
    'nyan',
    'nyan_big',
    'nyan_blaze',
    'nyan_freon',
    'nyan_kitty',
    'oak',
    'oak-cheets',
    'peach_pi',
    'peach_pit',
    'purin',
    'smaug',
    'smaug-cheets',
    'smaug-kasan',
    'storm',
    'veyron_fievel',
    'veyron_jaq',
    'veyron_jerry',
    'veyron_mickey',
    'veyron_mighty',
    'veyron_minnie',
    'veyron_minnie-cheets',
    'veyron_pinky',
    'veyron_rialto',
    'veyron_shark',
    'veyron_speedy',
    'veyron_tiger',
    'whirlwind',
])

_arm_external_boards = frozenset([
    'arm-generic',
    'arm-generic_freon',
    'arm64-generic',
    'arm64-llvmpipe',
])

_x86_internal_release_boards = frozenset([
    'amd64-generic-goofy',
    'amenia',
    'asuka',
    'auron',
    'auron_paine',
    'auron_yuna',
    'banjo',
    'banon',
    'buddy',
    'butterfly',
    'candy',
    'cave',
    'celes',
    'celes-cheets',
    'chell',
    'chell-cheets',
    'cid',
    'clapper',
    'cranky',
    'cyan',
    'cyan-cheets',
    'edgar',
    'enguarde',
    'expresso',
    'falco',
    'falco_li',
    'gandof',
    'glados',
    'glados-cheets',
    'glimmer',
    'glimmer-cheets',
    'gnawty',
    'guado',
    'guado_labstation',
    'guado_moblab',
    'heli',
    'jecht',
    'kefka',
    'kip',
    'kunimitsu',
    'lakitu',
    'lakitu_next',
    'lars',
    'leon',
    'link',
    'lulu',
    'lulu-cheets',
    'lumpy',
    'mccloud',
    'monroe',
    'ninja',
    'orco',
    'panther',
    'parrot',
    'parrot_ivb',
    'parry',
    'peppy',
    'quawks',
    'rambi',
    'reef',
    'reks',
    'relm',
    'rikku',
    'samus',
    'samus-cheets',
    'sentry',
    'setzer',
    'slippy',
    'squawks',
    'stout',
    'strago',
    'stumpy',
    'sumo',
    'swanky',
    'terra',
    'tidus',
    'tricky',
    'ultima',
    'umaro',
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
    'x32-generic',
    'x86-generic',
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
    'gale',
    'purin',
    'storm',
    'whirlwind',
])

_cheets_boards = frozenset([
    'cyan-cheets',
    'elm-cheets',
    'glados-cheets',
    'glimmer-cheets',
    'oak-cheets',
    'samus-cheets',
    'smaug-cheets',
    'veyron_minnie-cheets',
    'celes-cheets',
    'chell-cheets',
    'lulu-cheets',
])

_lakitu_boards = frozenset([
    'lakitu',
    'lakitu_next',
])

_moblab_boards = frozenset([
    'guado_moblab',
])

_nofactory_boards = _lakitu_boards | frozenset([
    'smaug',
])

_toolchains_from_source = frozenset([
    'x32-generic',
])

_noimagetest_boards = _lakitu_boards

_nohwqual_boards = _lakitu_boards

_norootfs_verification_boards = frozenset([
])

_base_layout_boards = _lakitu_boards

_no_unittest_boards = frozenset((
))

_no_vmtest_boards = _arm_boards | _brillo_boards | frozenset((
    'cyan-cheets',
    'elm-cheets',
    'glados-cheets',
    'glimmer-cheets',
    'oak-cheets',
    'samus-cheets',
    'smaug-cheets',
    'celes-cheets',
    'chell-cheets',
    'lulu-cheets',
))

# This is a list of configs that should be included on the main waterfall, but
# aren't included by default (see IsDefaultMainWaterfall). This loosely
# corresponds to the set of experimental or self-standing configs.
_waterfall_config_map = {
    constants.WATERFALL_EXTERNAL: frozenset([
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

    ]),

    constants.WATERFALL_INTERNAL: frozenset([
        # Experimental Paladins.
        'elm-cheets-paladin',
        'lakitu_next-paladin',

        # Experimental Canaries
        # auron
        'lulu-cheets-release',
        # glados
        'glados-release',
        'chell-release',
        'glados-cheets-release',
        'cave-release',
        'chell-cheets-release',
        'asuka-release',
        # reef
        'reef-release',
        'amenia-release',
        # storm
        'storm-release',
        'arkham-release',
        'whirlwind-release',
        # strago
        'celes-cheets-release',
        'kefka-release',
        'relm-release',
        # veyron
        'veyron_mickey-release',
        'veyron_tiger-release',
        'veyron_shark-release',
        'veyron_minnie-cheets-release',
        'veyron_fievel-release',
        # other
        'amd64-generic-goofy-release',
        'gale-release',
        'glimmer-cheets-release',
        'lakitu_next-release',
        'nyan_freon-release',
        'smaug-release',
        'smaug-cheets-release',
        'guado_moblab-release',
        'guado_labstation-release',

        # KASAN
        'smaug-kasan-kernel-3_18',

        # Incremental Builders.
        'mario-incremental',
        'lakitu-incremental',
        'lakitu_next-incremental',

        # Firmware Builders.
        'link-depthcharge-full-firmware',

        # Toolchain Builders.
        'gcc-toolchain-group',

        # LLVM
        'llvm-toolchain-group',
        'llvm-next-toolchain-group',
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
          aosp_remote: site_params['AOSP_GOB_URL'],
          weave_remote: site_params['WEAVE_GOB_URL']
      },
      GIT_REMOTES={
          chromium_remote: site_params['EXTERNAL_GOB_URL'],
          chrome_remote: site_params['INTERNAL_GOB_URL'],
          external_remote: site_params['EXTERNAL_GOB_URL'],
          internal_remote: site_params['INTERNAL_GOB_URL'],
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
          external_remote: r'(chromiumos|aosp)/(.+)',
          internal_remote: r'chromeos/(.+)',
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

  default_paladin_hw_tests_override = config_lib.BuildConfig(
      hw_tests_override=HWTestList.DefaultListNonCanary(
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
      run_gce_tests=False,
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
      build_timeout=20 * 60,
      important=True,
      uprev=True,
      overlays=constants.PUBLIC_OVERLAYS,
      manifest_version=True,
      trybot_list=True,
      doc='http://www.chromium.org/chromium-os/build/builder-overview#'
          'TOC-Chrome-PFQ',
      vm_tests=[config_lib.VMTestConfig(constants.SMOKE_SUITE_TEST_TYPE),
                config_lib.VMTestConfig(constants.SIMPLE_AU_TEST_TYPE)],
      vm_tests_override=TRADITIONAL_VM_TESTS_SUPPORTED,
  )

  paladin = site_config.AddTemplate(
      'paladin',
      default_paladin_hw_tests_override,
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

      vm_tests=[config_lib.VMTestConfig(constants.SMOKE_SUITE_TEST_TYPE)],
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
  )

  lakitu = config_lib.BuildConfig(
      sync_chrome=False,
      chrome_sdk=False,
      afdo_use=False,
      dev_installer_prebuilts=False,
      vm_tests=[],
      vm_tests_override=None,
      hw_tests=[],
  )

  # An anchor of Laktiu' test customizations.
  lakitu_test_customizations = config_lib.BuildConfig(
      vm_tests=[config_lib.VMTestConfig(constants.SMOKE_SUITE_TEST_TYPE)],
      run_gce_tests=True,
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

  _cros_sdk = site_config.AddWithoutTemplate(
      'chromiumos-sdk',
      full_prebuilts,
      no_hwtest_builder,
      # The amd64-host has to be last as that is when the toolchains
      # are bundled up for inclusion in the sdk.
      boards=[
          'x86-generic', 'arm-generic', 'amd64-generic'
      ],
      build_type=constants.CHROOT_BUILDER_TYPE,
      buildslave_type=constants.BAREMETAL_BUILD_SLAVE_TYPE,
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
      vm_tests=[config_lib.VMTestConfig(constants.SMOKE_SUITE_TEST_TYPE)],
      vm_tests_override=None,
      doc='http://www.chromium.org/chromium-os/build/builder-overview#'
          'TOC-ASAN',
  )

  telemetry = site_config.AddTemplate(
      'telemetry',
      default_hw_tests_override,
      build_type=constants.INCREMENTAL_TYPE,
      uprev=False,
      overlays=constants.PUBLIC_OVERLAYS,
      vm_tests=[config_lib.VMTestConfig(constants.TELEMETRY_SUITE_TEST_TYPE,
                                        # Add an extra 30 minutes.
                                        timeout=90 * 60)],
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
      unittests=False,
      description='Preflight Chromium Uprev & Build (public)',
      vm_tests=[config_lib.VMTestConfig(constants.SMOKE_SUITE_TEST_TYPE),
                config_lib.VMTestConfig(constants.SIMPLE_AU_TEST_TYPE)],
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

  site_config.Add(
      'master-chromium-pfq',
      internal_chromium_pfq,
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
      important=False,
      manifest_version=False,
  )

  chromium_info = site_config.AddTemplate(
      'chromium-pfq-informational',
      chromium_pfq,
      chrome_try,
      chrome_sdk=False,
      unittests=False,
      description='Informational Chromium Uprev & Build (public)',
  )

  chromium_info_gn = site_config.AddTemplate(
      'chromium-pfq-informational-gn',
      chromium_info,
      useflags=append_useflags(['gn']),
  )

  chrome_info = site_config.AddTemplate(
      'chrome-pfq-informational',
      chromium_info,
      internal, official,
      unittests=False,
      description='Informational Chrome Uprev & Build (internal)',
  )

  chrome_info_cheets = site_config.AddTemplate(
      'chrome-pfq-cheets-informational',
      chrome_info,
      hw_tests=HWTestList.SharedPoolAndroidPFQ(),
      hw_tests_override=HWTestList.SharedPoolAndroidPFQ()
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
      useflags=append_useflags(['-cros-debug']),
  )

  android_pfq = site_config.AddTemplate(
      'android-pfq',
      default_hw_tests_override,
      build_type=constants.ANDROID_PFQ_TYPE,
      important=True,
      uprev=False,
      overlays=constants.BOTH_OVERLAYS,
      manifest_version=True,
      android_rev=constants.ANDROID_REV_LATEST,
      description='Preflight Android Uprev & Build (internal)',
      vm_tests=[config_lib.VMTestConfig(constants.SMOKE_SUITE_TEST_TYPE),
                config_lib.VMTestConfig(constants.SIMPLE_AU_TEST_TYPE)],
      vm_tests_override=None,
  )

  site_config.Add(
      'master-android-pfq',
      android_pfq,
      internal,
      buildslave_type=constants.GCE_WIMPY_BUILD_SLAVE_TYPE,
      boards=[],
      master=True,
      push_overlays=constants.BOTH_OVERLAYS,
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
      if board in _lakitu_boards:
        base.update(lakitu)
      if board in _moblab_boards:
        base.update(moblab)
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
        # I derive the config_base build config since the config gets modified
        # below if they're in _nofactory_boards.  What happens if I don't derive
        # is that once factory_install is removed from images, all configs will
        # have factory_install removed from their images.  The derive creates a
        # new copy of the config_base and thus isolates changes to the instance
        # created.
        config = site_config.Add(config_name,
                                 config_base.derive(config_lib.BuildConfig()),
                                 _base_configs[board],
                                 **kwargs)
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
      'amd64-generic',
  ])

  _gn_boards = frozenset([
      'amd64-generic',
  ])

  def _AddFullConfigs():
    """Add x86 and arm full configs."""
    defaults = config_lib.DefaultSettings()
    external_overrides = config_lib.BuildConfig(
        manifest=defaults['manifest'],
        useflags=append_useflags(['-%s' % constants.USE_CHROME_INTERNAL]),
    )
    _CreateConfigsForBoards(full_prebuilts, _all_full_boards,
                            config_lib.CONFIG_TYPE_FULL,
                            internal=defaults['internal'],
                            manifest_repo_url=site_params['MANIFEST_URL'],
                            overlays=defaults['overlays'],
                            **external_overrides)
    _CreateConfigsForBoards(chromium_info, _all_full_boards,
                            'tot-chromium-pfq-informational',
                            important=False,
                            internal=defaults['internal'],
                            manifest_repo_url=site_params['MANIFEST_URL'],
                            overlays=constants.PUBLIC_OVERLAYS,
                            **external_overrides)
    _CreateConfigsForBoards(chromium_info_gn, _gn_boards,
                            'tot-chromium-pfq-informational-gn',
                            important=False,
                            internal=defaults['internal'],
                            manifest_repo_url=site_params['MANIFEST_URL'],
                            overlays=constants.PUBLIC_OVERLAYS,
                            **external_overrides)
    # Create important configs, then non-important configs.
    _CreateConfigsForBoards(internal_chromium_pfq,
                            _chromium_pfq_important_boards,
                            'chromium-pfq', **external_overrides)
    _CreateConfigsForBoards(internal_chromium_pfq, _all_full_boards,
                            'chromium-pfq', important=False,
                            **external_overrides)

  _AddFullConfigs()


  # These remaining chromium pfq configs have eccentricities that are easier to
  # create manually.

  site_config.Add(
      'amd64-generic-chromium-pfq', internal_chromium_pfq,
      _base_configs['amd64-generic'],
      disk_layout='2gb-rootfs',
  )

  _chrome_pfq_important_boards = frozenset([
      'peppy',
      'veyron_pinky',
      'veyron_rialto',
      'nyan',
  ])

  site_config.Add(
      'x86-alex-chrome-pfq', chrome_pfq,
      _base_configs['x86-alex'],
  )

  site_config.Add(
      'lumpy-chrome-pfq', chrome_pfq,
      _base_configs['lumpy'],
      afdo_generate=True,
      hw_tests=[HWTestList.AFDORecordTest()] + HWTestList.SharedPoolPFQ(),
  )

  site_config.Add(
      'cyan-cheets-chrome-pfq', chrome_pfq,
      _base_configs['cyan-cheets'],
      hw_tests=HWTestList.SharedPoolAndroidPFQ(),
  )

  site_config.Add(
      'daisy_skate-chrome-pfq', chrome_pfq,
      _base_configs['daisy_skate'],
      hw_tests=HWTestList.SharedPoolPFQ(),
  )

  site_config.Add(
      'falco-chrome-pfq', chrome_pfq,
      _base_configs['falco'],
      hw_tests=HWTestList.SharedPoolPFQ(),
  )

  site_config.Add(
      'veyron_minnie-cheets-chrome-pfq', chrome_pfq,
      _base_configs['veyron_minnie-cheets'],
      hw_tests=HWTestList.SharedPoolAndroidPFQ(),
  )

  site_config.Add(
      'peach_pit-chrome-pfq', chrome_pfq,
      _base_configs['peach_pit'],
      hw_tests=HWTestList.SharedPoolPFQ(),
  )

  site_config.Add(
      'tricky-chrome-pfq', chrome_pfq,
      _base_configs['tricky'],
      hw_tests=HWTestList.SharedPoolPFQ(),
  )

  _android_pfq_hwtest_boards = frozenset([
      'cyan-cheets',
      'veyron_minnie-cheets',
  ])

  _android_pfq_no_hwtest_boards = frozenset([
      'glados-cheets',
      'samus-cheets',
  ])

  _telemetry_boards = frozenset([
      'amd64-generic',
      'arm-generic',
      'x86-generic',
  ])

  _CreateConfigsForBoards(telemetry, _telemetry_boards, 'telemetry')

  site_config.Add(
      'x86-generic-asan',
      asan,
      incremental,
      boards=['x86-generic'],
      chroot_replace=True,
      hw_tests=HWTestList.AsanTest(),
      hw_tests_override=HWTestList.AsanTest(),
      description='Build with Address Sanitizer (Clang)',
      trybot_list=True,
  )

  tot_asan_info = site_config.AddTemplate(
      'tot-asan-informational',
      chromium_info,
      asan,
      unittests=True,
      description='Build TOT Chrome with Address Sanitizer (Clang)',
  )

  site_config.Add(
      'x86-generic-tot-asan-informational',
      tot_asan_info,
      boards=['x86-generic'],
  )

  site_config.Add(
      'amd64-generic-asan',
      asan,
      incremental,
      boards=['amd64-generic'],
      description='Build with Address Sanitizer (Clang)',
      trybot_list=True,
  )

  site_config.Add(
      'amd64-generic-tot-asan-informational', tot_asan_info,
      boards=['amd64-generic'],
  )

  incremental_beaglebone = incremental.derive(beaglebone)
  site_config.Add(
      'beaglebone-incremental', incremental_beaglebone,
      boards=['beaglebone'],
      trybot_list=True,
      description='Incremental Beaglebone Builder',
  )

  site_config.Add(
      'x86-generic-incremental', incremental,
      _base_configs['x86-generic'],
  )

  site_config.Add(
      'daisy-incremental', incremental,
      _base_configs['daisy'].derive(
          config_lib.BuildConfig.delete_keys(internal),
          manifest=config_lib.BuildConfig.delete_key()),
      useflags=append_useflags(['-chrome_internal']),
  )

  site_config.Add(
      'amd64-generic-incremental', incremental,
      _base_configs['amd64-generic'],
      # This builder runs on a VM, so it can't run VM tests.
      vm_tests=[],
  )

  site_config.Add(
      'x32-generic-incremental', incremental,
      _base_configs['x32-generic'],
      # This builder runs on a VM, so it can't run VM tests.
      vm_tests=[],
  )

  site_config.Add(
      'x86-generic-asan-paladin', paladin,
      _base_configs['x86-generic'],
      asan,
      description='Paladin build with Address Sanitizer (Clang)',
      important=False,
  )

  site_config.Add(
      'amd64-generic-asan-paladin', paladin,
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

  # pylint: disable=bad-continuation
  _CreateConfigsForBoards(chromium_info, ['x86-generic', 'amd64-generic'],
      'telem-chromium-pfq-informational',
      **telemetry.derive(chrome_try,
                         _template=(config_lib.BuildConfig.delete_key())))

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

  site_config.Add(
      'x86-mario-nowithdebug-paladin',
      internal_nowithdebug_paladin,
      boards=['x86-mario'])

  # Used for builders which build completely from source except Chrome.
  full_compile_paladin = paladin.derive(
      board_replace=True,
      chrome_binhost_only=True,
      chrome_sdk=False,
      compilecheck=True,
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
      vm_tests=[config_lib.VMTestConfig(constants.SMOKE_SUITE_TEST_TYPE)],
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

  site_config.AddWithoutTemplate(
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

  site_config.Add(
      'lumpy-pre-flight-branch', internal_pfq_branch,
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
      build_type=constants.INCREMENTAL_TYPE,
      description='WiFi AP images used in testing',
      profile='testbed-ap',
      vm_tests=[],
  )

  site_config.AddGroup(
      'test-ap-group',
      site_config.Add('stumpy-test-ap', _test_ap, boards=['stumpy']),
      site_config.Add('panther-test-ap', _test_ap, boards=['panther']),
      site_config.Add('whirlwind-test-ap', _test_ap, boards=['whirlwind']),
  )

  ### Master paladin (CQ builder).

  site_config.Add(
      'master-paladin', internal_paladin,
      boards=[],
      buildslave_type=constants.BAREMETAL_BUILD_SLAVE_TYPE,
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
      auto_reboot=False,
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
  site_config.Add(
      'wolf-tot-paladin', internal_paladin,
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
      'cyan-cheets',
      'daisy',
      'daisy_skate',
      'daisy_spring',
      'nyan_freon',
      'falco',
      'glados',
      'guado_moblab',
      'lakitu',
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
      'samus',
      'smaug',
      'storm',
      'stout',
      'strago',
      'stumpy',
      'tricky',
      'veyron_mighty',
      'veyron_minnie-cheets',
      'veyron_pinky',
      'veyron_rialto',
      'veyron_speedy',
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
      'veyron_mighty',
      'veyron_speedy',
      'wolf',
      'x86-alex',
      'x86-zgb',
  ])


  # Jetstream devices run unique hw tests
  _paladin_jetstream_hwtest_boards = frozenset([
      'whirlwind',
  ])

  _paladin_moblab_hwtest_boards = frozenset([
      'guado_moblab',
  ])

  # *-cheets devices run a different suite
  _paladin_cheets_hwtest_boards = frozenset([
    'cyan-cheets',
    'veyron_minnie-cheets',
  ])

  _paladin_chroot_replace_boards = frozenset([
      'butterfly',
      'daisy_spring',
  ])

  _paladin_separate_symbols = frozenset([
      'amd64-generic',
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
                    num=1, timeout=120*60,
                    pool=constants.HWTEST_PALADIN_POOL)
            ])
      if board in _paladin_cheets_hwtest_boards:
        customizations.update(
            hw_tests=[
                config_lib.HWTestConfig(
                    constants.HWTEST_ARC_COMMIT_SUITE,
                    pool=constants.HWTEST_PALADIN_POOL)
            ])
      if board in _paladin_jetstream_hwtest_boards:
        customizations.update(
            hw_tests=[
                config_lib.HWTestConfig(
                    constants.HWTEST_JETSTREAM_COMMIT_SUITE,
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
          vm_tests.append(
              config_lib.VMTestConfig(constants.SIMPLE_AU_TEST_TYPE))
        if board in _paladin_cros_vmtest_boards:
          vm_tests.append(config_lib.VMTestConfig(constants.CROS_VM_TEST_TYPE))
        if board in _paladin_devmode_vmtest_boards:
          vm_tests.append(config_lib.VMTestConfig(constants.DEV_MODE_TEST_TYPE))
        if board in _paladin_smoke_vmtest_boards:
          vm_tests.append(
              config_lib.VMTestConfig(constants.SMOKE_SUITE_TEST_TYPE))

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

      if board in _lakitu_boards:
        # Note: Because |customizations| precedes |base_config|, it will be
        # overridden by |base_config|. So we have to append lakitu
        # customizations after |base_config| is applied.
        # TODO(crbug.com/553749)
        # Also, I can't do
        # `lakitu_test_customizations if xxx else None` because the Add function
        # doesn't allow optional args to be None.
        site_config.Add(
            config_name, paladin,
            customizations,
            base_config,
            lakitu_test_customizations,
        )
      else:
        site_config.Add(
            config_name, paladin,
            customizations,
            base_config,
        )

  _CreatePaladinConfigs()

  site_config.Add(
      'lumpy-incremental-paladin', internal_paladin,
      boards=['lumpy'],
      build_before_patching=True,
      prebuilts=False,
      compilecheck=True,
      unittests=False,
  )

  # Paladins (CQ builders) which do not run VM or Unit tests on the builder
  # itself.
  internal_beaglebone_paladin = internal_paladin.derive(beaglebone)

  site_config.Add(
      'beaglebone-paladin', internal_beaglebone_paladin,
      boards=['beaglebone'],
      trybot_list=True,
  )

  site_config.Add(
      'beaglebone_servo-paladin', internal_beaglebone_paladin,
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
  ShardHWTestsBetweenBuilders('veyron_mighty-paladin', 'veyron_speedy-paladin')
  ShardHWTestsBetweenBuilders('lumpy-paladin', 'stumpy-paladin')

  # Add a pre-cq config for every board.
  _CreateConfigsForBoards(pre_cq, _all_boards, 'pre-cq')
  site_config.Add(
      'lakitu-pre-cq', pre_cq,
      _base_configs['lakitu'],
      lakitu_test_customizations,
  )
  site_config.Add(
      'lakitu_next-pre-cq', pre_cq,
      _base_configs['lakitu_next'],
      lakitu_test_customizations,
  )

  _CreateConfigsForBoards(no_vmtest_pre_cq, _all_boards, 'no-vmtest-pre-cq')
  _CreateConfigsForBoards(
      compile_only_pre_cq, _all_boards, 'compile-only-pre-cq')

  site_config.Add(
      constants.BINHOST_PRE_CQ, pre_cq,
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
  )

  site_config.AddWithoutTemplate(
      'pre-cq-launcher',
      internal_paladin,
      no_vmtest_builder,
      no_hwtest_builder,
      boards=[],
      build_type=constants.PRE_CQ_LAUNCHER_TYPE,
      buildslave_type=constants.GCE_WIMPY_BUILD_SLAVE_TYPE,
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


  site_config.Add(
      'mario-incremental', internal_incremental,
      boards=['x86-mario'],
  )

  site_config.Add(
      'lakitu-incremental', internal_incremental,
      _base_configs['lakitu'],
      lakitu_test_customizations,
  )

  site_config.Add(
      'lakitu_next-incremental', internal_incremental,
      _base_configs['lakitu_next'],
      lakitu_test_customizations,
  )

  _release = site_config.AddTemplate(
      'release',
      full,
      official,
      internal,
      default_hw_tests_override,
      build_type=constants.CANARY_TYPE,
      build_timeout=12 * 60 * 60 if IS_RELEASE_BRANCH else (7 * 60 + 50) * 60,
      useflags=append_useflags(['-cros-debug']),
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
      vm_tests=[
          config_lib.VMTestConfig(constants.SMOKE_SUITE_TEST_TYPE),
          config_lib.VMTestConfig(constants.DEV_MODE_TEST_TYPE),
          config_lib.VMTestConfig(constants.CROS_VM_TEST_TYPE)],
      # Some release builders disable VMTests to be able to build on GCE, but
      # still want VMTests enabled on trybot builders.
      vm_tests_override=[
          config_lib.VMTestConfig(constants.SMOKE_SUITE_TEST_TYPE),
          config_lib.VMTestConfig(constants.DEV_MODE_TEST_TYPE),
          config_lib.VMTestConfig(constants.CROS_VM_TEST_TYPE)],
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

  _toolchain = site_config.AddTemplate(
      'toolchain',
      full,
      official,
      internal,
      default_hw_tests_override,
      build_type=constants.TOOLCHAIN_TYPE,
      build_timeout=(9 * 60 + 50) * 60,
      useflags=append_useflags(['-cros-debug']),
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
      vm_tests=[config_lib.VMTestConfig(constants.SMOKE_SUITE_TEST_TYPE),
                config_lib.VMTestConfig(constants.DEV_MODE_TEST_TYPE),
                config_lib.VMTestConfig(constants.CROS_VM_TEST_TYPE)],
      hw_tests=HWTestList.SharedPoolCanary(),
      paygen=True,
      signer_tests=True,
      trybot_list=True,
      hwqual=True,
      description="Toolchain Builds (internal)",
      chrome_sdk=True,
      image_test=True,
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

  ### Master toolchain config.

  site_config.Add(
      'master-toolchain',
      _toolchain,
      boards=[],
      description='Toolchain master (all others are slaves).',
      master=True,
      sync_chrome=True,
      chrome_sdk=True,
      health_alert_recipients=['c-compiler-chrome@google.com'],
      health_threshold=1,
      afdo_use=False,
      usepkg_build_packages=False,
      branch_util_test=True,
      buildslave_type=constants.GCE_WIMPY_BUILD_SLAVE_TYPE,
  )

  llvm = site_config.AddTemplate(
      'llvm',
      # Use release builder as a base. Make sure that we are doing a full
      # build and that we are using AFDO.
      _toolchain,
      no_vmtest_builder,
      profile='llvm',
      hw_tests=HWTestList.ToolchainTest(),
      hw_tests_override=HWTestList.ToolchainTest(),
      images=['base', 'test'],
      description='Full release build with LLVM toolchain',
      paygen=False,
      signer_tests=False,
      # This config is only for toolchain use. No need to list with general
      # configs.
      trybot_list=False,
  )

  _grouped_toolchain_llvm = config_lib.BuildConfig(
      build_packages_in_background=False,
      chrome_sdk=False,
      chrome_sdk_build_chrome=False,
      chroot_replace=False,
  )

  _llvm_grouped = llvm.derive(_grouped_toolchain_llvm)

  site_config.AddGroup(
      'llvm-toolchain-group',
      site_config.Add(
          'peppy-toolchain-llvm', llvm,
          boards=['peppy'],
      ),
      site_config.Add(
          'daisy-toolchain-llvm', _llvm_grouped,
          boards=['daisy'],
      ),
      site_config.Add(
          'x86-alex-toolchain-llvm', _llvm_grouped,
          boards=['x86-alex'],
      ),
      site_config.Add(
          'oak-toolchain-llvm', _llvm_grouped,
          boards=['oak'],
      ),
  )

  site_config.AddGroup(
      'llvm-next-toolchain-group',
      site_config.Add(
          'peppy-next-toolchain-llvm', llvm,
          boards=['peppy'],
          useflags=append_useflags(['llvm-next clang']),
      ),
      site_config.Add(
          'daisy-next-toolchain-llvm', _llvm_grouped,
          boards=['daisy'],
          useflags=append_useflags(['llvm-next clang']),
      ),
      site_config.Add(
          'x86-alex-next-toolchain-llvm', _llvm_grouped,
          boards=['x86-alex'],
          useflags=append_useflags(['llvm-next clang']),
      ),
      site_config.Add(
          'oak-next-toolchain-llvm', _llvm_grouped,
          boards=['oak'],
          useflags=append_useflags(['llvm-next clang']),
      ),
  )

  gcc = site_config.AddTemplate(
      'gcc',
      # Use release builder as a base. Make sure that we are doing a full
      # build and that we are using AFDO.
      _toolchain,
      no_vmtest_builder,
      hw_tests=HWTestList.ToolchainTest(),
      hw_tests_override=HWTestList.ToolchainTest(),
      images=['base', 'test'],
      description='Full release build with next minor GCC toolchain revision.',
      paygen=False,
      signer_tests=False,
      # This config is only for toolchain use. No need to list with general
      # configs.
      trybot_list=False,
      latest_toolchain=True,
      prebuilts=False,
      gcc_githash='svn-mirror/google/gcc-4_9',
  )

  _grouped_toolchain_gcc = config_lib.BuildConfig(
      build_packages_in_background=False,
      chrome_sdk=False,
      chrome_sdk_build_chrome=False,
      chroot_replace=False,
  )

  _gcc_grouped = gcc.derive(_grouped_toolchain_gcc)

  site_config.AddGroup(
      'gcc-toolchain-group',
      site_config.Add(
          'peppy-toolchain-gcc', gcc,
          boards=['peppy'],
      ),
      site_config.Add(
          'daisy-toolchain-gcc', _gcc_grouped,
          boards=['daisy'],
      ),
      site_config.Add(
          'x86-alex-toolchain-gcc', _gcc_grouped,
          boards=['x86-alex'],
      ),
      site_config.Add(
          'oak-toolchain-gcc', _gcc_grouped,
          boards=['oak'],
      ),
  )

  ### Master release config.

  site_config.Add(
      'master-release', _release,
      boards=[],
      buildslave_type=constants.GCE_WIMPY_BUILD_SLAVE_TYPE,
      master=True,
      sync_chrome=False,
      chrome_sdk=False,
      health_alert_recipients=['chromeos-infra-eng@grotations.appspotmail.com',
                               'tree'],
      afdo_use=False,
      branch_util_test=True,
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
          site_config.Add(
              generate_config_name, release_afdo_generate, base
          ),
          site_config.Add(
              use_config_name, release_afdo_use, base
          ),
      )

  _AddAFDOConfigs()

  ### Release configs.

  _critical_for_chrome_boards = frozenset([
      'daisy',
      'lumpy',
      'parrot',
  ])

  site_config.Add(
      'samus-release', _release,
      _base_configs['samus'],
      important=True,
  )

  ### Arm release configs.

  site_config.Add(
      'smaug-release', _release,
      _base_configs['smaug'],
      images=['base', 'recovery', 'test'],
      sign_types=['nv_lp0_firmware'],
  )

  ### Informational hwtest

  _chrome_informational_hwtest_boards = frozenset([
      'peach_pit',
      'tricky',
  ])


  # We have to mark all autogenerated PFQs as not important so the master
  # does not wait for them.  http://crbug.com/386214
  # If you want an important PFQ, you'll have to declare it yourself.

  def _AddInformationalConfigs():
    _CreateConfigsForBoards(
        chrome_info, _chrome_informational_hwtest_boards,
        'tot-chrome-pfq-informational', important=False,
        hw_tests=HWTestList.DefaultListPFQ(
            pool=constants.HWTEST_CONTINUOUS_POOL))
    informational_boards = list(set(_all_release_boards) - set(_cheets_boards))
    _CreateConfigsForBoards(
        chrome_info, informational_boards, 'tot-chrome-pfq-informational',
        important=False)
    _CreateConfigsForBoards(
        chrome_info_cheets, _cheets_boards,
        'tot-chrome-pfq-cheets-informational', important=False)

  def _AddReleaseConfigs():
    _CreateConfigsForBoards(
        chrome_pfq, _chrome_pfq_important_boards, 'chrome-pfq')
    _CreateConfigsForBoards(
        chrome_pfq, _all_release_boards, 'chrome-pfq', important=False)
    _CreateConfigsForBoards(
        android_pfq, _android_pfq_hwtest_boards, 'android-pfq',
        hw_tests=HWTestList.SharedPoolAndroidPFQ())
    _CreateConfigsForBoards(
        android_pfq, _android_pfq_no_hwtest_boards, 'android-pfq')
    _CreateConfigsForBoards(
        _release, _critical_for_chrome_boards, config_lib.CONFIG_TYPE_RELEASE,
        critical_for_chrome=True)
    _CreateConfigsForBoards(
        _release, _all_release_boards, config_lib.CONFIG_TYPE_RELEASE)

  _AddInformationalConfigs()
  _AddReleaseConfigs()

  # beaglebone build doesn't generate signed images, so don't try to release
  # them.
  _beaglebone_release = _release.derive(beaglebone,
                                        paygen=False,
                                        signer_tests=False,
                                        images=['base', 'test'],
                                        important=True)

  site_config.Add(
      'beaglebone-release', _beaglebone_release,
      boards=['beaglebone'],
      buildslave_type=constants.GCE_BEEFY_BUILD_SLAVE_TYPE,
  )

  site_config.Add(
      'beaglebone_servo-release', _beaglebone_release,
      boards=['beaglebone_servo'],
      payload_image='base',
      buildslave_type=constants.GCE_BEEFY_BUILD_SLAVE_TYPE,
  )

  site_config.Add(
      'storm-release', _release,
      _base_configs['storm'],

      # Hw Lab can't test storm, yet.
      paygen_skip_testing=True,
      signer_tests=False,
  )

  site_config.Add(
      'amd64-generic-goofy-release', _release,
      _base_configs['amd64-generic-goofy'],
      important=False,
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
          config_lib.HWTestConfig(constants.HWTEST_MOBLAB_SUITE,
                                  num=1, timeout=120*60),
          config_lib.HWTestConfig(constants.HWTEST_BVT_SUITE,
                                  warn_only=True, num=1),
          config_lib.HWTestConfig(constants.HWTEST_AU_SUITE,
                                  warn_only=True, num=1),
          config_lib.HWTestConfig(constants.HWTEST_COMMIT_SUITE, async=True,
                                  warn_only=True, num=1)],
  )

  site_config.Add(
      'guado_moblab-release', moblab_release,
      _base_configs['guado_moblab'],
  )

  cheets_release = site_config.AddTemplate(
      'cheets-release',
      _release,
      description='Cheets release builders',
      hw_tests=[
          config_lib.HWTestConfig(constants.HWTEST_ARC_COMMIT_SUITE, num=1),
          config_lib.HWTestConfig(constants.HWTEST_AU_SUITE,
                                  warn_only=True, num=1)],
  )

  site_config.Add(
      'cyan-cheets-release', cheets_release,
      _base_configs['cyan-cheets'],
  )

  site_config.Add(
      'elm-cheets-release', cheets_release,
      _base_configs['elm-cheets'],
  )

  site_config.Add(
      'glados-cheets-release', cheets_release,
      _base_configs['glados-cheets'],
  )

  site_config.Add(
      'glimmer-cheets-release', cheets_release,
      _base_configs['glimmer-cheets'],
  )

  site_config.Add(
      'oak-cheets-release', cheets_release,
      _base_configs['oak-cheets'],
  )

  site_config.Add(
      'samus-cheets-release', cheets_release,
      _base_configs['samus-cheets'],
  )

  site_config.Add(
      'smaug-cheets-release', cheets_release,
      _base_configs['smaug-cheets'],
  )

  site_config.Add(
      'celes-cheets-release', cheets_release,
      _base_configs['celes-cheets'],
  )

  site_config.Add(
      'chell-cheets-release', cheets_release,
      _base_configs['chell-cheets'],
  )

  site_config.Add(
      'lulu-cheets-release', cheets_release,
      _base_configs['lulu-cheets'],
  )

  site_config.Add(
      'veyron_minnie-cheets-release', cheets_release,
      _base_configs['veyron_minnie-cheets'],
  )

  site_config.Add(
      'veyron_mickey-release', _release,
      _base_configs['veyron_mickey'],
      vm_tests=[],
  )

  site_config.Add(
      'whirlwind-release', _release,
      _base_configs['whirlwind'],
      dev_installer_prebuilts=True,
  )

  site_config.Add(
      'lakitu-release', _release,
      _base_configs['lakitu'],
      lakitu_test_customizations,
      sign_types=['base'],
      important=True,
      images=['base', 'recovery', 'test'],
  )

  site_config.Add(
      'lakitu_next-release', _release,
      _base_configs['lakitu_next'],
      lakitu_test_customizations,
      signer_tests=False,
      images=['base', 'recovery', 'test'],
  )

  site_config.Add(
      'guado_labstation-release', _release,
      _base_configs['guado_labstation'],
      hw_tests=[
          config_lib.HWTestConfig(constants.HWTEST_CANARY_SUITE,
                                  num=1, timeout=120*60, warn_only=True,
                                  async=True, retry=False, max_retries=None,
                                  file_bugs=False),
      ],
      image_test=False,
      images=['test'],
      signer_tests=False,
      paygen=False,
      vm_tests=[],
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
      site_config.Add(
          'winky-wificell-pre-cq',
          _wificell_pre_cq,
          _base_configs['winky']),
      site_config.Add(
          'veyron_speedy-wificell-pre-cq',
          _wificell_pre_cq,
          _base_configs['veyron_speedy']),
      site_config.Add(
          'veyron_jerry-wificell-pre-cq',
          _wificell_pre_cq,
          _base_configs['veyron_jerry']),
      site_config.Add(
          'daisy-wificell-pre-cq',
          _wificell_pre_cq,
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

  def _AdjustLeaderFollowerReleaseConfigs(
      leader_boards,
      follower_boards=None,
      unimportant_boards=None,
      **kwargs):
    """Adjust existing release configs into a leader/follower group.

    _AddReleaseConfigs created a <board>-release config for every board. This
    method adjusts those existing configs to be important, and to designate one
    of them to be a 'leader'.

    We do full testing for the leader board, and cheaper testing on the
    followers since we expect them to be very similar.

    Args:
      leader_boards: List/tuple of leader boards for the group, or name of a
                     single leader board.
      follower_boards: List of board names for normal followers (reduced GCE
                       friendly testing).
      unimportant_boards: List of boards that are not to be marked important.
                          These boards are assumed to be followers, unless they
                          are also in the leaders list.
      kwargs: Any special build config settings needed for all builds in this
              group can be passed in as additional named arguments.
    """
    def release_name(board):
      """Convert a board name into the name of it's release config."""
      return '%s-release' % board

    # If leader_boards is a simple string, turn it into a single element tuple.
    if isinstance(leader_boards, basestring):
      leader_boards = (leader_boards,)

    assert leader_boards

    # Compute all release configuration names.
    leaders = [release_name(b) for b in leader_boards]
    followers = [release_name(b) for b in follower_boards or []]
    unimportant = set(release_name(b) for b in unimportant_boards or [])

    # Add unimportant to followers, if they are not covered already.
    followers.extend(unimportant - set(leaders) - set(followers))

    leader_config = config_lib.BuildConfig(
        buildslave_type=constants.BAREMETAL_BUILD_SLAVE_TYPE,
    )

    # Followers are built on GCE instances, and turn off testing that breaks
    # on GCE. The missing tests run on the leader board.
    follower_config = config_lib.BuildConfig(
        buildslave_type=constants.GCE_BEEFY_BUILD_SLAVE_TYPE,
        chrome_sdk_build_chrome=False,
        vm_tests=[],
    )

    # Adjust the leader board.
    for config_name in leaders:
      site_config[config_name] = site_config[config_name].derive(
          leader_config,
          important=config_name not in unimportant,
          **kwargs
      )

    # Adjust all follower boards based on above options.
    for config_name in followers:
      site_config[config_name] = site_config[config_name].derive(
          follower_config,
          important=config_name not in unimportant,
          **kwargs
      )

  # pineview chipset boards
  _AdjustLeaderFollowerReleaseConfigs(
      'x86-mario', (
          'x86-alex',
          'x86-zgb',
          'x86-alex_he',
          'x86-zgb_he',
      )
  )

  # sandybridge chipset boards
  _AdjustLeaderFollowerReleaseConfigs(
      'parrot', (
          'lumpy',
          'butterfly',
          'stumpy',
      )
  )

  # ivybridge chipset boards
  _AdjustLeaderFollowerReleaseConfigs(
      'stout', (
          'link',
          'parrot_ivb',
      )
  )

  # slippy, and beltino haswell boards
  _AdjustLeaderFollowerReleaseConfigs(
      'peppy', (
          'falco',
          'leon',
          'wolf',
          'panther',
          'mccloud',
          'monroe',
          'tricky',
          'zako',
          'falco_li',
      )
  )

  # rambi-based boards
  _AdjustLeaderFollowerReleaseConfigs(
      'clapper', (
          'rambi',
          'expresso',
          'enguarde',
          'glimmer',
          'gnawty',
          'kip',
          'quawks',
          'swanky',
          'winky',
          'candy',
          'squawks',
          'banjo',
          'ninja',
          'sumo',
          'orco',
          'heli',
      ),
      unimportant_boards=(
          'glimmer-cheets',
      ),
  )

  # daisy-based boards
  _AdjustLeaderFollowerReleaseConfigs(
      'daisy', (
          'daisy_spring',
          'daisy_skate',
      ),
  )

  # peach-based boards
  _AdjustLeaderFollowerReleaseConfigs(
      'peach_pit', (
          'peach_pi',
      )
  )

  # Nyan-based boards
  _AdjustLeaderFollowerReleaseConfigs(
      'nyan_big', (
          'nyan',
          'nyan_blaze',
          'nyan_kitty',
      ),
  )

  # auron-based boards
  _AdjustLeaderFollowerReleaseConfigs(
      'auron_paine', (
          'auron',
          'auron_yuna',
          'buddy',
          'gandof',
          'lulu',
          'samus-cheets',
      ),
      unimportant_boards=(
          'lulu-cheets',
      ),
  )

  # veyron-based boards
  _AdjustLeaderFollowerReleaseConfigs(
      'veyron_pinky', (
          'veyron_jerry',
          'veyron_mighty',
          'veyron_speedy',
          'veyron_jaq',
          'veyron_minnie',
          'veyron_rialto',
      ),
      unimportant_boards=(
          'veyron_mickey',
          'veyron_tiger',
          'veyron_shark',
          'veyron_minnie-cheets',
          'veyron_fievel',
      ),
  )

  # jecht-based boards
  _AdjustLeaderFollowerReleaseConfigs(
      'jecht', (
          'guado',
          'tidus',
          'rikku',
      )
  )

  # strago-based boards
  _AdjustLeaderFollowerReleaseConfigs(
      'strago', (
          'cyan',
          'celes',
          'ultima',
          'reks',
          'cyan-cheets',
          'wizpig',
          'terra',
          'edgar',
          'setzer',
          'umaro',
          'banon',
      ),
      unimportant_boards=(
          'celes-cheets',
          'kefka',
          'relm',
      ),
  )

  # oak-based boards
  _AdjustLeaderFollowerReleaseConfigs(
      'oak', (
          'elm',
          'oak-cheets',
          'elm-cheets',
      ),
  )

  # glados-based boards
  _AdjustLeaderFollowerReleaseConfigs(
      'glados', None,
      unimportant_boards=(
          'glados',
          'chell',
          'glados-cheets',
          'cave',
          'chell-cheets',
          'asuka',
      ),
  )

  # storm-based boards
  _AdjustLeaderFollowerReleaseConfigs(
      'storm', None,
      unimportant_boards=(
          'storm',
          'arkham',
          'whirlwind',
      ),
  )

  # kunimitsu-based boards
  _AdjustLeaderFollowerReleaseConfigs(
      'kunimitsu', (
          'lars',
          'sentry',
      ),
  )

  # gru-based boards
  _AdjustLeaderFollowerReleaseConfigs(
      'gru', (
          'kevin',
      ),
  )

  # gale-based boards
  _AdjustLeaderFollowerReleaseConfigs(
      'gale', None,
      unimportant_boards=(
        'gale',
      ),
  )

  # reef-based boards
  _AdjustLeaderFollowerReleaseConfigs(
      'reef', None,
      unimportant_boards=(
          'reef',
          'amenia',
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
      'asuka',
      'auron',
      'banjo',
      'banon',
      'butterfly',
      'candy',
      'cave',
      'chell',
      'clapper',
      'cyan',
      'daisy',
      'daisy_skate',
      'daisy_spring',
      'edgar',
      'enguarde',
      'expresso',
      'falco',
      'gale',
      'glimmer',
      'gnawty',
      'jecht',
      'kefka',
      'kip',
      'lars',
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
      'reks',
      'relm',
      'rikku',
      'samus',
      'sentry',
      'setzer',
      'slippy',
      'smaug',
      'squawks',
      'storm',
      'stout',
      'strago',
      'stumpy',
      'sumo',
      'swanky',
      'terra',
      'winky',
      'wolf',
      'x86-mario',
      'zako',
  ])

  _x86_depthcharge_firmware_boards = frozenset([
      'auron',
      'banjo',
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
      site_config.Add(
          '%s-%s' % (board, config_lib.CONFIG_TYPE_FIRMWARE),
          _firmware_release,
          _base_configs[board],
          no_vmtest_builder,
      )

    for board in _x86_depthcharge_firmware_boards:
      site_config.Add(
          '%s-%s-%s' % (board, 'depthcharge', config_lib.CONFIG_TYPE_FIRMWARE),
          _depthcharge_release,
          _base_configs[board],
          no_vmtest_builder,
      )
      site_config.Add(
          '%s-%s-%s-%s' % (board, 'depthcharge', config_lib.CONFIG_TYPE_FULL,
                           config_lib.CONFIG_TYPE_FIRMWARE),
          _depthcharge_full_internal,
          _base_configs[board],
          no_vmtest_builder,
      )

  _AddFirmwareConfigs()


  def _AddKernelTemplate(version):
    build_config = config_lib.BuildConfig(
      no_vmtest_builder,
      build_type=constants.ANDROID_PFQ_TYPE,
      images=[],
      factory_toolkit=False,
      packages=['sys-kernel/chromeos-kernel-%s' % version],
      usepkg_build_packages=True,
      sync_chrome=False,
      chrome_sdk=False,
      unittests=False,
      hw_tests=[],
      dev_installer_prebuilts=False,
      upload_hw_test_artifacts=True,
      upload_symbols=True,
      signer_tests=False,
      trybot_list=False,
      paygen=False,
      image_test=False)
    return site_config.AddTemplate(
      'kernel-%s' % version, build_config)

  _kernel_boards_versions = {
      'smaug-kasan': ['3_18'],
  }

  def _AddKernelConfigs():
    """Add kernel configs."""
    for board in _kernel_boards_versions:
      for version in _kernel_boards_versions[board]:
        site_config.Add(
            '%s-kernel-%s' % (board, version),
            _AddKernelTemplate(version),
            _base_configs[board],
            no_vmtest_builder,
        )

  _AddKernelConfigs()


  # This is an example factory branch configuration.
  # Modify it to match your factory branch.
  site_config.Add(
      'x86-mario-factory', _factory_release,
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
      site_config.Add(name, _payloads, boards=[board])

  _AddPayloadConfigs()

  # Add special builders to help with cbuidlbot development/testing.
  site_config.Add(
      'sync-test-cbuildbot',
      no_hwtest_builder,
      boards=[],
      build_type=constants.INCREMENTAL_TYPE,
      builder_class_name='test_builders.ManifestVersionedSyncBuilder',
      chroot_replace=True,
  )

  def _SetupWaterfalls():
    for name, c in site_config.iteritems():
      if not c.get('active_waterfall'):
        c['active_waterfall'] = GetDefaultWaterfall(c)

    # Apply manual configs, not used for release branches.
    if not IS_RELEASE_BRANCH:
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
