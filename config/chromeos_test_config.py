# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configuration options for various cbuildbot tests."""

from __future__ import print_function

import copy

from chromite.lib import config_lib
from chromite.lib import constants

from chromite.config import chromeos_config_boards as config_boards

vmtest_boards = frozenset([
    # Full VMTest support on ChromeOS is currently limited to devices derived
    # from betty & co.
    'amd64-generic', # Has kernel 4.4, used with public Chromium.
    'betty',         # amd64 Chrome OS VM board with 32 bit arm/x86 ARC++ ABI.
    'betty-pi-arc',  # Like betty but P version of ARC++.
    'novato',        # Like betty but with GMSCore but not the Play Store
    'novato-arc64',  # 64 bit x86_64 ARC++ ABI
]) | config_boards.lakitu_boards  # All lakitu boards have VM support.


def getInfoVMTest():
  suites = [
      'vmtest-informational4'
  ]
  ret = []
  for suite in suites:
    ret.append(config_lib.VMTestConfig(
        constants.VM_SUITE_TEST_TYPE,
        test_suite=suite,
        warn_only=True,
        timeout=12 * 60 * 60))
  return ret


class HWTestList(object):
  """Container for methods to generate HWTest lists."""

  def __init__(self, ge_build_config):
    """Helper class for creating hwtests.

    Args:
      ge_build_config: Dictionary containing the decoded GE configuration file.
    """
    self.is_release_branch = ge_build_config[
        config_lib.CONFIG_TEMPLATE_RELEASE_BRANCH]

  def DefaultList(self, **kwargs):
    """Returns a default list of HWTestConfigs for a build.

    Args:
      *kwargs: overrides for the configs
    """
    return [
        config_lib.HWTestConfig(constants.HWTEST_BVT_SUITE,
                                **self._bvtInlineHWTestArgs(kwargs)),
        config_lib.HWTestConfig(constants.HWTEST_ARC_COMMIT_SUITE,
                                **self._bvtInlineHWTestArgs(kwargs)),
        self.TastConfig(constants.HWTEST_TAST_CQ_SUITE,
                        **self._bvtInlineHWTestArgs(kwargs)),
        # Start informational Tast tests before the installer suite to let the
        # former run even if the latter fails: https://crbug.com/911921
        self.TastConfig(constants.HWTEST_TAST_INFORMATIONAL_SUITE,
                        **self._asyncHWTestArgs(kwargs)),
        # Context: crbug.com/976834
        # Because this blocking suite fails a fair bit, it effectively acts as a
        # rate limiter to the Autotest scheduling system since all of the
        # subsequent test suites aren't run if this fails.
        # Making this non-blocking and async will cause Autotest scheduling to
        # fail.
        config_lib.HWTestConfig(constants.HWTEST_INSTALLER_SUITE,
                                **self._blockingHWTestArgs(kwargs)),
        config_lib.HWTestConfig(constants.HWTEST_COMMIT_SUITE,
                                **self._asyncHWTestArgs(kwargs)),
        config_lib.HWTestConfig(constants.HWTEST_CANARY_SUITE,
                                **self._asyncHWTestArgs(kwargs)),
    ]

  def _asyncHWTestArgs(self, kwargs):
    """Get updated kwargs for asynchronous hardware tests."""
    kwargs = kwargs.copy()
    kwargs['priority'] = constants.HWTEST_POST_BUILD_PRIORITY
    kwargs['async'] = True
    kwargs['suite_min_duts'] = 1
    kwargs['timeout'] = config_lib.HWTestConfig.ASYNC_HW_TEST_TIMEOUT
    return kwargs

  def _blockingHWTestArgs(self, kwargs):
    """Get updated kwargs for blockiong hardware tests."""
    kwargs = kwargs.copy()
    kwargs['blocking'] = True
    kwargs['async'] = False
    kwargs['priority'] = constants.HWTEST_CQ_PRIORITY
    kwargs['quota_account'] = 'bvt-sync'
    return kwargs

  def _bvtInlineHWTestArgs(self, kwargs):
    """Get updated kwargs for bvt-inline hardware tests."""
    if self.is_release_branch:
      return self._asyncHWTestArgs(kwargs)
    kwargs = kwargs.copy()
    kwargs['timeout'] = config_lib.HWTestConfig.SHARED_HW_TEST_TIMEOUT
    return kwargs

  def DefaultListCanary(self, **kwargs):
    """Returns a default list of config_lib.HWTestConfig's for a canary build.

    Args:
      *kwargs: overrides for the configs
    """
    # Set minimum_duts default to 4, which means that lab will check the
    # number of available duts to meet the minimum requirement before creating
    # the suite job for canary builds.
    kwargs.setdefault('minimum_duts', 4)
    kwargs.setdefault('file_bugs', True)
    kwargs['blocking'] = False
    kwargs['async'] = True
    return self.DefaultList(**kwargs)

  def AFDOList(self, **kwargs):
    """Returns a default list of HWTestConfigs for a AFDO build.

    For more details on AFDO:
    - https://gcc.gnu.org/wiki/AutoFDO
    - https://sites.google.com/a/google.com/chromeos-toolchain-team-home2
      -> 11. AFDO PROFILE-GUIDED OPTIMIZATIONS

    Args:
      *kwargs: overrides for the configs
    """
    afdo_dict = dict(pool=constants.HWTEST_SUITES_POOL,
                     timeout=120 * 60, retry=False,
                     max_retries=None)
    # Python 3.7+ made async a reserved keyword.
    afdo_dict['async'] = True
    afdo_dict.update(kwargs)
    return [config_lib.HWTestConfig('perf_v2', **afdo_dict)]

  def DefaultListNonCanary(self, **kwargs):
    """Return a default list of HWTestConfigs for a non-canary build.

    Optional arguments may be overridden in `kwargs`, except that
    the `blocking` setting cannot be provided.
    """
    # N.B.  The ordering here is coupled with the column order of
    # entries in _paladin_hwtest_assignments, below.  If you change the
    # ordering here you must also change the ordering there.
    return [
        config_lib.HWTestConfig(constants.HWTEST_BVT_SUITE,
                                **kwargs),
        config_lib.HWTestConfig(constants.HWTEST_COMMIT_SUITE,
                                **kwargs),
        config_lib.HWTestConfig(constants.HWTEST_ARC_COMMIT_SUITE,
                                **kwargs)]

  def DefaultListChromePFQInformational(self, **kwargs):
    """Return a default list of HWTestConfigs for an inform. Chrome PFQ build.

    Optional arguments may be overridden in `kwargs`, except that
    the `blocking` setting cannot be provided.
    """
    # The informational PFQ does not honor to retry jobs. This reduces
    # hwtest times and shows flakes clearer.
    default_dict = dict(pool=constants.HWTEST_PFQ_POOL, file_bugs=True,
                        priority=constants.HWTEST_PFQ_PRIORITY,
                        retry=False, max_retries=None, minimum_duts=1)
    # Allows kwargs overrides to default_dict for pfq.
    default_dict.update(kwargs)
    suite_list = self.DefaultListNonCanary(**default_dict)
    suite_list.append(config_lib.HWTestConfig(
        constants.HWTEST_CHROME_INFORMATIONAL, warn_only=True, **default_dict))
    suite_list.append(self.TastConfig(constants.HWTEST_TAST_CHROME_PFQ_SUITE,
                                      **default_dict))
    return suite_list

  def DefaultListPFQ(self, **kwargs):
    """Return a default list of HWTestConfig's for a PFQ build.

    Optional arguments may be overridden in `kwargs`, except that
    the `blocking` setting cannot be provided.
    """
    default_dict = dict(file_bugs=True,
                        pool=constants.HWTEST_QUOTA_POOL,
                        quota_account='pfq',
                        timeout=config_lib.HWTestConfig.ASYNC_HW_TEST_TIMEOUT,
                        priority=constants.HWTEST_PFQ_PRIORITY, minimum_duts=3)
    # Allows kwargs overrides to default_dict for pfq.
    default_dict.update(kwargs)

    return [
        config_lib.HWTestConfig(constants.HWTEST_ARC_COMMIT_SUITE,
                                **default_dict),
        self.TastConfig(constants.HWTEST_TAST_ANDROID_PFQ_SUITE,
                        **default_dict),
    ]

  def SharedPoolPFQ(self, **kwargs):
    """Return a list of HWTestConfigs for PFQ which uses a shared pool.

    The returned suites will run in quotascheduler by default, which is
    shared with other types of builders (canaries, cq). The first suite in the
    list is a blocking sanity suite that verifies the build will not break dut.
    """
    sanity_dict = dict(pool=constants.HWTEST_QUOTA_POOL,
                       file_bugs=True, quota_account='pfq')
    sanity_dict.update(kwargs)
    sanity_dict.update(dict(minimum_duts=1, suite_min_duts=1,
                            blocking=True))
    default_dict = dict(suite_min_duts=3)
    default_dict.update(kwargs)
    suite_list = [config_lib.HWTestConfig(constants.HWTEST_SANITY_SUITE,
                                          **sanity_dict)]
    suite_list.extend(self.DefaultListPFQ(**default_dict))
    return suite_list

  def SharedPoolCanary(self, **kwargs):
    """Return a list of HWTestConfigs for Canary which uses a shared pool.

    The returned suites will run in pool:critical by default, which is
    shared with CQs.
    """
    default_dict = dict(pool=constants.HWTEST_MACH_POOL,
                        suite_min_duts=6)
    default_dict.update(kwargs)
    suite_list = self.DefaultListCanary(**default_dict)
    return suite_list

  def AFDORecordTest(self, **kwargs):
    default_dict = dict(pool=constants.HWTEST_QUOTA_POOL,
                        quota_account='toolchain',
                        file_bugs=True,
                        timeout=constants.AFDO_GENERATE_TIMEOUT,
                        priority=constants.HWTEST_PFQ_PRIORITY)
    # Allows kwargs overrides to default_dict for cq.
    default_dict.update(kwargs)
    return config_lib.HWTestConfig(constants.HWTEST_AFDO_SUITE, **default_dict)

  def ToolchainTestFull(self, machine_pool, **kwargs):
    """Return full set of HWTESTConfigs to run toolchain correctness tests."""
    default_dict = dict(pool=machine_pool,
                        file_bugs=False,
                        priority=constants.HWTEST_DEFAULT_PRIORITY)
    # Python 3.7+ made async a reserved keyword.
    default_dict['async'] = False
    default_dict.update(kwargs)
    return [config_lib.HWTestConfig(constants.HWTEST_BVT_SUITE,
                                    **default_dict),
            config_lib.HWTestConfig(constants.HWTEST_COMMIT_SUITE,
                                    **default_dict),
            self.TastConfig(constants.HWTEST_TAST_CQ_SUITE,
                            **default_dict),
            config_lib.HWTestConfig('security',
                                    **default_dict),
            config_lib.HWTestConfig('kernel_daily_regression',
                                    **default_dict),
            config_lib.HWTestConfig('kernel_daily_benchmarks',
                                    **default_dict)]

  def ToolchainTestMedium(self, machine_pool, **kwargs):
    """Return list of HWTESTConfigs to run toolchain LLVM correctness tests.

    Since the kernel is not built with LLVM, it makes no sense for the
    toolchain to run kernel tests on LLVM builds.
    """
    default_dict = dict(pool=machine_pool,
                        file_bugs=False,
                        priority=constants.HWTEST_DEFAULT_PRIORITY)
    # Python 3.7+ made async a reserved keyword.
    default_dict['async'] = False
    default_dict.update(kwargs)
    return [config_lib.HWTestConfig(constants.HWTEST_BVT_SUITE,
                                    **default_dict),
            config_lib.HWTestConfig(constants.HWTEST_COMMIT_SUITE,
                                    **default_dict),
            self.TastConfig(constants.HWTEST_TAST_CQ_SUITE,
                            **default_dict),
            config_lib.HWTestConfig('security',
                                    **default_dict)]

  def TastConfig(self, suite_name, **kwargs):
    """Return an HWTestConfig that runs the provided Tast test suite.

    Args:
      suite_name: String suite name, e.g. constants.HWTEST_TAST_CQ_SUITE.
      kwargs: Dict containing additional keyword args to use when constructing
              the HWTestConfig.

    Returns:
      HWTestConfig object for running the suite.
    """
    kwargs = kwargs.copy()

    # Tast test suites run at most three jobs (for system, Chrome, and Android
    # tests) and have short timeouts, so request at most 1 DUT (while retaining
    # passed-in requests for 0 DUTs).
    if kwargs.get('minimum_duts', 0):
      kwargs['minimum_duts'] = 1
    if kwargs.get('suite_min_duts', 0):
      kwargs['suite_min_duts'] = 1

    return config_lib.HWTestConfig(suite_name, **kwargs)

TRADITIONAL_VM_TESTS_SUPPORTED = [
    config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                            test_suite='smoke',
                            use_ctest=False),
    config_lib.VMTestConfig(constants.SIMPLE_AU_TEST_TYPE),
    config_lib.VMTestConfig(constants.CROS_VM_TEST_TYPE)]

def InsertHwTestsOverrideDefaults(build):
  """Insert default hw_tests values for a given build.

  Also updates child builds.

  Args:
    build: BuildConfig instance to modify in place.
  """
  for child in build['child_configs']:
    InsertHwTestsOverrideDefaults(child)

  if build['hw_tests_override'] is not None:
    # Explicitly set, no need to insert defaults.
    return

  if build['hw_tests']:
    # Copy over base tests.
    build['hw_tests_override'] = [copy.copy(x) for x in build['hw_tests']]

    # Adjust for manual test environment.
    for hw_config in build['hw_tests_override']:
      hw_config.pool = constants.HWTEST_TRYBOT_POOL
      hw_config.file_bugs = False
      hw_config.priority = constants.HWTEST_DEFAULT_PRIORITY


def EnsureVmTestsOnVmTestBoards(site_config, boards_dict, _gs_build_config):
  """Make sure VMTests are only enabled on boards that support them.

  Args:
    site_config: config_lib.SiteConfig containing builds to have their
                 waterfall values updated.
    boards_dict: A dict mapping board types to board name collections.
    ge_build_config: Dictionary containing the decoded GE configuration file.
  """
  for c in site_config.values():
    if set(c['boards']).intersection(set(boards_dict['no_vmtest_boards'])):
      c.apply(site_config.templates.no_vmtest_builder)
      if c.child_configs:
        for cc in c.child_configs:
          cc.apply(site_config.templates.no_vmtest_builder)


def ApplyCustomOverrides(site_config):
  """Method with to override specific flags for specific builders.

  Generally try really hard to avoid putting anything here that isn't
  a really special case for a single specific builder. This is performed
  after every other bit of processing, so it always has the final say.

  Args:
    site_config: config_lib.SiteConfig containing builds to have their
                 waterfall values updated.
  """

  overwritten_configs = {
      'lakitu-release': config_lib.BuildConfig().apply(
          site_config.templates.lakitu_test_customizations,
      ),

      # This is the full build of open-source overlay.
      'lakitu-full': config_lib.BuildConfig().apply(
          # logging_CrashSender is expected to fail for lakitu-full.
          # See b/111567339 for more details.
          useflags=config_lib.append_useflags(['-tests_logging_CrashSender']),
      ),

      'lakitu-gpu-release': config_lib.BuildConfig().apply(
          site_config.templates.lakitu_test_customizations,
      ),

      'lakitu-nc-release': config_lib.BuildConfig().apply(
          signer_tests=False,
      ),

      'lakitu-st-release': config_lib.BuildConfig().apply(
          site_config.templates.lakitu_test_customizations,
      ),

      'lakitu_next-release': config_lib.BuildConfig().apply(
          site_config.templates.lakitu_test_customizations,
          signer_tests=False,
      ),

      'guado_labstation-release': {
          'hw_tests': [],
          # 'hwqual':False,
          'image_test':False,
          # 'images':['test'],
          'signer_tests':False,
          'vm_tests':[],
      },

      'fizz-labstation-release': {
          'hw_tests': [],
          'image_test':False,
          'signer_tests':False,
          'vm_tests':[],
      },

      'betty-pi-arc-pi-android-pfq':
          site_config.templates.tast_vm_android_pfq_tests,

      # There's no amd64-generic-release builder, so we use amd64-generic-full
      # to validate informational Tast tests on amd64-generic:
      # https://crbug.com/946858
      'amd64-generic-full': site_config.templates.tast_vm_canary_tests,
      'betty-pi-arc-release': site_config.templates.tast_vm_canary_tests,
      'betty-release': site_config.templates.tast_vm_canary_tests,
  }

  for config_name, overrides in overwritten_configs.items():
    # TODO: Turn this assert into a unittest.
    # config = site_config[config_name]
    # for k, v in overrides.items():
    #   assert config[k] != v, ('Unnecessary override: %s: %s' %
    #                           (config_name, k))
    site_config[config_name].apply(**overrides)


def IncrementalBuilders(site_config):
  """Create all incremental test configs.

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
  """

  # incremental
  site_config['amd64-generic-incremental'].apply(
      site_config.templates.no_vmtest_builder,
  )

  site_config['betty-incremental'].apply(
      vm_tests=getInfoVMTest(),
      vm_tests_override=getInfoVMTest(),
  )

  site_config['lakitu-incremental'].apply(
      site_config.templates.lakitu_test_customizations,
  )

  site_config['x32-generic-incremental'].apply(
      site_config.templates.no_vmtest_builder,
  )

  site_config['lakitu-gpu-incremental'].apply(
      site_config.templates.lakitu_test_customizations,
  )

  site_config['lakitu-st-incremental'].apply(
      site_config.templates.lakitu_test_customizations,
  )

  site_config['lakitu_next-incremental'].apply(
      site_config.templates.lakitu_test_customizations,
  )

  site_config['kumo-incremental'].apply(
      vm_tests=[config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                        test_suite='smoke')],
  )


def PostsubmitBuilders(site_config):
  """Create all postsubmit test configs.

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
  """
  for config in site_config.values():
    if config.name.endswith('postsubmit'):
      config.apply(
          site_config.templates.no_vmtest_builder,
          site_config.templates.no_hwtest_builder,
      )


def GeneralTemplates(site_config, ge_build_config):
  """Apply test config to general templates

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
    ge_build_config: Dictionary containing the decoded GE configuration file.
  """
  hw_test_list = HWTestList(ge_build_config)

  # TryjobMirrors uses hw_tests_override to ensure that tryjobs run all suites
  # rather than just the ones that are assigned to the board being used. Add
  # bvt-tast-cq here since it includes system, Chrome, and Android tests.
  site_config.AddTemplate(
      'default_hw_tests_override',
      hw_tests_override=hw_test_list.DefaultList(
          pool=constants.HWTEST_TRYBOT_POOL,
          file_bugs=False,
      ),
  )

  # Notice all builders except for vmtest_boards should not run vmtest.
  site_config.AddTemplate(
      'no_vmtest_builder',
      vm_tests=[],
      vm_tests_override=None,
      gce_tests=[],
      tast_vm_tests=[],
      moblab_vm_tests=[],
  )

  site_config.AddTemplate(
      'no_hwtest_builder',
      hw_tests=[],
      hw_tests_override=[],
  )

  site_config.AddTemplate(
      'moblab',
      site_config.templates.no_vmtest_builder,
      image_test=False,
  )

  site_config.templates.full.apply(
      site_config.templates.default_hw_tests_override,
      image_test=True,
  )

  site_config.templates.fuzzer.apply(
      site_config.templates.default_hw_tests_override,
      site_config.templates.no_hwtest_builder,
      image_test=True,
  )

  # BEGIN asan
  site_config.templates.asan.apply(
      site_config.templates.default_hw_tests_override,
  )
  # END asan

  # BEGIN Incremental
  site_config.templates.incremental.apply(
      site_config.templates.default_hw_tests_override,
  )

  site_config.templates.internal_incremental.apply(
      site_config.templates.default_hw_tests_override,
  )
  # END Incremental

  # BEGIN Factory
  site_config.templates.factory.apply(
      # site_config.templates.default_hw_tests_override,
      site_config.templates.no_vmtest_builder,
      site_config.templates.no_hwtest_builder,
  )
  # END Factory

  # BEGIN Firmware
  site_config.templates.firmwarebranch.apply(
      site_config.templates.no_vmtest_builder,
  )
  # END Firmware

  # BEGIN Lakitu
  site_config.templates.lakitu.apply(
      site_config.templates.no_hwtest_builder,
  )
  # END Lakitu

  # BEGIN Loonix
  site_config.templates.loonix.apply(
      site_config.templates.no_vmtest_builder,
      site_config.templates.no_hwtest_builder,
  )
  # END Loonix

  # BEGIN WSHWOS
  site_config.templates.wshwos.apply(
      site_config.templates.no_vmtest_builder,
      site_config.templates.no_hwtest_builder,
  )
  # END WSHWOS

  # BEGIN Dustbuster
  site_config.templates.dustbuster.apply(
      site_config.templates.no_vmtest_builder,
      site_config.templates.no_hwtest_builder,
  )
  # END Dustbuster

  # BEGIN Release
  release_hw_tests = hw_test_list.SharedPoolCanary()

  site_config.templates.release.apply(
      site_config.templates.default_hw_tests_override,
      hw_tests=release_hw_tests,
  )

  site_config.templates.moblab_release.apply(
      site_config.templates.default_hw_tests_override,
      hw_tests=[
          config_lib.HWTestConfig(constants.HWTEST_MOBLAB_SUITE,
                                  timeout=120*60),
          config_lib.HWTestConfig(constants.HWTEST_BVT_SUITE,
                                  warn_only=True),
          hw_test_list.TastConfig(constants.HWTEST_TAST_CQ_SUITE,
                                  warn_only=True),
          config_lib.HWTestConfig(constants.HWTEST_INSTALLER_SUITE,
                                  warn_only=True)],
  )

  site_config.templates.payloads.apply(
      site_config.templates.no_vmtest_builder,
      site_config.templates.no_hwtest_builder,
  )
  # END Release

  site_config.templates.test_ap.apply(
      site_config.templates.no_vmtest_builder,
      site_config.templates.default_hw_tests_override,
  )

  # BEGIN Termina
  site_config.templates.termina.apply(
      site_config.templates.no_vmtest_builder,
      site_config.templates.no_hwtest_builder,
  )
  # END Termina

  # BEGIN Unittest Stress
  site_config.templates.unittest_stress.apply(
      site_config.templates.no_vmtest_builder,
      site_config.templates.no_hwtest_builder,
  )
  # END Unittest Stress

  # BEGIN Ubsan
  site_config.templates.ubsan.apply(
      site_config.templates.default_hw_tests_override,
  )
  # END Ubsan

  # BEGIN x30evb
  site_config.templates.x30evb.apply(
      site_config.templates.no_hwtest_builder,
  )
  # END x30evb


def AndroidTemplates(site_config):
  """Apply test config to Android specific templates

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
  """
  site_config.templates.generic_android_pfq.apply(
      site_config.templates.default_hw_tests_override,
  )

  site_config.templates.pi_android_pfq.apply(
      site_config.templates.default_hw_tests_override,
  )


def ApplyConfig(site_config, boards_dict, ge_build_config):
  """Apply test specific config to site_config

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
    boards_dict: A dict mapping board types to board name collections.
    ge_build_config: Dictionary containing the decoded GE configuration file.
  """

  # Insert default HwTests for tryjobs.
  for build in site_config.values():
    InsertHwTestsOverrideDefaults(build)

  IncrementalBuilders(site_config)

  PostsubmitBuilders(site_config)

  EnsureVmTestsOnVmTestBoards(site_config, boards_dict, ge_build_config)

  ApplyCustomOverrides(site_config)
