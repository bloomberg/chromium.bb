# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configuration options for various cbuildbot builders."""

from __future__ import print_function

import copy
import re

from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import factory

from chromite.config import chromeos_config_boards as chromeos_boards
from chromite.config import chromeos_test_config as chromeos_test

# TODO(yshaul): Restrict the import when we're done splitting
from chromite.config.chromeos_test_config import HWTestList
from chromite.config.chromeos_test_config import TRADITIONAL_VM_TESTS_SUPPORTED
from chromite.config.chromeos_test_config import getInfoVMTest


def remove_images(unsupported_images):
  """Remove unsupported images when applying changes to a BuildConfig.

  Used similarly to append_useflags.

  Args:
    unsupported_images: A list of image names that should not be present
                        in the final build config.

  Returns:
    A callable suitable for use with BuildConfig.apply.
  """
  unsupported = set(unsupported_images)

  def handler(old_images):
    if not old_images:
      old_images = []
    return [i for i in old_images if i not in unsupported]

  return handler


def GetBoardTypeToBoardsDict(ge_build_config):
  """Get board type to board names dict.

  Args:
    ge_build_config: Dictionary containing the decoded GE configuration file.

  Returns:
    A dict mapping board types to board name collections.
    The dict contains board types including distinct_board_sets,
    all_release_boards, all_full_boards, all_boards, internal_boards,
    and no_vmtest_boards.
  """
  ge_arch_board_dict = config_lib.GetArchBoardDict(ge_build_config)

  boards_dict = {}

  arm_internal_release_boards = (
      chromeos_boards.arm_internal_release_boards |
      ge_arch_board_dict.get(config_lib.CONFIG_ARM_INTERNAL, set())
  )
  arm_external_boards = (
      chromeos_boards.arm_external_boards |
      ge_arch_board_dict.get(config_lib.CONFIG_ARM_EXTERNAL, set())
  )

  x86_internal_release_boards = (
      chromeos_boards.x86_internal_release_boards |
      ge_arch_board_dict.get(config_lib.CONFIG_X86_INTERNAL, set())
  )
  x86_external_boards = (
      chromeos_boards.x86_external_boards |
      ge_arch_board_dict.get(config_lib.CONFIG_X86_EXTERNAL, set())
  )

  # Every board should be in only 1 of the above sets.
  boards_dict['distinct_board_sets'] = [
      arm_internal_release_boards,
      arm_external_boards,
      x86_internal_release_boards,
      x86_external_boards,
  ]

  arm_full_boards = (
      arm_internal_release_boards |
      arm_external_boards)
  x86_full_boards = (
      x86_internal_release_boards |
      x86_external_boards)

  arm_boards = arm_full_boards
  x86_boards = x86_full_boards

  boards_dict['all_release_boards'] = (
      chromeos_boards.arm_internal_release_boards |
      chromeos_boards.x86_internal_release_boards
  )
  boards_dict['all_full_boards'] = (
      arm_full_boards |
      x86_full_boards
  )
  all_boards = x86_boards | arm_boards
  boards_dict['all_boards'] = (
      all_boards
  )

  boards_dict['internal_boards'] = boards_dict['all_release_boards']

  # This set controls the final vmtest override. It allows us to specify
  # vm_tests for each class of builders, but only execute on vmtest_boards.
  boards_dict['no_vmtest_boards'] = (
      all_boards - chromeos_test.vmtest_boards
  )

  boards_dict['generic_kernel_boards'] = frozenset([
      'amd64-generic',
      'arm-generic',
      'arm64-generic'
  ])

  return boards_dict


def DefaultSettings():
  """Create the default build config values for this site.

  Returns:
    dict: of default config_lib.BuildConfig values to use for this site.
  """
  # Site specific adjustments for default BuildConfig values.
  defaults = config_lib.DefaultSettings()

  # Git repository URL for our manifests.
  #  https://chromium.googlesource.com/chromiumos/manifest
  #  https://chrome-internal.googlesource.com/chromeos/manifest-internal
  defaults['manifest_repo_url'] = config_lib.GetSiteParams().MANIFEST_URL

  return defaults


def GeneralTemplates(site_config):
  """Defines templates that are shared between categories of builders.

  Args:
    site_config: A SiteConfig object to add the templates too.
    ge_build_config: Dictionary containing the decoded GE configuration file.
  """
  # Config parameters for builders that do not run tests on the builder.
  site_config.AddTemplate(
      'no_unittest_builder',
      unittests=False,
  )

  # Builder type templates.

  site_config.AddTemplate(
      'full',
      # Full builds are test builds to show that we can build from scratch,
      # so use settings to build from scratch, and archive the results.
      usepkg_build_packages=False,
      chrome_sdk=True,
      # Increase master timeout: crbug.com/927886
      build_timeout=6 * 60 * 60,
      display_label=config_lib.DISPLAY_LABEL_FULL,
      build_type=constants.FULL_TYPE,
      luci_builder=config_lib.LUCI_BUILDER_FULL,
      archive_build_debug=True,
      images=['base', 'recovery', 'test', 'factory_install'],
      git_sync=True,
      description='Full Builds',
      image_test=True,
      doc='https://dev.chromium.org/chromium-os/build/builder-overview#'
          'TOC-Continuous',
  )

  site_config.AddTemplate(
      'paladin',
      chroot_replace=False,
      display_label=config_lib.DISPLAY_LABEL_CQ,
      build_type=constants.PALADIN_TYPE,
      overlays=constants.PUBLIC_OVERLAYS,
      luci_builder=config_lib.LUCI_BUILDER_COMMITQUEUE,
      manifest_version=True,
      description='Commit Queue',
      upload_standalone_images=False,
      images=['base', 'test'],
      image_test=True,
      chrome_sdk=True,
      chrome_sdk_build_chrome=False,
      doc='https://dev.chromium.org/chromium-os/build/builder-overview#TOC-CQ',
      # This only applies to vmtest enabled boards like betty and novato.
      vm_tests=[config_lib.VMTestConfig(
          constants.VM_SUITE_TEST_TYPE,
          test_suite='smoke',
          use_ctest=False)],
      vm_tests_override=TRADITIONAL_VM_TESTS_SUPPORTED,
  )

  site_config.AddTemplate(
      'unittest_only_paladin',
      chroot_replace=False,
      board_replace=True,
      display_label=config_lib.DISPLAY_LABEL_CQ,
      build_type=constants.PALADIN_TYPE,
      overlays=constants.PUBLIC_OVERLAYS,
      luci_builder=config_lib.LUCI_BUILDER_COMMITQUEUE,
      manifest_version=True,
      unittests=True,
      compilecheck=True,
      build_packages=False,
      upload_standalone_images=False,
      sync_chrome=False,
      # The unittest only paladin needs to build everything from source
      # as a way to ensure ISA compatibility with the remote builder and
      # our binpkg selection mechanisms will provide incorrect binaries
      # in this case, so we need to disable them entirely.
      # TODO(cjmcdonald): Remove this once binpkg fallback selection
      #                   correctly respects -march flags.
      usepkg_build_packages=False,
      profile='generic_build'
  )

  # Incremental builders are intended to test the developer workflow.
  # For that reason, they don't uprev.
  site_config.AddTemplate(
      'incremental',
      display_label=config_lib.DISPLAY_LABEL_INCREMENATAL,
      build_type=constants.INCREMENTAL_TYPE,
      luci_builder=config_lib.LUCI_BUILDER_INCREMENTAL,
      chroot_replace=False,
      uprev=False,
      overlays=constants.PUBLIC_OVERLAYS,
      description='Incremental Builds',
      doc='https://dev.chromium.org/chromium-os/build/builder-overview#'
          'TOC-Continuous',
  )

  site_config.AddTemplate(
      'informational',
      display_label=config_lib.DISPLAY_LABEL_INFORMATIONAL,
      description='Informational Builds',
      luci_builder=config_lib.LUCI_BUILDER_INFORMATIONAL,
  )

  site_config.AddTemplate(
      'external',
      internal=False,
      overlays=constants.PUBLIC_OVERLAYS,
      manifest_repo_url=config_lib.GetSiteParams().MANIFEST_URL,
      manifest=constants.DEFAULT_MANIFEST,
  )

  # This builds with more source available.
  site_config.AddTemplate(
      'internal',
      internal=True,
      overlays=constants.BOTH_OVERLAYS,
      manifest_repo_url=config_lib.GetSiteParams().MANIFEST_INT_URL,
  )

  site_config.AddTemplate(
      'infra_builder',
      luci_builder=config_lib.LUCI_BUILDER_INFRA,
  )

  site_config.AddTemplate(
      'accelerator',
      sync_chrome=False,
      chrome_sdk=False,
  )

  site_config.AddTemplate(
      'brillo',
      sync_chrome=False,
      chrome_sdk=False,
      afdo_use=False,
      dev_installer_prebuilts=False,
  )

  site_config.AddTemplate(
      'lakitu',
      sync_chrome=False,
      chrome_sdk=False,
      afdo_use=False,
      dev_installer_prebuilts=False,
      paygen_skip_testing=True,
  )

  site_config.AddTemplate(
      'lassen',
      sync_chrome=False,
      chrome_sdk=False,
      image_test=False,
  )

  site_config.AddTemplate(
      'x30evb',
      sync_chrome=False,
      chrome_sdk=False,
      signer_tests=False,
      paygen=False,
      upload_hw_test_artifacts=False,
      image_test=False,
      images=['base', 'test'],
      packages=['virtual/target-os',
                'virtual/target-os-dev',
                'virtual/target-os-test'],
  )

  site_config.AddTemplate(
      'termina',
      sync_chrome=False,
      chrome_sdk=False,
      afdo_use=False,
      dev_installer_prebuilts=False,
      signer_tests=False,
      sign_types=None,
      paygen=False,
      upload_hw_test_artifacts=False,
      upload_stripped_packages=['sys-kernel/*kernel*'],
      image_test=False,
      images=['base', 'test'],
      packages=['virtual/target-os',
                'virtual/target-os-dev',
                'virtual/target-os-test'],
  )

  site_config.AddTemplate(
      'loonix',
      sync_chrome=False,
      chrome_sdk=False,
      afdo_use=False,
      dev_installer_prebuilts=False,
      # TODO(harshmodi): Re-enable this when we start using vboot
      signer_tests=False,
      paygen=False,
      upload_hw_test_artifacts=False,
      image_test=False,
      images=remove_images(['recovery', 'factory_install'])
  )

  site_config.AddTemplate(
      'wshwos',
      site_config.templates.loonix
  )

  site_config.AddTemplate(
      'dustbuster',
      # TODO(ehislen): Starting with loonix but will diverge later.
      site_config.templates.loonix
  )

  # An anchor of Laktiu' test customizations.
  # TODO: renable SIMPLE_AU_TEST_TYPE once b/67510964 is fixed.
  site_config.AddTemplate(
      'lakitu_test_customizations',
      vm_tests=[config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                        test_suite='smoke')],
      vm_tests_override=None,
      gce_tests=[config_lib.GCETestConfig(constants.GCE_SUITE_TEST_TYPE,
                                          test_suite='gce-sanity'),
                 config_lib.GCETestConfig(constants.GCE_SUITE_TEST_TYPE,
                                          test_suite='gce-smoke')],
  )

  # No GCE tests for lakitu-nc; Enable 'hsm' profile by default.
  site_config.AddTemplate(
      'lakitu_nc_customizations',
      profile='hsm',
      vm_tests=[config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                        test_suite='smoke')],
      vm_tests_override=None,
  )

  # Test customizations for lakitu boards' paladin builders.
  site_config.AddTemplate(
      'lakitu_paladin_test_customizations',
      vm_tests=[config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                        test_suite='smoke')],
      vm_tests_override=None,
      gce_tests=[config_lib.GCETestConfig(constants.GCE_SUITE_TEST_TYPE,
                                          test_suite='gce-sanity')],
  )

  # An anchor of Laktiu' notification email settings.
  site_config.AddTemplate(
      'lakitu_notification_emails',
      # Send an email on build failures.
      health_threshold=1,
      health_alert_recipients=['gci-alerts+buildbots@google.com'],
  )

  site_config.AddTemplate(
      'beaglebone',
      site_config.templates.brillo,
      image_test=False,
      rootfs_verification=False,
      paygen=False,
      signer_tests=False,
      images=remove_images(['dev', 'test', 'recovery', 'factory_install']),
  )

  # This adds Chrome branding.
  site_config.AddTemplate(
      'official_chrome',
      useflags=config_lib.append_useflags([constants.USE_CHROME_INTERNAL]),
  )

  # This sets chromeos_official.
  site_config.AddTemplate(
      'official',
      site_config.templates.official_chrome,
      chromeos_official=True,
  )

  site_config.AddTemplate(
      'asan',
      site_config.templates.full,
      profile='asan',
      luci_builder=config_lib.LUCI_BUILDER_COMMITQUEUE,
      # THESE IMAGES CAN DAMAGE THE LAB and cannot be used for hardware testing.
      disk_layout='16gb-rootfs',
      # TODO(deymo): ASan builders generate bigger files, in particular a bigger
      # Chrome binary, that update_engine can't handle in delta payloads due to
      # memory limits. Remove the following lines once crbug.com/329248 is
      # fixed.
      chrome_sdk=False,
      vm_tests=[config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                        test_suite='smoke')],
      vm_tests_override=None,
      doc='https://dev.chromium.org/chromium-os/build/builder-overview#'
          'TOC-ASAN',
  )

  site_config.AddTemplate(
      'ubsan',
      profile='ubsan',
      # Need larger rootfs for ubsan builds.
      disk_layout='16gb-rootfs',
      chrome_sdk=False,
      vm_tests=[config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                        test_suite='smoke')],
      vm_tests_override=None,
      doc='https://dev.chromium.org/chromium-os/build/builder-overview#'
          'TOC-ASAN',
  )

  site_config.AddTemplate(
      'fuzzer',
      site_config.templates.full,
      site_config.templates.informational,
      profile='fuzzer',
      chrome_sdk=False,
      sync_chrome=False,
      # Run fuzzer builder specific stages.
      builder_class_name='fuzzer_builders.FuzzerBuilder',
      # Need larger rootfs since fuzzing also enables asan.
      disk_layout='2gb-rootfs',
      gs_path='gs://chromeos-fuzzing-artifacts/libfuzzer-asan',
      images=[],
      image_test=None,
      packages=['virtual/target-fuzzers'],
  )

  site_config.AddTemplate(
      'external_chromium_pfq',
      build_type=constants.CHROME_PFQ_TYPE,
      uprev=False,
      # Increase the master timeout to 8 hours crbug.com/933284.
      build_timeout=12 * 60 * 60,
      overlays=constants.PUBLIC_OVERLAYS,
      manifest_version=True,
      chrome_rev=constants.CHROME_REV_LATEST,
      chrome_sdk=True,
      unittests=False,
      description='Preflight Chromium Uprev & Build (public)',
      # Add betty smoke VMTest crbug.com/710629.
      vm_tests=[config_lib.VMTestConfig(constants.SIMPLE_AU_TEST_TYPE)],
      vm_tests_override=None,
  )

  # TODO(davidjames): Convert this to an external config once the unified master
  # logic is ready.
  site_config.AddTemplate(
      'chromium_pfq',
      site_config.templates.internal,
      site_config.templates.external_chromium_pfq,
      display_label=config_lib.DISPLAY_LABEL_CHROME_PFQ,
      description='Preflight Chromium Uprev & Build (internal)',
      overlays=constants.BOTH_OVERLAYS,
      prebuilts=constants.PUBLIC,
      luci_builder=config_lib.LUCI_BUILDER_PFQ,
      doc='https://dev.chromium.org/chromium-os/build/builder-overview#'
          'TOC-Chrome-PFQ',
  )

  site_config.AddTemplate(
      'chrome_pfq',
      site_config.templates.chromium_pfq,
      site_config.templates.official,
      overlays=constants.BOTH_OVERLAYS,
      description='Preflight Chrome Uprev & Build (internal)',
      prebuilts=constants.PRIVATE,
  )

  site_config.AddTemplate(
      'chrome_try',
      build_type=constants.CHROME_PFQ_TYPE,
      chrome_rev=constants.CHROME_REV_TOT,
      manifest_version=False,
  )

  site_config.AddTemplate(
      'chromium_pfq_informational',
      site_config.templates.external_chromium_pfq,
      site_config.templates.chrome_try,
      site_config.templates.informational,
      display_label=config_lib.DISPLAY_LABEL_CHROME_INFORMATIONAL,
      chrome_sdk=False,
      unittests=False,
      description='Informational Chromium Uprev & Build (public)',
  )

  site_config.AddTemplate(
      'chrome_pfq_informational',
      site_config.templates.chromium_pfq_informational,
      site_config.templates.internal,
      site_config.templates.official,
      unittests=False,
      description='Informational Chrome Uprev & Build (internal)',
  )

  site_config.AddTemplate(
      'chrome_pfq_cheets_informational',
      site_config.templates.chrome_pfq_informational,
  )

  site_config.AddTemplate(
      'telemetry',
      site_config.templates.chromium_pfq_informational,
      vm_tests=[config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                        test_suite='telemetry_unit_server',
                                        # Add an extra 60 minutes.
                                        timeout=120 * 60)],
      description='Telemetry Builds',
  )

  site_config.AddTemplate(
      'tot_asan_informational',
      site_config.templates.chromium_pfq_informational,
      site_config.templates.asan,
      site_config.templates.informational,
      unittests=True,
      description='Build TOT Chrome with Address Sanitizer (Clang)',
      board_replace=True,
  )

  site_config.AddTemplate(
      'tot_ubsan_informational',
      site_config.templates.chromium_pfq_informational,
      site_config.templates.ubsan,
      site_config.templates.informational,
      unittests=True,
      description='Build TOT Chrome with Undefined Behavior Sanitizer (Clang)',
      board_replace=True,
  )
  site_config.AddTemplate(
      'chrome_perf',
      site_config.templates.chrome_pfq_informational,
      site_config.templates.no_unittest_builder,
      description='Chrome Performance test bot',
      hw_tests=[config_lib.HWTestConfig(
          'perf_v2', pool=constants.HWTEST_CHROME_PERF_POOL,
          timeout=90 * 60, critical=True)],
      use_chrome_lkgm=True,
      useflags=config_lib.append_useflags(['-cros-debug']),
  )

  site_config.AddTemplate(
      'pre_flight_branch',
      site_config.templates.internal,
      site_config.templates.official_chrome,
      build_type=constants.PFQ_TYPE,
      luci_builder=config_lib.LUCI_BUILDER_PFQ,
      build_timeout=20 * 60,
      manifest_version=True,
      branch=True,
      master=True,
      slave_configs=[],
      vm_tests=[],
      vm_tests_override=TRADITIONAL_VM_TESTS_SUPPORTED,
      hw_tests=[],
      hw_tests_override=[],
      unittests=False,
      uprev=True,
      overlays=constants.BOTH_OVERLAYS,
      push_overlays=constants.BOTH_OVERLAYS,
      doc='https://dev.chromium.org/chromium-os/build/builder-overview#'
          'TOC-Chrome-PFQ')

  site_config.AddTemplate(
      'internal_paladin',
      site_config.templates.paladin,
      site_config.templates.internal,
      site_config.templates.official_chrome,
      manifest=constants.OFFICIAL_MANIFEST,
      overlays=constants.BOTH_OVERLAYS,
      vm_tests=[],
      description=site_config.templates.paladin.description + ' (internal)',
  )

  # Used for paladin builders with nowithdebug flag (a.k.a -cros-debug)
  site_config.AddTemplate(
      'internal_nowithdebug_paladin',
      site_config.templates.internal_paladin,
      useflags=config_lib.append_useflags(['-cros-debug']),
      description=(site_config.templates.paladin.description +
                   ' (internal, nowithdebug)'),
  )

  # Internal incremental builders don't use official chrome because we want
  # to test the developer workflow.
  site_config.AddTemplate(
      'internal_incremental',
      site_config.templates.internal,
      site_config.templates.incremental,
      overlays=constants.BOTH_OVERLAYS,
      description='Incremental Builds (internal)',
  )

  # A test-ap image is just a test image with a special profile enabled.
  # Note that each board enabled for test-ap use has to have the testbed-ap
  # profile linked to from its private overlay.
  site_config.AddTemplate(
      'test_ap',
      site_config.templates.internal,
      display_label=config_lib.DISPLAY_LABEL_UTILITY,
      build_type=constants.INCREMENTAL_TYPE,
      description='WiFi AP images used in testing',
      profile='testbed-ap',
  )

  # Create tryjob build configs to help with stress testing.
  site_config.AddTemplate(
      'unittest_stress',
      display_label=config_lib.DISPLAY_LABEL_TRYJOB,
      build_type=constants.TRYJOB_TYPE,
      description='Run Unittests repeatedly to look for flake.',

      builder_class_name='test_builders.UnittestStressBuilder',

      # Make this available, so we can stress a previous build.
      manifest_version=True,
  )

  site_config.AddTemplate(
      'release_common',
      site_config.templates.full,
      site_config.templates.official,
      site_config.templates.internal,
      display_label=config_lib.DISPLAY_LABEL_RELEASE,
      build_type=constants.CANARY_TYPE,
      luci_builder=config_lib.LUCI_BUILDER_PROD,
      chroot_use_image=False,
      suite_scheduling=True,
      # Because release builders never use prebuilts, they need the
      # longer timeout.  See crbug.com/938958.
      build_timeout=12 * 60 * 60,
      useflags=config_lib.append_useflags(['-cros-debug']),
      afdo_use=True,
      manifest=constants.OFFICIAL_MANIFEST,
      manifest_version=True,
      images=['base', 'recovery', 'test', 'factory_install'],
      sign_types=['recovery'],
      push_image=True,
      upload_symbols=True,
      run_cpeexport=True,
      run_build_configs_export=True,
      binhost_bucket='gs://chromeos-dev-installer',
      binhost_key='RELEASE_BINHOST',
      binhost_base_url='https://commondatastorage.googleapis.com/'
                       'chromeos-dev-installer',
      dev_installer_prebuilts=True,
      git_sync=False,
      vm_tests=[
          config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                  test_suite='smoke'),
          config_lib.VMTestConfig(constants.DEV_MODE_TEST_TYPE),
          config_lib.VMTestConfig(constants.CROS_VM_TEST_TYPE)],
      # Some release builders disable VMTests to be able to build on GCE, but
      # still want VMTests enabled on trybot builders.
      vm_tests_override=[
          config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                  test_suite='smoke'),
          config_lib.VMTestConfig(constants.DEV_MODE_TEST_TYPE),
          config_lib.VMTestConfig(constants.CROS_VM_TEST_TYPE)],
      paygen=True,
      signer_tests=True,
      hwqual=True,
      description='Release Builds (canary) (internal)',
      chrome_sdk=True,
      image_test=True,
      doc='https://dev.chromium.org/chromium-os/build/builder-overview#'
          'TOC-Canaries',
  )

  site_config.AddTemplate(
      'release',
      site_config.templates.release_common,
      luci_builder=config_lib.LUCI_BUILDER_LEGACY_RELEASE,
  )

  site_config.AddTemplate(
      'factory_firmware',
      site_config.templates.release_common,
      luci_builder=config_lib.LUCI_BUILDER_FACTORY,
  )

  ### Release AFDO configs.
  site_config.AddTemplate(
      'release_afdo',
      site_config.templates.release,
      suite_scheduling=False,
      push_image=False,
      paygen=False,
      dev_installer_prebuilts=False,
  )

  site_config.AddTemplate(
      'release_afdo_generate',
      site_config.templates.release_afdo,
      afdo_generate_min=True,
      afdo_use=False,
      afdo_update_ebuild=True,
  )

  site_config.AddTemplate(
      'release_afdo_use',
      site_config.templates.release_afdo,
      afdo_use=True,
  )

  site_config.AddTemplate(
      'moblab_release',
      site_config.templates.release,
      description='Moblab release builders',
      images=['base', 'recovery', 'test'],
      afdo_use=False,
      signer_tests=False,
  )

  # Factory and Firmware releases much inherit from these classes.
  # Modifications for these release builders should go here.

  # Naming conventions also must be followed. Factory and firmware branches
  # must end in -factory or -firmware suffixes.

  site_config.AddTemplate(
      'factory',
      site_config.templates.factory_firmware,
      display_label=config_lib.DISPLAY_LABEL_FACTORY,
      afdo_use=False,
      chrome_sdk=False,
      chrome_sdk_build_chrome=False,
      description='Factory Builds',
      dev_installer_prebuilts=False,
      factory_toolkit=True,
      hwqual=False,
      images=['test', 'factory_install'],
      image_test=False,
      paygen=False,
      signer_tests=False,
      sign_types=['factory'],
      upload_hw_test_artifacts=False,
      upload_symbols=False,
  )

  # This should be used by any "workspace_builders."
  site_config.AddTemplate(
      'workspace',
      postsync_patch=False,
  )

  # Requires that you set boards, and workspace_branch.
  site_config.AddTemplate(
      'firmwarebranch',
      site_config.templates.factory_firmware,
      site_config.templates.workspace,
      display_label=config_lib.DISPLAY_LABEL_FIRMWARE,
      images=[],
      hwqual=False,
      factory_toolkit=False,
      packages=['virtual/chromeos-firmware'],
      usepkg_build_packages=False,
      sync_chrome=False,
      chrome_sdk=False,
      unittests=False,
      dev_installer_prebuilts=False,
      upload_hw_test_artifacts=False,
      upload_symbols=False,
      useflags=config_lib.append_useflags(['chromeless_tty']),
      signer_tests=False,
      paygen=False,
      image_test=False,
      manifest=constants.DEFAULT_MANIFEST,
      afdo_use=False,
      sign_types=['firmware', 'accessory_rwsig'],
      build_type=constants.GENERIC_TYPE,
      uprev=True,
      overlays=constants.BOTH_OVERLAYS,
      push_overlays=constants.BOTH_OVERLAYS,
      builder_class_name='workspace_builders.FirmwareBranchBuilder',
      build_timeout=6*60 * 60,
      description='TOT builder to build a firmware branch.',
      doc='https://goto.google.com/tot-for-firmware-branches',
  )

  site_config.AddTemplate(
      'payloads',
      site_config.templates.internal,
      site_config.templates.no_unittest_builder,
      display_label=config_lib.DISPLAY_LABEL_TRYJOB,
      build_type=constants.PAYLOADS_TYPE,
      luci_builder=config_lib.LUCI_BUILDER_RELEASE,
      builder_class_name='release_builders.GeneratePayloadsBuilder',
      description='Regenerate release payloads.',
      # Sync to the code used to do the build the first time.
      manifest_version=True,
      # This is the actual work we want to do.
      paygen=True,
      upload_hw_test_artifacts=False,
  )

  site_config.AddTemplate(
      'build_external_chrome',
      useflags=config_lib.append_useflags(
          ['-%s' % constants.USE_CHROME_INTERNAL]),
  )

  # Tast is an alternate system for running integration tests.

  # The expression specified here matches the union of the tast.critical-*
  # Autotest server tests, which are executed by the bvt-tast-cq suite on real
  # hardware in the lab.
  site_config.AddTemplate(
      'tast_vm_paladin_tests',
      tast_vm_tests=[
          config_lib.TastVMTestConfig(
              'tast_vm_paladin',
              ['(!disabled && !"group:*" && !informational)'])],
  )
  # The expression specified here matches the union of the
  # tast.critical-{android,chrome} Autotest server tests, which are executed by
  # the bvt-tast-chrome-pfq suite on real hardware in the lab.
  site_config.AddTemplate(
      'tast_vm_chrome_pfq_tests',
      tast_vm_tests=[
          config_lib.TastVMTestConfig(
              'tast_vm_chrome_pfq',
              ['(!disabled && !"group:*" && !informational && '
               '("dep:android*" || "dep:chrome"))'])],
  )
  # The expression specified here matches the tast.critical-android Autotest
  # server test, which is executed by the bvt-tast-android-pfq suite on real
  # hardware in the lab.
  site_config.AddTemplate(
      'tast_vm_android_pfq_tests',
      tast_vm_tests=[
          config_lib.TastVMTestConfig(
              'tast_vm_android_pfq',
              ['(!disabled && !"group:*" && !informational && '
               '"dep:android*")'])],
  )
  # The expression specified here matches the union of the tast.critical-* and
  # tast.informational-* Autotest server tests, which are executed by the
  # bvt-tast-cq and bvt-tast-informational suites on real hardware in the lab.
  site_config.AddTemplate(
      'tast_vm_canary_tests',
      tast_vm_tests=[config_lib.TastVMTestConfig(
          'tast_vm_canary', ['(!disabled && !"group:*")'])],
  )

  site_config.AddTemplate(
      'moblab_vm_tests',
      moblab_vm_tests=[
          config_lib.MoblabVMTestConfig(constants.MOBLAB_VM_SMOKE_TEST_TYPE)],
  )

  site_config.AddTemplate(
      'buildspec',
      site_config.templates.workspace,
      site_config.templates.internal,
      luci_builder=config_lib.LUCI_BUILDER_FACTORY,
      master=True,
      boards=[],
      build_type=constants.GENERIC_TYPE,
      uprev=True,
      overlays=constants.BOTH_OVERLAYS,
      push_overlays=constants.BOTH_OVERLAYS,
      builder_class_name='workspace_builders.BuildSpecBuilder',
      build_timeout=4*60 * 60,
      description='Buildspec creator.',
  )


def CreateBoardConfigs(site_config, boards_dict, ge_build_config):
  """Create mixin templates for each board."""
  # Extract the full list of board names from GE data.
  separate_board_names = set(config_lib.GeBuildConfigAllBoards(ge_build_config))
  unified_builds = config_lib.GetUnifiedBuildConfigAllBuilds(ge_build_config)
  unified_board_names = set([b[config_lib.CONFIG_TEMPLATE_REFERENCE_BOARD_NAME]
                             for b in unified_builds])
  board_names = separate_board_names | unified_board_names

  # TODO(crbug.com/648473): Remove these, after GE adds them to their data set.
  board_names = board_names.union(boards_dict['all_boards'])

  result = dict()
  for board in board_names:
    board_config = config_lib.BuildConfig(boards=[board])

    if board in chromeos_boards.brillo_boards:
      board_config.apply(site_config.templates.brillo)
    if board in chromeos_boards.lakitu_boards:
      board_config.apply(site_config.templates.lakitu)
    if board in chromeos_boards.lassen_boards:
      board_config.apply(site_config.templates.lassen)
    if board in ['x30evb']:
      board_config.apply(site_config.templates.x30evb)
    if board in chromeos_boards.loonix_boards:
      board_config.apply(site_config.templates.loonix)
    if board in chromeos_boards.wshwos_boards:
      board_config.apply(site_config.templates.wshwos)
    if board in chromeos_boards.dustbuster_boards:
      board_config.apply(site_config.templates.dustbuster)
    if board in chromeos_boards.moblab_boards:
      board_config.apply(site_config.templates.moblab)
    if board in chromeos_boards.accelerator_boards:
      board_config.apply(site_config.templates.accelerator)
    if board in chromeos_boards.termina_boards:
      board_config.apply(site_config.templates.termina)
    if board in chromeos_boards.nofactory_boards:
      board_config.apply(factory=False,
                         factory_toolkit=False,
                         factory_install_netboot=False,
                         images=remove_images(['factory_install']))
    if board in chromeos_boards.toolchains_from_source:
      board_config.apply(usepkg_toolchain=False)
    if board in chromeos_boards.noimagetest_boards:
      board_config.apply(image_test=False)
    if board in chromeos_boards.nohwqual_boards:
      board_config.apply(hwqual=False)
    if board in chromeos_boards.norootfs_verification_boards:
      board_config.apply(rootfs_verification=False)
    if board in chromeos_boards.base_layout_boards:
      board_config.apply(disk_layout='base')
    if board in chromeos_boards.beaglebone_boards:
      board_config.apply(site_config.templates.beaglebone)
    if board in chromeos_boards.builder_incompatible_binaries_boards:
      board_config.apply(unittests=False)

    result[board] = board_config

  return result


def CreateInternalBoardConfigs(site_config, boards_dict, ge_build_config):
  """Create mixin templates for each board."""
  result = CreateBoardConfigs(
      site_config, boards_dict, ge_build_config)

  for board in boards_dict['internal_boards']:
    if board in result:
      result[board].apply(site_config.templates.internal,
                          site_config.templates.official_chrome,
                          manifest=constants.OFFICIAL_MANIFEST)

  return result


def UpdateBoardConfigs(board_configs, boards, *args, **kwargs):
  """Update "board_configs" for selected chromeos_boards.

  Args:
    board_configs: Dict in CreateBoardConfigs format to filter from.
    boards: Iterable of boards to update in the dict.
    args: List of templates to apply.
    kwargs: Individual keys to update.

  Returns:
    Copy of board_configs dict with boards boards update with templates
    and values applied.
  """
  result = board_configs.copy()
  for b in boards:
    result[b] = result[b].derive(*args, **kwargs)

  return result


def ToolchainBuilders(site_config, boards_dict, ge_build_config):
  """Define templates used for toolchain builders.

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
    boards_dict: A dict mapping board types to board name collections.
    ge_build_config: Dictionary containing the decoded GE configuration file.
  """
  board_configs = CreateInternalBoardConfigs(
      site_config, boards_dict, ge_build_config)
  hw_test_list = HWTestList(ge_build_config)

  site_config.AddTemplate(
      'base_toolchain',
      # Full build, AFDO, latest-toolchain, -cros-debug, and simple-chrome.
      site_config.templates.full,
      display_label=config_lib.DISPLAY_LABEL_TOOLCHAIN,
      build_type=constants.TOOLCHAIN_TYPE,
      build_timeout=(15 * 60 + 50) * 60,
      # Need to re-enable platform_SyncCrash after issue crosbug/658864 is
      # fixed. Need to re-enable network_VPNConnect.* tests after issue
      # crosbug/585936 is fixed. Need to re-enable
      # power_DarkResumeShutdownServer after issue crosbug/689598 is fixed.
      # According to crosbug/653496 security_OpenFDs will not work for
      # non-official builds, so we need to leave it permanently disabled.
      # Need to reenable power_DarkResumeDisplay after crosbug/703250 is fixed.
      # Need to reenable cheets_SELinuxTest after crosbug/693308 is fixed.
      # Need to reenable security_SMMLocked when crosbug/654610 is fixed.
      # Add strict_toolchain_checks to perform toolchain-related checks
      useflags=config_lib.append_useflags([
          '-cros-debug',
          '-tests_security_OpenFDs',
          '-tests_platform_SyncCrash',
          '-tests_network_VPNConnect.l2tpipsec_xauth',
          '-tests_network_VPNConnect.l2tpipsec_psk',
          '-tests_power_DarkResumeShutdownServer',
          '-tests_power_DarkResumeDisplay',
          '-tests_security_SMMLocked',
          '-tests_cheets_SELinuxTest',
          'thinlto',
          'strict_toolchain_checks']),
      afdo_use=True,
      latest_toolchain=True,
      enable_skylab_hw_tests=True,
  )

  site_config.AddTemplate(
      'toolchain',
      site_config.templates.base_toolchain,
      site_config.templates.internal,
      site_config.templates.official_chrome,
      site_config.templates.no_vmtest_builder,
      images=['base', 'test', 'recovery'],
      manifest=constants.OFFICIAL_MANIFEST,
      manifest_version=True,
      git_sync=False,
      description='Toolchain Builds (internal)',
  )
  site_config.AddTemplate(
      'gcc_toolchain',
      site_config.templates.toolchain,
      description='Full release build with next minor GCC toolchain revision',
      useflags=config_lib.append_useflags(['next_gcc']),
      hw_tests=hw_test_list.ToolchainTestFull(
          constants.HWTEST_QUOTA_POOL,
          quota_account='toolchain'
      ),
      hw_tests_override=hw_test_list.ToolchainTestFull(
          constants.HWTEST_QUOTA_POOL,
          quota_account='toolchain'
      ),
  )
  site_config.AddTemplate(
      'llvm_toolchain',
      site_config.templates.toolchain,
      description='Full release build with LLVM toolchain',
      hw_tests=hw_test_list.ToolchainTestMedium(
          constants.HWTEST_QUOTA_POOL,
          quota_account='toolchain'
      ),
      hw_tests_override=hw_test_list.ToolchainTestMedium(
          constants.HWTEST_QUOTA_POOL,
          quota_account='toolchain'
      ),
  )
  site_config.AddTemplate(
      'llvm_next_toolchain',
      site_config.templates.llvm_toolchain,
      description='Full release build with LLVM (next) toolchain',
      useflags=config_lib.append_useflags(['llvm-next']),
  )
  site_config.AddTemplate(
      'llvm_tot_toolchain',
      site_config.templates.llvm_toolchain,
      useflags=config_lib.append_useflags(['llvm-tot']),
      description='Full release builds with a near-top-of-tree LLVM. Since '
      'this uses internal sources, it should only be used with LLVM revisions '
      'that have been reviewed manually somehow',
  )

  site_config.AddTemplate(
      'afdo_toolchain',
      site_config.templates.full,
      site_config.templates.official,
      site_config.templates.internal,
      site_config.templates.no_vmtest_builder,
      site_config.templates.no_hwtest_builder,
      git_sync=False,
      images=[],
      unittests=False,
      image_test=False,
      chrome_sdk=False,
      paygen=False,
      signer_tests=False,
      afdo_use=True,
      useflags=config_lib.append_useflags(['-cros-debug']),
      display_label=config_lib.DISPLAY_LABEL_TOOLCHAIN,
  )

  site_config.AddTemplate(
      'afdo_verify',
      site_config.templates.afdo_toolchain,
      images=['base', 'test'],
      image_test=True,
      unittests=True,
  )
  # This build config is dedicated to improve code layout of Chrome. It adopts
  # the Call-Chain Clustering (C3) approach and other techniques to improve
  # the code layout. It builds Chrome and generates an orderfile as a result.
  # The orderfile will be uploaded so Chrome in the future production will use
  # the orderfile to improve code layout.
  #
  # This builder is not a toolchain builder, i.e. it won't build all the
  # toolchain. Instead, it's a release builder. It's put here because it
  # belongs to the toolchain team.

  site_config.AddTemplate(
      'orderfile_generate_toolchain',
      site_config.templates.afdo_toolchain,
      orderfile_generate=True,
      useflags=config_lib.append_useflags(['orderfile_generate',
                                           '-orderfile_use']),
      description='Build Chrome and generate an orderfile for better layout',
  )

  site_config.AddTemplate(
      'orderfile_verify_toolchain',
      site_config.templates.afdo_verify,
      orderfile_verify=True,
      useflags=config_lib.append_useflags(['orderfile_verify',
                                           '-reorder_text_sections',
                                           'strict_toolchain_checks']),
      description='Verify the most recent orderfile is building correctly',
  )

  # This build config is used to asynchronously generate benchmark
  # AFDO profile. It inherites from afdo_toolchain template, i.e.
  # it is essentially a release builder but doesn't run hwtests, etc.
  # But it should run a special HWTest "AFDOGenerate" to collect
  # profiling data on a device. So this template needs to build image.
  site_config.AddTemplate(
      'benchmark_afdo_generate',
      site_config.templates.afdo_toolchain,
      images=['test'],
      hw_tests=[
          config_lib.HWTestConfig(constants.HWTEST_AFDO_SUITE,
                                  pool=constants.HWTEST_MACH_POOL,
                                  timeout=constants.AFDO_GENERATE_TIMEOUT,
                                  priority=constants.HWTEST_BUILD_PRIORITY,
                                  file_bugs=True)],
      hw_tests_override=[
          config_lib.HWTestConfig(constants.HWTEST_AFDO_SUITE,
                                  pool=constants.HWTEST_MACH_POOL,
                                  timeout=constants.AFDO_GENERATE_TIMEOUT,
                                  priority=constants.HWTEST_BUILD_PRIORITY,
                                  file_bugs=True)],
      afdo_generate_async=True,
      # FIXME(tcwang): Might need to revisit this later.
      # For now, use the same useflags as PFQ AFDO generator:
      # In other words, it turns off afdo_use, thinlto, cfi compared to
      # release images.
      afdo_use=False,
      useflags=config_lib.append_useflags(['-transparent_hugepage',
                                           '-debug_fission',
                                           '-thinlto',
                                           '-cfi']),
      description='Generate AFDO profile based on benchmarks.'
  )

  # This build config is used to verify merged and redacted AFDO profile,
  # aka. profiles for release, can build packages without any problems.
  site_config.AddTemplate(
      'release_afdo_verify',
      site_config.templates.afdo_verify,
      description='Verify the most recent AFDO profile for release can '
                  'build Chrome images correctly.'
  )

  ### Toolchain waterfall entries.
  ### Toolchain builder configs: 3 architectures {amd64,arm,arm64}
  ###                          x 1 toolchains {llvm-next}
  ### All of these builders should be slaves of 'master-toolchain'.

  ### Master toolchain config.
  master = site_config.Add(
      'master-toolchain',
      site_config.templates.toolchain,
      boards=[],
      description='Toolchain master (all others are slaves).',
      master=True,
      sync_chrome=True,
      health_alert_recipients=['c-compiler-chrome@google.com'],
      health_threshold=1,
      afdo_use=False,
      slave_configs=[],
      # 3 PM UTC is 7 AM PST (no daylight savings)
      schedule='0 15 * * *',
  )

  def toolchainSlaveHelper(name, board, *args, **kwargs):
    master.AddSlaves([
        site_config.Add(
            name + '-llvm-next-toolchain',
            site_config.templates.llvm_next_toolchain,
            *args,
            boards=[board],
            **kwargs
        )
    ])

  # Create all waterfall slave builders.
  toolchainSlaveHelper('amd64', 'eve')
  toolchainSlaveHelper('arm', 'veyron_mighty')
  toolchainSlaveHelper('arm64', 'kevin')

  #
  # Create toolchain tryjob builders.
  #
  builder_to_boards_dict = config_lib.GroupBoardsByBuilder(
      ge_build_config[config_lib.CONFIG_TEMPLATE_BOARDS])

  toolchain_tryjob_boards = builder_to_boards_dict[
      config_lib.CONFIG_TEMPLATE_RELEASE] | boards_dict['all_boards']

  site_config.AddForBoards(
      'llvm-toolchain',
      toolchain_tryjob_boards,
      board_configs,
      site_config.templates.llvm_toolchain,
  )
  site_config.AddForBoards(
      'llvm-next-toolchain',
      toolchain_tryjob_boards,
      board_configs,
      site_config.templates.llvm_next_toolchain,
  )
  site_config.Add(
      'llvm-clang-tidy-toolchain',
      site_config.templates.toolchain,
      site_config.templates.no_hwtest_builder,
      description='Full release build with LLVM toolchain, with clang-tidy)',
      chrome_sdk=False,
      # Run clang-tidy specific stages.
      builder_class_name='clang_tidy_builders.ClangTidyBuilder',
      useflags=config_lib.append_useflags(['clang_tidy']),
      boards=['grunt'],
      # Weekly on Sunday 3 AM UTC
      schedule='0 0 3 * * 0 *',
  )

  def PGOBuilders(name, board):
    site_config.Add(
        name + '-pgo-generate-llvm-next-toolchain',
        site_config.templates.llvm_next_toolchain,
        site_config.templates.no_hwtest_builder,
        description='Full release build with PGO instrumented LLVM toolchain)',
        chrome_sdk=False,
        # Run PGO generate specific stages.
        builder_class_name='pgo_generate_builders.PGOGenerateBuilder',
        useflags=config_lib.append_useflags(['llvm_pgo_generate',
                                             '-llvm_pgo_use']),
        boards=[board],
        images=['base'],
        # Build chrome as C++ training set, and kernel as C training set.
        packages=[
            'chromeos-base/chromeos-chrome',
            'virtual/linux-sources'
        ],
        # Weekly on Sunday 1 AM UTC
        schedule='0 0 1 * * 0 *',
    )
  # Create three PGO profile collecting builders.
  PGOBuilders('amd64', 'eve')
  PGOBuilders('arm', 'kevin')
  PGOBuilders('arm64', 'kevin64')

  # All *-generic boards are external.
  site_config.Add(
      'eve-llvm-tot-toolchain',
      site_config.templates.llvm_tot_toolchain,
      vm_tests=[config_lib.VMTestConfig(
          constants.VM_SUITE_TEST_TYPE,
          test_suite='smoke',
          use_ctest=False)],
      vm_tests_override=TRADITIONAL_VM_TESTS_SUPPORTED,
      boards=['eve'],
  )
  site_config.Add(
      'kevin-llvm-tot-toolchain',
      site_config.templates.llvm_tot_toolchain,
      site_config.templates.no_vmtest_builder,
      boards=['kevin'],
  )

  site_config.Add(
      'benchmark-afdo-generate',
      site_config.templates.benchmark_afdo_generate,
      boards=['chell'],
      # 10 AM UTC is 2 AM PST (no daylight savings)
      # Repeat every 12 hours
      schedule='0 10/12 * * *',
      health_alert_recipients=['c-compiler-chrome@google.com'],
  )

  site_config.Add(
      'chrome-release-afdo-verify',
      site_config.templates.release_afdo_verify,
      boards=['eve'],
      chrome_afdo_verify=True,
      # Start at 7 hours after benchmark-afdo-generate, to
      # give the builder enough time to finish
      schedule='0 5/12 * * *',
      # FIXME(tcwang): Enable health alert once deployed
      #health_alert_recipients=['c-compiler-chrome@google.com'],
  )

  def KernelAFDOPublishBuilders(name, board, schedule):
    site_config.Add(
        name + '-release-afdo-verify',
        site_config.templates.release_afdo_verify,
        boards=[board],
        kernel_afdo_verify=True,
        schedule=schedule,
        health_alert_recipients=['c-compiler-chrome@google.com'],
        # Send emails if this builder fails once
        health_threshold=1,
    )

  KernelAFDOPublishBuilders('kernel-3_14', 'lulu', '0 11 * * 1')
  KernelAFDOPublishBuilders('kernel-3_18', 'chell', '0 17 * * 1')
  KernelAFDOPublishBuilders('kernel-4_4', 'eve', '0 23 * * 1')

  site_config.Add(
      'orderfile-generate-toolchain',
      site_config.templates.orderfile_generate_toolchain,
      # The board should not matter much, since we are not running
      # anything on the board.
      boards=['terra'],
      # 10 AM UTC is 2 AM PST (no daylight savings)
      # Repeat every 12 hours
      schedule='0 10/12 * * *',
      health_alert_recipients=['c-compiler-chrome@google.com'],
      # Send emails if this builder fails once
      health_threshold=1,
  )

  site_config.Add(
      'orderfile-verify-toolchain',
      site_config.templates.orderfile_verify_toolchain,
      # Only test on X86 for now.
      boards=['eve'],
      # 2 PM UTC is 6 AM PST (no daylight savings)
      # Start this builder 4 hours after orderfile-generate-toolchain
      # Repeat every 12 hours
      schedule='0 2/12 * * *',
      health_alert_recipients=['c-compiler-chrome@google.com'],
      # Send emails if this builder fails once
      health_threshold=1,
  )

  # This config is manually run when we want to generate a 'release' AFDO
  # profile, which is a mixture of CWP/benchmark profiles. This is done
  # manually once per branch, since these are (currently) hand-rolled into
  # branches for Android and Linux builds of Chrome. See crbug.com/858856 for
  # context (comment #4 in particular).
  #
  # FIXME(gbiv): Ideally, this should be done as a part of the regular AFDO
  # pipeline if Chrome OS finds these profiles useful.
  site_config.Add(
      'release-afdo-profile-generate',
      # No board is necessary; this just runs a few commands inside of the SDK.
      boards=[],
      build_type=constants.TOOLCHAIN_TYPE,
      builder_class_name='release_profile_builders.ReleaseProfileMergeBuilder',
      chrome_sdk=False,
      description='Generates merged AFDO profiles for select Chrome releases',
      display_label=config_lib.DISPLAY_LABEL_TOOLCHAIN,
  )


def PreCqBuilders(site_config, boards_dict, ge_build_config):
  """Create all build configs associated with the PreCQ.

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
    boards_dict: A dict mapping board types to board name collections.
    ge_build_config: Dictionary containing the decoded GE configuration file.
  """
  board_configs = CreateInternalBoardConfigs(
      site_config, boards_dict, ge_build_config)
  hw_test_list = HWTestList(ge_build_config)

  # The PreCQ Launcher doesn't limit eternal PreCQ builds to external
  # CLs.  as a hack, use internal checkouts for external builds so
  # they can apply (and ignore) internal CLs. crbug.com/882965
  for b in (chromeos_boards.arm_external_boards
            | chromeos_boards.x86_external_boards):
    board_configs[b].apply(site_config.templates.internal)

  site_config.AddTemplate(
      'pre_cq',
      site_config.templates.paladin,
      display_label=config_lib.DISPLAY_LABEL_PRECQ,
      luci_builder=config_lib.LUCI_BUILDER_PRECQ,
      build_type=constants.PRE_CQ_TYPE,
      build_timeout=4 * 60 * 60,
      pre_cq=True,
      archive=False,
      chrome_sdk=False,
      chroot_replace=True,
      debug_symbols=False,
      prebuilts=False,
      cpe_export=False,
      vm_tests=[config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                        test_suite='smoke',
                                        use_ctest=False)],
      vm_tests_override=None,
      description='Verifies compilation, building an image, and vm/unit tests '
                  'if supported.',
      doc='https://dev.chromium.org/chromium-os/build/builder-overview#'
          'TOC-Pre-CQ',
      sanity_check_threshold=3,
  )

  # Pre-CQ targets that only check compilation and unit tests.
  site_config.AddTemplate(
      'unittest_only_pre_cq',
      site_config.templates.pre_cq,
      site_config.templates.no_vmtest_builder,
      description='Verifies compilation and unit tests only',
      compilecheck=True,
  )

  # Pre-CQ targets that don't run VMTests.
  site_config.AddTemplate(
      'no_vmtest_pre_cq',
      site_config.templates.pre_cq,
      site_config.templates.no_vmtest_builder,
      description='Verifies compilation, building an image, and unit tests '
                  'if supported.',
  )

  # Pre-CQ targets that only check compilation.
  site_config.AddTemplate(
      'compile_only_pre_cq',
      site_config.templates.unittest_only_pre_cq,
      description='Verifies compilation only',
      unittests=False,
  )

  site_config.AddWithoutTemplate(
      'pre-cq-launcher',
      site_config.templates.paladin,
      site_config.templates.internal_paladin,
      site_config.templates.no_vmtest_builder,
      site_config.templates.no_hwtest_builder,
      boards=[],
      display_label=config_lib.DISPLAY_LABEL_PRECQ,
      build_type=constants.PRE_CQ_LAUNCHER_TYPE,
      luci_builder=config_lib.LUCI_BUILDER_PRECQ_LAUNCHER,
      description='Launcher for Pre-CQ builders',
      manifest_version=False,
      doc='https://dev.chromium.org/chromium-os/build/builder-overview#'
          'TOC-Pre-CQ',
      schedule='with 3m interval',
  )

  # Add a pre-cq config for every board.
  site_config.AddForBoards(
      'pre-cq',
      boards_dict['all_boards'],
      board_configs,
      site_config.templates.pre_cq,
  )
  # Add special pre-cq for generic build tests with non-default kernel version
  site_config.AddForBoards(
      'linux-v4_4-pre-cq',
      boards_dict['generic_kernel_boards'],
      board_configs,
      site_config.templates.pre_cq,
      useflags=config_lib.append_useflags(['kernel-4_4', '-kernel-4_14',
                                           '-kernel-4_19']),
  )
  site_config.AddForBoards(
      'linux-v4_4-buildtest-compile-only-pre-cq',
      boards_dict['generic_kernel_boards'],
      board_configs,
      site_config.templates.compile_only_pre_cq,
      useflags=config_lib.append_useflags(['kernel-4_4', '-kernel-4_14',
                                           '-kernel-4_19',
                                           '-clang', 'buildtest',
                                           '-chrome_internal']),
      packages=['sys-kernel/chromeos-kernel-4_4'],
  )
  site_config.AddForBoards(
      'linux-v4_14-pre-cq',
      boards_dict['generic_kernel_boards'],
      board_configs,
      site_config.templates.pre_cq,
      useflags=config_lib.append_useflags(['-kernel-4_4', 'kernel-4_14',
                                           '-kernel-4_19']),
  )
  site_config.AddForBoards(
      'linux-v4_14-buildtest-compile-only-pre-cq',
      boards_dict['generic_kernel_boards'],
      board_configs,
      site_config.templates.compile_only_pre_cq,
      useflags=config_lib.append_useflags(['-kernel-4_4', 'kernel-4_14',
                                           '-kernel-4_19',
                                           '-clang', 'buildtest']),
      packages=['sys-kernel/chromeos-kernel-4_14'],
  )
  site_config.AddForBoards(
      'linux-v4_19-pre-cq',
      boards_dict['generic_kernel_boards'],
      board_configs,
      site_config.templates.pre_cq,
      useflags=config_lib.append_useflags(['-kernel-4_4', '-kernel-4_14',
                                           'kernel-4_19']),
  )
  site_config.AddForBoards(
      'linux-v4_19-buildtest-compile-only-pre-cq',
      boards_dict['generic_kernel_boards'],
      board_configs,
      site_config.templates.compile_only_pre_cq,
      useflags=config_lib.append_useflags(['-kernel-4_4', '-kernel-4_14',
                                           'kernel-4_19',
                                           '-clang', 'buildtest']),
      packages=['sys-kernel/chromeos-kernel-4_19'],
  )
  site_config.AddForBoards(
      'no-vmtest-pre-cq',
      boards_dict['all_boards'],
      board_configs,
      site_config.templates.no_vmtest_pre_cq,
  )
  site_config.AddForBoards(
      'compile-only-pre-cq',
      boards_dict['all_boards'],
      board_configs,
      site_config.templates.compile_only_pre_cq,
  )
  site_config.Add(
      constants.BINHOST_PRE_CQ,
      site_config.templates.pre_cq,
      site_config.templates.no_vmtest_pre_cq,
      site_config.templates.internal,
      boards=[],
      binhost_test=True,
  )

  # Wifi specific PreCQ.
  site_config.AddTemplate(
      'wificell_pre_cq',
      site_config.templates.pre_cq,
      unittests=False,
      hw_tests=hw_test_list.WiFiCellPoolPreCQ(),
      hw_tests_override=hw_test_list.WiFiCellPoolPreCQ(),
      archive=True,
      image_test=False,
      description='WiFi tests acting as pre-cq for WiFi related changes',
  )

  _wifi_boards = frozenset([
      'winky',
      'veyron_speedy',
      'veyron_jerry',
      'lulu',
      'cyan',
      'elm',
  ])

  site_config.AddForBoards(
      'wificell-pre-cq',
      _wifi_boards,
      board_configs,
      site_config.templates.wificell_pre_cq,
  )

  # Bluestreak specific PreCQ.
  site_config.Add(
      'bluestreak-pre-cq',
      board_configs['guado'],
      site_config.templates.pre_cq,
      hw_tests=hw_test_list.BluestreakPoolPreCQ(),
      hw_tests_override=hw_test_list.BluestreakPoolPreCQ(),
      archive=True,
      description='Bluestreak tests as pre-cq for CFM related changes',
  )

  site_config.Add(
      'signer-pre-cq',
      site_config.templates.pre_cq,
      site_config.templates.internal,
      site_config.templates.no_hwtest_builder,
      site_config.templates.no_vmtest_builder,
      boards=[],
      builder_class_name='test_builders.SignerTestsBuilder',
      description='Run the signer unittests.',
  )

  site_config.Add(
      'chromite-pre-cq',
      site_config.templates.pre_cq,
      site_config.templates.internal,
      site_config.templates.no_hwtest_builder,
      site_config.templates.no_vmtest_builder,
      boards=[],
      builder_class_name='test_builders.ChromiteTestsBuilder',
      description='Run the chromite unittests.',
  )


  site_config.Add(
      'cbuildbot-launch-pre-cq',
      site_config.templates.pre_cq,
      site_config.templates.internal,
      site_config.templates.no_hwtest_builder,
      site_config.templates.no_vmtest_builder,
      boards=[],
      builder_class_name='test_builders.CbuildbotLaunchTestBuilder',
      description='Run cbuildbot_launch test builds.',
  )

  # Pre-cq for lakitu's public overlay.
  site_config.Add(
      'lakitu-external-pre-cq',
      site_config.templates.pre_cq,
      board_configs['lakitu'],
      site_config.templates.lakitu,
      site_config.templates.external,
      useflags=config_lib.append_useflags(['-chrome_internal']),
  )

  site_config.AddWithoutTemplate(
      'chromeos-infra-puppet-pre-cq',
      site_config.templates.pre_cq,
      site_config.templates.internal,
      site_config.templates.no_hwtest_builder,
      site_config.templates.no_unittest_builder,
      site_config.templates.no_vmtest_builder,
      boards=[],
      builder_class_name='infra_builders.PuppetPreCqBuilder',
      use_sdk=True,
      build_timeout=60 * 60,
      description='Test Puppet specs',
      doc='https://chrome-internal.googlesource.com/'
          'chromeos/chromeos-admin/+/HEAD/puppet/README.md',
  )

  site_config.AddWithoutTemplate(
      'chromeos-infra-go-pre-cq',
      site_config.templates.pre_cq,
      site_config.templates.no_hwtest_builder,
      site_config.templates.no_unittest_builder,
      site_config.templates.no_vmtest_builder,
      boards=[],
      builder_class_name='infra_builders.InfraGoPreCqBuilder',
      use_sdk=True,
      build_timeout=60 * 60,
      description='Test building Chromium OS infra Go binaries',
      doc='https://goto.google.com/cros-infra-go-packaging',
  )

  site_config.AddWithoutTemplate(
      'chromeos-infra-unittests-pre-cq',
      site_config.templates.pre_cq,
      site_config.templates.internal,
      site_config.templates.no_hwtest_builder,
      site_config.templates.no_unittest_builder,
      site_config.templates.no_vmtest_builder,
      boards=[],
      builder_class_name='infra_builders.InfraUnittestsPreCqBuilder',
      use_sdk=True,
      build_timeout=60 * 60,
      description='Run unittests for infra repositories',
  )


def AndroidTemplates(site_config):
  """Apply Android specific config to site_config

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
  """
  # Generic template shared by all Android versions.
  site_config.AddTemplate(
      'generic_android_pfq',
      site_config.templates.no_vmtest_builder,
      build_type=constants.ANDROID_PFQ_TYPE,
      uprev=False,
      overlays=constants.BOTH_OVERLAYS,
      manifest_version=True,
      android_rev=constants.ANDROID_REV_LATEST,
      description='Preflight Android Uprev & Build (internal)',
      luci_builder=config_lib.LUCI_BUILDER_PFQ,
  )

  # Template for Android NYC.
  site_config.AddTemplate(
      'nyc_android_pfq',
      site_config.templates.generic_android_pfq,
      display_label=config_lib.DISPLAY_LABEL_NYC_ANDROID_PFQ,
      android_package='android-container-nyc',
      android_import_branch=constants.ANDROID_NYC_BUILD_BRANCH,
      android_gts_build_branch='git_nyc-mr2-dev',
  )

  # Template for Android Pi.
  site_config.AddTemplate(
      'pi_android_pfq',
      site_config.templates.generic_android_pfq,
      site_config.templates.internal,
      display_label=config_lib.DISPLAY_LABEL_PI_ANDROID_PFQ,
      android_package='android-container-pi',
      android_import_branch=constants.ANDROID_PI_BUILD_BRANCH,
  )

  # Template for Android VM Pi.
  site_config.AddTemplate(
      'vmpi_android_pfq',
      site_config.templates.generic_android_pfq,
      site_config.templates.internal,
      display_label=config_lib.DISPLAY_LABEL_VMPI_ANDROID_PFQ,
      android_package='android-vm-pi',
      android_import_branch=constants.ANDROID_VMPI_BUILD_BRANCH,
  )

  # Template for Android Qt.
  site_config.AddTemplate(
      'qt_android_pfq',
      site_config.templates.generic_android_pfq,
      site_config.templates.internal,
      display_label=config_lib.DISPLAY_LABEL_QT_ANDROID_PFQ,
      android_package='android-container-qt',
      android_import_branch=constants.ANDROID_QT_BUILD_BRANCH,
  )

  # Template for Android Master.
  site_config.AddTemplate(
      'mst_android_pfq',
      site_config.templates.generic_android_pfq,
      site_config.templates.internal,
      display_label=config_lib.DISPLAY_LABEL_MST_ANDROID_PFQ,
      android_package='android-container-master-arc-dev',
      android_import_branch=constants.ANDROID_MST_BUILD_BRANCH,
  )

  # Mixin for masters.
  site_config.AddTemplate(
      'master_android_pfq_mixin',
      site_config.templates.internal,
      site_config.templates.no_vmtest_builder,
      boards=[],
      master=True,
      push_overlays=constants.BOTH_OVERLAYS,
  )


def AndroidPfqBuilders(site_config, boards_dict, ge_build_config):
  """Create all build configs associated with the Android PFQ.

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
    boards_dict: A dict mapping board types to board name collections.
    ge_build_config: Dictionary containing the decoded GE configuration file.
  """
  board_configs = CreateInternalBoardConfigs(
      site_config, boards_dict, ge_build_config)
  hw_test_list = HWTestList(ge_build_config)


  # Android MST master.
  mst_master_config = site_config.Add(
      constants.MST_ANDROID_PFQ_MASTER,
      site_config.templates.mst_android_pfq,
      site_config.templates.master_android_pfq_mixin,
      schedule='with 150m interval',
  )

  _mst_hwtest_boards = frozenset([])
  _mst_hwtest_skylab_boards = frozenset([])
  _mst_no_hwtest_boards = frozenset([])
  _mst_no_hwtest_experimental_boards = frozenset([])
  _mst_vmtest_boards = frozenset([
      'betty-arcmaster',
  ])

  # Android QT master.
  qt_master_config = site_config.Add(
      constants.QT_ANDROID_PFQ_MASTER,
      site_config.templates.qt_android_pfq,
      site_config.templates.master_android_pfq_mixin,
      schedule='with 150m interval',
  )

  _qt_hwtest_boards = frozenset([
      'eve-arcnext',
  ])
  _qt_hwtest_skylab_boards = frozenset([
      'eve-arcnext',
  ])
  _qt_no_hwtest_boards = frozenset([])
  _qt_no_hwtest_experimental_boards = frozenset([])
  _qt_vmtest_boards = frozenset([
      'betty-qt-arc',
  ])

  # Android PI master.
  pi_master_config = site_config.Add(
      constants.PI_ANDROID_PFQ_MASTER,
      site_config.templates.pi_android_pfq,
      site_config.templates.master_android_pfq_mixin,
      schedule='with 60m interval',
  )

  _pi_no_hwtest_boards = frozenset([
      'caroline',
      'coral',
      'reef',
  ])
  _pi_no_hwtest_experimental_boards = frozenset([])
  _pi_hwtest_boards = frozenset([
      'eve',
      'grunt',
      'kevin',
  ])
  _pi_hwtest_experimental_boards = frozenset([])
  _pi_hwtest_skylab_boards = frozenset([
      'eve',
      'grunt',
      'kevin',
  ])
  _pi_vmtest_boards = frozenset([
      'betty-arcnext',
      'betty-pi-arc',
  ])
  _pi_vmtest_experimental_boards = frozenset([
  ])

  # Android VM PI master.
  vmpi_master_config = site_config.Add(
      constants.VMPI_ANDROID_PFQ_MASTER,
      site_config.templates.vmpi_android_pfq,
      site_config.templates.master_android_pfq_mixin,
      schedule='with 1440m interval',
  )

  _vmpi_no_hwtest_boards = frozenset([
      'eve-arcvm',
  ])
  _vmpi_no_hwtest_experimental_boards = frozenset([])
  _vmpi_hwtest_boards = frozenset([])
  _vmpi_hwtest_experimental_boards = frozenset([])
  _vmpi_vmtest_boards = frozenset([])
  _vmpi_vmtest_experimental_boards = frozenset([])

  # Android NYC master.
  nyc_master_config = site_config.Add(
      constants.NYC_ANDROID_PFQ_MASTER,
      site_config.templates.nyc_android_pfq,
      site_config.templates.master_android_pfq_mixin,
      schedule='with 150m interval',
  )

  _nyc_hwtest_boards = frozenset([
      'veyron_mighty',
      'gandof',
  ])
  _nyc_hwtest_skylab_boards = frozenset([])
  _nyc_no_hwtest_boards = frozenset([
      'veyron_tiger',
  ])
  _nyc_no_hwtest_experimental_boards = frozenset([])
  _nyc_vmtest_boards = frozenset([
      'betty',
      'betty-arc64',
  ])

  # Android MST slaves.
  mst_master_config.AddSlaves(
      site_config.AddForBoards(
          'mst-android-pfq',
          _mst_hwtest_boards - _mst_hwtest_skylab_boards,
          board_configs,
          site_config.templates.mst_android_pfq,
          hw_tests=hw_test_list.SharedPoolPFQ(),
      ) +
      site_config.AddForBoards(
          'mst-android-pfq',
          _mst_hwtest_skylab_boards,
          board_configs,
          site_config.templates.mst_android_pfq,
          enable_skylab_hw_tests=True,
          hw_tests=hw_test_list.SharedPoolPFQ(),
      ) +
      site_config.AddForBoards(
          'mst-android-pfq',
          _mst_no_hwtest_boards,
          board_configs,
          site_config.templates.mst_android_pfq,
      ) +
      site_config.AddForBoards(
          'mst-android-pfq',
          _mst_no_hwtest_experimental_boards,
          board_configs,
          site_config.templates.mst_android_pfq,
          important=False,
      ) +
      site_config.AddForBoards(
          'mst-android-pfq',
          _mst_vmtest_boards,
          board_configs,
          site_config.templates.mst_android_pfq,
          vm_tests=[config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                            test_suite='smoke')],
      )
  )

  # Android QT slaves.
  qt_master_config.AddSlaves(
      site_config.AddForBoards(
          'qt-android-pfq',
          _qt_hwtest_boards - _qt_hwtest_skylab_boards,
          board_configs,
          site_config.templates.qt_android_pfq,
          hw_tests=hw_test_list.SharedPoolPFQ(),
      ) +
      site_config.AddForBoards(
          'qt-android-pfq',
          _qt_hwtest_skylab_boards,
          board_configs,
          site_config.templates.qt_android_pfq,
          enable_skylab_hw_tests=True,
          hw_tests=hw_test_list.SharedPoolPFQ(),
      ) +
      site_config.AddForBoards(
          'qt-android-pfq',
          _qt_no_hwtest_boards,
          board_configs,
          site_config.templates.qt_android_pfq,
      ) +
      site_config.AddForBoards(
          'qt-android-pfq',
          _qt_no_hwtest_experimental_boards,
          board_configs,
          site_config.templates.qt_android_pfq,
          important=False,
      ) +
      site_config.AddForBoards(
          'qt-android-pfq',
          _qt_vmtest_boards,
          board_configs,
          site_config.templates.qt_android_pfq,
          vm_tests=[config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                            test_suite='smoke')],
      )
  )

  # Android PI slaves.
  pi_master_config.AddSlaves(
      site_config.AddForBoards(
          'pi-android-pfq',
          _pi_hwtest_boards - _pi_hwtest_skylab_boards,
          board_configs,
          site_config.templates.pi_android_pfq,
          hw_tests=hw_test_list.SharedPoolPFQ(),
      ) +
      site_config.AddForBoards(
          'pi-android-pfq',
          _pi_hwtest_skylab_boards,
          board_configs,
          site_config.templates.pi_android_pfq,
          enable_skylab_hw_tests=True,
          hw_tests=hw_test_list.SharedPoolPFQ(),
      ) +
      site_config.AddForBoards(
          'pi-android-pfq',
          _pi_no_hwtest_boards,
          board_configs,
          site_config.templates.pi_android_pfq,
      ) +
      site_config.AddForBoards(
          'pi-android-pfq',
          _pi_no_hwtest_experimental_boards,
          board_configs,
          site_config.templates.pi_android_pfq,
          important=False,
      ) +
      site_config.AddForBoards(
          'pi-android-pfq',
          _pi_hwtest_experimental_boards,
          board_configs,
          site_config.templates.pi_android_pfq,
          important=False,
          hw_tests=hw_test_list.SharedPoolPFQ(),
      ) +
      site_config.AddForBoards(
          'pi-android-pfq',
          _pi_vmtest_boards,
          board_configs,
          site_config.templates.pi_android_pfq,
          vm_tests=[config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                            test_suite='smoke')],
      ) +
      site_config.AddForBoards(
          'pi-android-pfq',
          _pi_vmtest_experimental_boards,
          board_configs,
          site_config.templates.pi_android_pfq,
          important=False,
          vm_tests=[config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                            test_suite='smoke')],
      )
  )

  # Android VM PI slaves.
  vmpi_master_config.AddSlaves(
      site_config.AddForBoards(
          'vmpi-android-pfq',
          _vmpi_hwtest_boards,
          board_configs,
          site_config.templates.vmpi_android_pfq,
          hw_tests=hw_test_list.SharedPoolPFQ(),
      ) +
      site_config.AddForBoards(
          'vmpi-android-pfq',
          _vmpi_no_hwtest_boards,
          board_configs,
          site_config.templates.vmpi_android_pfq,
      ) +
      site_config.AddForBoards(
          'vmpi-android-pfq',
          _vmpi_no_hwtest_experimental_boards,
          board_configs,
          site_config.templates.vmpi_android_pfq,
          important=False,
      ) +
      site_config.AddForBoards(
          'vmpi-android-pfq',
          _vmpi_hwtest_experimental_boards,
          board_configs,
          site_config.templates.vmpi_android_pfq,
          important=False,
          hw_tests=hw_test_list.SharedPoolPFQ(),
      ) +
      site_config.AddForBoards(
          'vmpi-android-pfq',
          _vmpi_vmtest_boards,
          board_configs,
          site_config.templates.vmpi_android_pfq,
          vm_tests=[config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                            test_suite='smoke')],
      ) +
      site_config.AddForBoards(
          'vmpi-android-pfq',
          _vmpi_vmtest_experimental_boards,
          board_configs,
          site_config.templates.vmpi_android_pfq,
          important=False,
          vm_tests=[config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                            test_suite='smoke')],
      )
  )

  # Android NYC slaves.
  nyc_master_config.AddSlaves(
      site_config.AddForBoards(
          'nyc-android-pfq',
          _nyc_hwtest_boards - _nyc_hwtest_skylab_boards,
          board_configs,
          site_config.templates.nyc_android_pfq,
          hw_tests=hw_test_list.SharedPoolPFQ(),
      ) +
      site_config.AddForBoards(
          'nyc-android-pfq',
          _nyc_hwtest_skylab_boards,
          board_configs,
          site_config.templates.nyc_android_pfq,
          enable_skylab_hw_tests=True,
          hw_tests=hw_test_list.SharedPoolPFQ(),
      ) +
      site_config.AddForBoards(
          'nyc-android-pfq',
          _nyc_no_hwtest_boards,
          board_configs,
          site_config.templates.nyc_android_pfq,
      ) +
      site_config.AddForBoards(
          'nyc-android-pfq',
          _nyc_no_hwtest_experimental_boards,
          board_configs,
          site_config.templates.nyc_android_pfq,
          important=False,
      ) +
      site_config.AddForBoards(
          'nyc-android-pfq',
          _nyc_vmtest_boards,
          board_configs,
          site_config.templates.nyc_android_pfq,
          vm_tests=[config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                            test_suite='smoke'),],
      )
  )


def FullBuilders(site_config, boards_dict, ge_build_config):
  """Create all full builders.

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
    boards_dict: A dict mapping board types to board name collections.
    ge_build_config: Dictionary containing the decoded GE configuration file.
  """
  active_builders = frozenset([
      'amd64-generic',
      'arm-generic',
      'arm64-generic',
      'kevin',
      'kevin64',
      'oak',
      'tael',
      'tatl',
  ])

  # Move the following builders to active_builders once they are consistently
  # green.
  unstable_builders = frozenset([
      'lakitu',  # TODO: Re-enable after crbug.com/919630 resolved.
  ])

  external_board_configs = CreateBoardConfigs(
      site_config, boards_dict, ge_build_config)

  site_config.AddForBoards(
      config_lib.CONFIG_TYPE_FULL,
      boards_dict['all_full_boards'],
      external_board_configs,
      site_config.templates.full,
      site_config.templates.build_external_chrome,
      internal=False,
      manifest_repo_url=config_lib.GetSiteParams().MANIFEST_URL,
      overlays=constants.PUBLIC_OVERLAYS,
      prebuilts=constants.PUBLIC)

  master_config = site_config.Add(
      'master-full',
      site_config.templates.full,
      site_config.templates.internal,
      site_config.templates.build_external_chrome,
      boards=[],
      master=True,
      manifest_version=True,
      overlays=constants.PUBLIC_OVERLAYS,
      slave_configs=[],
      schedule='0 */3 * * *',
  )

  master_config.AddSlaves(
      site_config.ApplyForBoards(
          config_lib.CONFIG_TYPE_FULL,
          active_builders,
          manifest_version=True,
      )
  )

  master_config.AddSlaves(
      site_config.ApplyForBoards(
          config_lib.CONFIG_TYPE_FULL,
          unstable_builders,
          manifest_version=True,
          important=False,
      )
  )


def CqBuilders(site_config, boards_dict, ge_build_config):
  """Create all CQ build configs.

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
    boards_dict: A dict mapping board types to board name collections.
    ge_build_config: Dictionary containing the decoded GE configuration file.
  """
  site_config.AddTemplate(
      'cq_luci_slave',
      build_affinity=False,
  )


  board_configs = CreateInternalBoardConfigs(
      site_config, boards_dict, ge_build_config)
  hw_test_list = HWTestList(ge_build_config)

  _separate_boards = boards_dict['all_boards']
  _unified_builds = config_lib.GetUnifiedBuildConfigAllBuilds(ge_build_config)
  _unified_board_names = set([b[config_lib.CONFIG_TEMPLATE_REFERENCE_BOARD_NAME]
                              for b in _unified_builds])

  _paladin_boards = _separate_boards | _unified_board_names
  # List of paladin boards where the regular paladin config is important.
  _paladin_important_boards = frozenset([
      'amd64-generic',
      'arm-generic',
      'arm64-generic',
      'atlas',
      'auron_paine',
      'auron_yuna',
      'beaglebone',
      'betty',
      'betty-arc64',
      'betty-arcnext',
      'betty-pi-arc',
      'bob',
      'capri',
      'capri-zfpga',
      'caroline',
      'cave',
      'chell',
      'cheza',
      'cobblepot',
      'coral',
      'cyan',
      'drallion',
      'edgar',
      'elm',
      'eve',
      'eve-arcnext',
      'fizz',
      'fizz-accelerator',
      'flapjack',
      'gandof',
      'glados',
      'gonzo',
      'guado-accelerator',
      'hana',
      'hatch',
      'kalista',
      'kevin',
      'kevin64',
      'kevin-arcnext',
      'kip',
      'kukui',
      'lakitu',
      'lakitu-gpu',
      'lakitu-st',
      'lasilla-ground',
      'littlejoe',
      'monroe',
      'nami',
      'nocturne',
      'nyan_kitty',
      'octavius',
      'octopus',
      'rammus',
      'reef',
      'romer',
      'sarien',
      'samus',
      'scarlet',
      'sentry',
      'sludge',
      'tael',
      'tatl',
      'terra',
      'tidus',
      'veyron_jaq',
      'veyron_jerry',
      'veyron_mighty',
      'veyron_minnie',
      'veyron_rialto',
      'veyron_speedy',
      'veyron_tiger',
      'viking',
      'whirlwind',
      'winky',
      'wooten',
      'zork',
  ])

  # Paladin configs that exist and should be important as soon as they are
  # shown to be green. All new paladins should start in this group and get
  # promoted to _paladin_important_boards.
  #
  # A paladin is generally considered good enough for important if it can
  # pass the last ~20 build attempts, e.g. the builder page shows mostly green.
  # Note that paladins are expected to fail occasionally as they block bad CLs
  # from landing, a red paladin from a bad CL in the CQ is a working paladin.
  #
  # If the device is to be used with HW testing, the standard is higher, and
  # the device should be proven to be stable for at least three weeks.
  # Generally only PVT/MP stage devices should be used with HW testing due
  # to stability requirements for the commit queue. If a EVT/DVT device is
  # to be deployed in a HW test capacity as a paladin, it must be approved
  # by a member of the associated product team, the test infrastructure team,
  # and the CI team.
  #
  # The definition of what paladins run HW tests are in the
  # _paladin_hwtest_assignments table further down this script.
  _paladin_new_boards = frozenset([
      'betty-kernelnext',
      'bubs',
      'caroline-arcnext',
      'excelsior',
      'jacuzzi',
      'kefka-kernelnext',
      'kevin-arc64',
      'kumo',
      'puff',
      'veyron_jerry-kernelnext',
      'veyron_minnie-kernelnext',
      'volteer',
  ])

  # Paladin configs that exist and should stay as experimental until further
  # notice, preferably with a comment indicating why and a bug.
  _paladin_experimental_boards = _paladin_new_boards | frozenset([
      'fizz-moblab', # crbug.com/995170
      'gale', # http://b/140523449
      'grunt', # crbug.com/984614
      'guado', # crbug.com/950812
      'mistral', # b/140536001
      'moblab-generic-vm', # crbug.com/960998, crbug.com/976297
      'samus-kernelnext', # https://crbug.com/1004618
      'wizpig', # crbug.com/996944
  ])


  assert not (_paladin_experimental_boards & _paladin_important_boards), (
      'Experimental and important paladin board sets must be disjoint.')

  _paladin_active = _paladin_important_boards | _paladin_experimental_boards

  _paladin_simple_vmtest_boards = frozenset([
      'betty',
      'betty-arcnext',
      'betty-kernelnext',
      'betty-pi-arc',
  ])

  _paladin_devmode_vmtest_boards = frozenset([
      'betty',
      'betty-arcnext',
      'betty-kernelnext',
      'betty-pi-arc',
  ])

  _paladin_cros_vmtest_boards = frozenset([
      'betty',
      'betty-arcnext',
      'betty-kernelnext',
      'betty-pi-arc',
  ])

  _paladin_smoke_vmtest_boards = frozenset([
      'betty',
      'betty-arc64',
      'betty-arcnext',
      'betty-kernelnext',
      'betty-pi-arc',
  ])

  _paladin_default_vmtest_boards = frozenset([
      'betty',
      'betty-arcnext',
      'betty-kernelnext',
      'betty-pi-arc',
  ])

  # Jetstream devices run unique hw tests
  _paladin_jetstream_hwtest_boards = frozenset([
      'whirlwind',
      'gale',
      'mistral',
  ])

  _paladin_moblab_hwtest_boards = frozenset([
      'fizz-moblab',
  ])

  _paladin_chroot_replace_boards = frozenset([])

  _paladin_separate_symbols = frozenset([
      'amd64-generic',
  ])

  _paladin_bluestreak_hwtest_boards = frozenset([
      'guado',
  ])

  _paladin_enable_skylab_hwtest = frozenset([
      'auron_paine',
      'auron_yuna',
      'atlas',
      'bob',
      'caroline',
      'cave',
      'coral',
      'cyan',
      'edgar',
      'elm',
      'eve',
      'gale',
      'grunt',
      'hana',
      'kevin',
      'kip',
      'mistral',
      'nocturne',
      'nyan_kitty',
      'octopus',
      'reef',
      'scarlet',
      'sentry',
      'tidus',
      'veyron_mighty',
      'veyron_minnie',
      'veyron_speedy',
      'whirlwind',
      'winky',
      'wizpig',
  ])

  _paladin_separate_unittest_phase = frozenset([
      'grunt',
  ])

  ### Master paladin (CQ builder).
  master_config = site_config.Add(
      'master-paladin',
      site_config.templates.paladin,
      site_config.templates.internal_paladin,
      boards=[],
      master=True,
      binhost_test=True,
      push_overlays=constants.BOTH_OVERLAYS,
      description='Commit Queue master (all others are slaves)',
      schedule='with 2m interval',
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

  for board in _paladin_boards:
    assert board in board_configs, '%s not in board_configs' % board
    config_name = '%s-%s' % (board, constants.PALADIN_TYPE)
    customizations = config_lib.BuildConfig()
    base_config = board_configs[board]
    if board in _unified_board_names:
      for unibuild in _unified_builds:
        if board == unibuild[config_lib.CONFIG_TEMPLATE_REFERENCE_BOARD_NAME]:
          models = []
          for model in unibuild[config_lib.CONFIG_TEMPLATE_MODELS]:
            name = model[config_lib.CONFIG_TEMPLATE_MODEL_NAME]
            lab_board_name = model[config_lib.CONFIG_TEMPLATE_MODEL_BOARD_NAME]
            if (config_lib.CONFIG_TEMPLATE_MODEL_CQ_TEST_ENABLED in model
                and model[config_lib.CONFIG_TEMPLATE_MODEL_CQ_TEST_ENABLED]):
              models.append(config_lib.ModelTestConfig(
                  name, lab_board_name, enable_skylab=True))

          customizations.update(models=models)

    if board in _paladin_enable_skylab_hwtest:
      customizations.update(enable_skylab_hw_tests=True)

    if board in _paladin_moblab_hwtest_boards:
      customizations.update(
          hw_tests=[
              config_lib.HWTestConfig(
                  constants.HWTEST_MOBLAB_QUICK_SUITE,
                  timeout=90*60,
                  pool=constants.HWTEST_QUOTA_POOL),
              config_lib.HWTestConfig(
                  constants.HWTEST_TAST_CQ_SUITE,
                  pool=constants.HWTEST_QUOTA_POOL),
          ],
          hw_tests_override=None)
    if board in _paladin_jetstream_hwtest_boards:
      customizations.update(
          hw_tests=[
              hw_test_list.DefaultListCQ()[0],
              config_lib.HWTestConfig(
                  constants.HWTEST_JETSTREAM_COMMIT_SUITE,
                  pool=constants.HWTEST_QUOTA_POOL,
                  quota_account='cq',
              )
          ],
          hw_tests_override=None)
    if board in _paladin_bluestreak_hwtest_boards:
      customizations.update(
          hw_tests=hw_test_list.BluestreakPoolPreCQ(),
          hw_tests_override=hw_test_list.BluestreakPoolPreCQ())
    if board in _paladin_experimental_boards:
      customizations.update(important=False)
    if board in _paladin_chroot_replace_boards:
      customizations.update(chroot_replace=True)
    if (board in boards_dict['internal_boards']
        or board in _unified_board_names):
      customizations = customizations.derive(
          site_config.templates.internal,
          site_config.templates.official_chrome,
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
            config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                    test_suite='smoke'))

      customizations.update(vm_tests=vm_tests)

      if site_config.templates.paladin.vm_tests_override is not None:
        # Make sure any new tests are also in override.
        override = site_config.templates.paladin.vm_tests_override[:]
        for test in vm_tests:
          if test not in override:
            override.append(test)

        customizations.update(vm_tests_override=override)

    if base_config.get('internal'):
      customizations.update(
          description=(site_config.templates.paladin.description +
                       ' (internal)'))

    if board in _paladin_active:
      customizations.update(
          site_config.templates.cq_luci_slave,
      )

    if board in _paladin_separate_unittest_phase:
      customizations.update(unittests=False)

    if board in chromeos_boards.lakitu_boards:
      customizations.update(
          site_config.templates.lakitu_paladin_test_customizations)

    config = site_config.Add(
        config_name,
        site_config.templates.paladin,
        customizations,
        base_config,
    )

    if board in _paladin_active:
      master_config.AddSlave(config)

  # N.B. The ordering of columns here is coupled to the ordering of
  # suites returned by DefaultListCQ().  If you change the ordering here,
  # you must also change the ordering there.
  #
  # CAUTION: Only add devices to this table which are known to be stable in
  # the HW test lab, even low rates of flake from these devices quickly
  # add up and can destabilize the commit queue.
  #
  # TODO: Fill in any notable gaps in this table. crbug.com/730076
  # pylint: disable=bad-continuation, bad-whitespace, line-too-long
  _paladin_hwtest_assignments = frozenset([
    # bvt-inline      bvt-cq           bvt-arc             bvt-tast-cq          family
    ('winky',         'kip',           None,               'winky'),            # rambi (BYT)
    ('auron_paine',   'tidus',         'auron_yuna',       'auron_paine'),      # auron (BDW)
    ('veyron_mighty', 'veyron_speedy', 'veyron_minnie',    'veyron_mighty'),    # pinky (RK3288)
    ('wizpig',        'edgar',         'cyan',             'wizpig'),           # strago (BSW)
    ('cave',          'sentry',        None,               'cave'),             # glados (SKL)
    ('elm',           None,            'hana',             'elm'),              # oak (MTK8173)
    ('bob',           None,            'kevin',            'bob'),              # gru (RK3399)
    ('reef',          None,            None,               'reef'),             # reef (APL)
    ('coral',         None,            None,               'coral'),            # coral (APL)
    (None,            'eve',           'soraka',           'eve'),              # poppy (KBL)
    ('nocturne',      None,            None,               'nocturne'),         # Nocturne (KBL)
    ('atlas',         None,            None,               'atlas'),            # Atlas (KBL)
    ('octopus',       None,            None,               'octopus'),          # Octopus (GLK unibuild)
    ('scarlet'   ,    None,            None,               'scarlet'),          # scarlet (RK3399 unibuild)
    ('grunt',         None,            'grunt',            'grunt'),            # grunt (AMD unibuild)
  ])
  # pylint: enable=bad-continuation, bad-whitespace, line-too-long

  sharded_hw_tests = hw_test_list.DefaultListCQ()
  # Run provision suite first everywhere.
  default_tests = [sharded_hw_tests.pop(0)]
  config_default_reset = set()
  for board_assignments in _paladin_hwtest_assignments:
    assert len(board_assignments) == len(sharded_hw_tests)
    for board, suite in zip(board_assignments, sharded_hw_tests):
      if board is None:
        continue

      config_name = '%s-%s' % (board, constants.PALADIN_TYPE)
      # Only configurate hw_tests for unified builds if they have specified
      # models they want to test against (based on lab provisioning)
      if (board in _unified_board_names and
          not site_config[config_name]['models']):
        continue

      if config_name not in config_default_reset:
        site_config[config_name]['hw_tests'] = default_tests[:]
        config_default_reset.add(config_name)

      site_config[config_name]['hw_tests'] += [suite]

  #
  # Paladins with alternative configs.
  #
  site_config.AddForBoards(
      'nowithdebug-paladin',
      ['amd64-generic'],
      board_configs,
      site_config.templates.paladin,
      site_config.templates.internal_nowithdebug_paladin,
  )

  master_config.AddSlaves([
      site_config.Add(
          'chell-nowithdebug-paladin',
          site_config.templates.paladin,
          site_config.templates.internal_nowithdebug_paladin,
          site_config.templates.cq_luci_slave,
          boards=['chell'],
      ),

      site_config.Add(
          'incremental-paladin',
          site_config.templates.paladin,
          site_config.templates.internal_paladin,
          site_config.templates.no_hwtest_builder,
          site_config.templates.cq_luci_slave,
          boards=['reef'],
          build_before_patching=True,
          compilecheck=True,
          unittests=False,
          important=False, # crbug.com/984006
      ),
  ])

  # Used for builders which build completely from source except Chrome.
  # These boards pass with -clang-clean CFLAG, so ensure they stay that way.
  master_config.AddSlaves(
      site_config.AddForBoards(
          'full-compile-paladin',
          ['nyan_kitty'],
          board_configs,
          site_config.templates.paladin,
          site_config.templates.no_hwtest_builder,
          site_config.templates.cq_luci_slave,
          board_replace=True,
          chrome_sdk=False,
          compilecheck=True,
          cpe_export=False,
          debug_symbols=False,
          unittests=False,
          upload_hw_test_artifacts=False,
          vm_tests=[],
      )
  )

  master_config.AddSlaves(
      site_config.AddForBoards(
          'unittest-only-paladin',
          list(_paladin_separate_unittest_phase),
          board_configs,
          site_config.templates.unittest_only_paladin,
          site_config.templates.cq_luci_slave,
          unittests=True,
      )
  )
  site_config.Add(
      'amd64-generic-asan-paladin',
      site_config.templates.paladin,
      site_config.templates.no_hwtest_builder,
      board_configs['amd64-generic'],
      site_config.templates.asan,
      description='Paladin build with Address Sanitizer (Clang)',
  )

  site_config.Add(
      'betty-asan-paladin',
      site_config.templates.paladin,
      site_config.templates.no_hwtest_builder,
      board_configs['betty'],
      site_config.templates.asan,
      description='Paladin build with Address Sanitizer (Clang)',
  )

  site_config.Add(
      'amd64-generic-ubsan-paladin',
      site_config.templates.paladin,
      site_config.templates.no_hwtest_builder,
      board_configs['amd64-generic'],
      site_config.templates.ubsan,
      description='Paladin build with Undefined Behavior Sanitizer (Clang)',
  )


def IncrementalBuilders(site_config, boards_dict, ge_build_config):
  """Create all incremental build configs.

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
    boards_dict: A dict mapping board types to board name collections.
    ge_build_config: Dictionary containing the decoded GE configuration file.
  """
  board_configs = CreateInternalBoardConfigs(
      site_config, boards_dict, ge_build_config)

  site_config.AddTemplate(
      'incremental_affinity',
      build_affinity=True,
      luci_builder=config_lib.LUCI_BUILDER_INCREMENTAL,
  )

  master_config = site_config.Add(
      'master-incremental',
      site_config.templates.incremental,
      site_config.templates.internal_incremental,
      boards=[],
      master=True,
      manifest_version=True,
      slave_configs=[],
      schedule='with 10m interval',
  )

  # Build external source, for an internal board.
  master_config.AddSlave(
      site_config.Add(
          'amd64-generic-incremental',
          site_config.templates.incremental,
          site_config.templates.incremental_affinity,
          board_configs['amd64-generic'],
          manifest_version=True,
      )
  )

  master_config.AddSlave(
      site_config.Add(
          'betty-incremental',
          site_config.templates.incremental,
          site_config.templates.internal_incremental,
          site_config.templates.incremental_affinity,
          boards=['betty'],
          manifest_version=True,
      )
  )

  master_config.AddSlave(
      site_config.Add(
          'chell-incremental',
          site_config.templates.incremental,
          site_config.templates.internal_incremental,
          site_config.templates.incremental_affinity,
          boards=['chell'],
          manifest_version=True,
      )
  )

  master_config.AddSlave(
      site_config.Add(
          'lakitu-incremental',
          site_config.templates.incremental,
          site_config.templates.internal_incremental,
          site_config.templates.incremental_affinity,
          site_config.templates.lakitu_notification_emails,
          board_configs['lakitu'],
          manifest_version=True,
      )
  )

  #
  # Available, but not regularly scheduled.
  #
  site_config.Add(
      'x32-generic-incremental',
      site_config.templates.incremental,
      board_configs['x32-generic'],
  )

  site_config.Add(
      'beaglebone-incremental',
      site_config.templates.incremental,
      site_config.templates.beaglebone,
      boards=['beaglebone'],
      description='Incremental Beaglebone Builder',
  )

  site_config.Add(
      'kumo-incremental',
      site_config.templates.incremental,
      site_config.templates.internal_incremental,
      site_config.templates.lakitu_notification_emails,
      board_configs['kumo'],
  )

  site_config.Add(
      'lakitu-gpu-incremental',
      site_config.templates.incremental,
      site_config.templates.internal_incremental,
      site_config.templates.lakitu_notification_emails,
      board_configs['lakitu-gpu'],
  )

  site_config.Add(
      'lakitu-st-incremental',
      site_config.templates.incremental,
      site_config.templates.internal_incremental,
      site_config.templates.lakitu_notification_emails,
      board_configs['lakitu-st'],
  )

  site_config.Add(
      'lakitu_next-incremental',
      site_config.templates.incremental,
      site_config.templates.internal_incremental,
      site_config.templates.lakitu_notification_emails,
      board_configs['lakitu_next'],
  )


def ReleaseAfdoBuilders(site_config, boards_dict, ge_build_config):
  """Create AFDO Performance tryjobs.

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
    boards_dict: A dict mapping board types to board name collections.
    ge_build_config: Dictionary containing the decoded GE configuration file.
  """
  board_configs = CreateInternalBoardConfigs(
      site_config, boards_dict, ge_build_config)

  # Now generate generic release-afdo configs if we haven't created anything
  # more specific above already. release-afdo configs are builders that do AFDO
  # profile collection and optimization in the same builder. Used by developers
  # that want to measure performance changes caused by their changes.
  for board in boards_dict['all_release_boards']:
    base = board_configs[board]

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
            generate_config_name,
            site_config.templates.release_afdo_generate,
            base
        ),
        site_config.Add(
            use_config_name,
            site_config.templates.release_afdo_use,
            base
        ),
    )


def InformationalBuilders(site_config, boards_dict, ge_build_config):
  """Create all informational builders.

  We have a number of informational builders that are built, but whose output is
  not directly used for anything other than reporting success or failure.

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
    boards_dict: A dict mapping board types to board name collections.
    ge_build_config: Dictionary containing the decoded GE configuration file.
  """
  external_board_configs = CreateBoardConfigs(
      site_config, boards_dict, ge_build_config)
  internal_board_configs = CreateInternalBoardConfigs(
      site_config, boards_dict, ge_build_config)

  _chrome_boards = frozenset(
      board for board, config in internal_board_configs.items()
      if config.get('sync_chrome', True))


  hw_test_list = HWTestList(ge_build_config)

  _chrome_informational_hwtest_boards = frozenset([
      'caroline',
      'eve',
      'reks',
      'veyron_minnie',
  ])

  _chrome_informational_swarming_boards = frozenset([
      'eve',
  ])

  # We have to mark all autogenerated PFQs as not important so the master
  # does not wait for them.  https://crbug.com/386214
  # If you want an important PFQ, you'll have to declare it yourself.

  informational_boards = (
      (boards_dict['all_release_boards'] & _chrome_boards))

  _tot_chrome_pfq_informational_board_configs = UpdateBoardConfigs(
      internal_board_configs,
      _chrome_informational_hwtest_boards,
      hw_tests=hw_test_list.DefaultListChromePFQInformational(
          pool=constants.HWTEST_CONTINUOUS_POOL))

  _tot_chrome_pfq_informational_board_configs = UpdateBoardConfigs(
      _tot_chrome_pfq_informational_board_configs,
      _chrome_informational_swarming_boards)

  site_config.AddForBoards(
      'tot-chrome-pfq-informational',
      informational_boards,
      _tot_chrome_pfq_informational_board_configs,
      site_config.templates.chrome_pfq_informational)

  site_config.Add(
      'amd64-generic-asan',
      site_config.templates.asan,
      site_config.templates.incremental,
      site_config.templates.no_hwtest_builder,
      site_config.templates.informational,
      boards=['amd64-generic'],
      description='Build with Address Sanitizer (Clang)',
      # THESE IMAGES CAN DAMAGE THE LAB and cannot be used for hardware testing.
      disk_layout='4gb-rootfs',
      # Every 3 hours.
      schedule='0 */3 * * *',
      board_replace=True,
  )

  site_config.Add(
      'amd64-generic-tot-asan-informational',
      site_config.templates.tot_asan_informational,
      site_config.templates.no_hwtest_builder,
      build_type=constants.CHROME_PFQ_TYPE,
      boards=['amd64-generic'],
      schedule='with 30m interval',
  )

  site_config.Add(
      'betty-asan',
      site_config.templates.asan,
      site_config.templates.incremental,
      site_config.templates.no_hwtest_builder,
      site_config.templates.internal,
      site_config.templates.informational,
      boards=['betty'],
      description='Build with Address Sanitizer (Clang)',
      # Once every day. 3 PM UTC is 7 AM PST (no daylight savings).
      schedule='0 15 * * *',
      board_replace=True,
  )

  site_config.Add(
      'betty-tot-asan-informational',
      site_config.templates.tot_asan_informational,
      site_config.templates.no_hwtest_builder,
      build_type=constants.CHROME_PFQ_TYPE,
      boards=['betty'],
  )

  site_config.Add(
      'amd64-generic-fuzzer',
      site_config.templates.fuzzer,
      boards=['amd64-generic'],
      description='Build for fuzzing testing',
      # THESE IMAGES CAN DAMAGE THE LAB and cannot be used for hardware testing.
      disk_layout='4gb-rootfs',
      image_test=None,
      # Every 3 hours.
      schedule='0 */3 * * *',
      board_replace=True,
  )

  site_config.Add(
      'amd64-generic-coverage-fuzzer',
      site_config.templates.fuzzer,
      boards=['amd64-generic'],
      profile='coverage-fuzzer',
      description='Build for fuzzing coverage testing',
      gs_path='gs://chromeos-fuzzing-artifacts/libfuzzer-coverage',
      disk_layout='4gb-rootfs',
      image_test=None,
      # Every 3 hours.
      schedule='0 */3 * * *',
      board_replace=True,
  )

  site_config.Add(
      'amd64-generic-msan-fuzzer',
      site_config.templates.fuzzer,
      boards=['amd64-generic'],
      profile='msan-fuzzer',
      description='Build for msan fuzzing testing',
      gs_path='gs://chromeos-fuzzing-artifacts/libfuzzer-msan',
      disk_layout='4gb-rootfs',
      image_test=None,
      # Every 3 hours.
      schedule='0 */3 * * *',
      board_replace=True,
  )

  site_config.Add(
      'amd64-generic-ubsan',
      site_config.templates.ubsan,
      site_config.templates.incremental,
      site_config.templates.no_hwtest_builder,
      site_config.templates.informational,
      boards=['amd64-generic'],
      description='Build with Undefined Behavior Sanitizer (Clang)',
      # THESE IMAGES CAN DAMAGE THE LAB and cannot be used for hardware testing.
      disk_layout='16gb-rootfs',
      # Every 3 hours.
      schedule='0 */3 * * *',
      board_replace=True,
  )

  site_config.Add(
      'amd64-generic-ubsan-fuzzer',
      site_config.templates.fuzzer,
      boards=['amd64-generic'],
      profile='ubsan-fuzzer',
      description='Build for fuzzing testing',
      gs_path='gs://chromeos-fuzzing-artifacts/libfuzzer-ubsan',
      disk_layout='4gb-rootfs',
      image_test=None,
      # Every 3 hours.
      schedule='0 */3 * * *',
      board_replace=True,
  )

  site_config.Add(
      'amd64-generic-goma-canary-chromium-pfq-informational',
      site_config.templates.chromium_pfq_informational,
      site_config.templates.no_hwtest_builder,
      site_config.templates.no_vmtest_builder,
      description='Test canary versions of goma.',
      boards=[
          'amd64-generic',
      ],
      schedule='with 30m interval',
      goma_client_type='candidate',
  )

  _chrome_perf_boards = frozenset([
      'reef',
  ])

  site_config.AddForBoards(
      'chrome-perf',
      _chrome_perf_boards,
      internal_board_configs,
      site_config.templates.chrome_perf,
  )

  _tot_chromium_pfq_informational_swarming_boards = frozenset([
      'amd64-generic',
  ])

  _tot_chromium_pfq_informational_board_configs = UpdateBoardConfigs(
      external_board_configs,
      _tot_chromium_pfq_informational_swarming_boards)

  site_config.AddForBoards(
      'tot-chromium-pfq-informational',
      (boards_dict['all_full_boards'] & _chrome_boards),
      _tot_chromium_pfq_informational_board_configs,
      site_config.templates.chromium_pfq_informational,
      site_config.templates.build_external_chrome,
      internal=False,
      manifest_repo_url=config_lib.GetSiteParams().MANIFEST_URL,
      overlays=constants.PUBLIC_OVERLAYS,
  )

  site_config.ApplyForBoards(
      'tot-chromium-pfq-informational',
      ['amd64-generic'],
      schedule='with 30m interval',
  )

  site_config.ApplyForBoards(
      'tot-chrome-pfq-informational',
      [
          'caroline',
          'eve',
          'veyron_minnie',
          'kevin-arcnext',
      ],
      schedule='with 30m interval',
  )

  _telemetry_boards = frozenset([
      'amd64-generic',
      'arm-generic',
      'betty',
      'betty-arcnext',
      'betty-pi-arc',
  ])

  site_config.AddForBoards(
      'telemetry',
      _telemetry_boards,
      internal_board_configs,
      site_config.templates.telemetry,
  )

  site_config['amd64-generic-telemetry'].apply(
      schedule='with 30m interval',
  )


def ChromePfqBuilders(site_config, boards_dict, ge_build_config):
  """Create all Chrome PFQ build configs.

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
    boards_dict: A dict mapping board types to board name collections.
    ge_build_config: Dictionary containing the decoded GE configuration file.
  """
  external_board_configs = CreateBoardConfigs(
      site_config, boards_dict, ge_build_config)
  internal_board_configs = CreateInternalBoardConfigs(
      site_config, boards_dict, ge_build_config)

  _chrome_boards = frozenset(
      board for board, config in internal_board_configs.items()
      if config.get('sync_chrome', True))

  _chromium_pfq_important_boards = frozenset([
      'amd64-generic',
      'arm-generic',
      'arm64-generic',
      'scarlet',
      'veyron_jerry',
  ])

  _chromium_pfq_experimental_boards = frozenset([
  ])

  master_config = site_config.Add(
      'master-chromium-pfq',
      site_config.templates.chromium_pfq,
      boards=[],
      master=True,
      slave_configs=[],
      binhost_test=True,
      push_overlays=constants.BOTH_OVERLAYS,
      afdo_update_ebuild=True,
      chrome_sdk=False,
      health_alert_recipients=['chromeos-infra-eng@grotations.appspotmail.com',
                               'chrome'],
      schedule='triggered',
      triggered_gitiles=[[
          'https://chromium.googlesource.com/chromium/src',
          ['regexp:refs/tags/[^/]+']
      ]],
  )

  # Create important configs, then non-important configs.
  master_config.AddSlaves(
      site_config.AddForBoards(
          'chromium-pfq',
          _chromium_pfq_important_boards,
          external_board_configs,
          site_config.templates.chromium_pfq,
          site_config.templates.build_external_chrome,
      )
  )
  # non-important configs.
  master_config.AddSlaves(
      site_config.AddForBoards(
          'chromium-pfq',
          _chromium_pfq_experimental_boards,
          external_board_configs,
          site_config.templates.chromium_pfq,
          site_config.templates.build_external_chrome,
          important=False,
      )
  )
  site_config.AddForBoards(
      'chromium-pfq',
      ((boards_dict['all_full_boards'] & _chrome_boards) -
       (_chromium_pfq_important_boards | _chromium_pfq_experimental_boards)),
      external_board_configs,
      site_config.templates.chromium_pfq,
      site_config.templates.build_external_chrome,
  )

  _chrome_pfq_important_boards = frozenset([
      'atlas',
      'betty',
      'betty-arcnext',
      'betty-pi-arc',
      'bob',
      'caroline',
      'chell',
      'coral',
      'cyan',
      'eve',
      'eve-arcnext',
      'grunt',
      'hana',
      'kevin64',
      'nocturne',
      'nyan_kitty',
      'reef',
      'terra',
      'veyron_minnie',
      'veyron_rialto',
  ])

  _chrome_pfq_experimental_boards = frozenset([
  ])

  _chrome_pfq_skylab_boards = frozenset([
      'caroline',
      'kevin-arcnext',
      'kevin64',
      'grunt',
      'reef',
      'veyron_minnie',
  ])

  _chrome_pfq_tryjob_boards = (
      (boards_dict['all_release_boards'] & _chrome_boards) -
      (_chrome_pfq_important_boards | _chrome_pfq_experimental_boards)
  )

  master_config.AddSlaves(
      site_config.AddForBoards(
          'chrome-pfq',
          _chrome_pfq_important_boards - _chrome_pfq_skylab_boards,
          internal_board_configs,
          site_config.templates.chrome_pfq,
      )
  )
  master_config.AddSlaves(
      site_config.AddForBoards(
          'chrome-pfq',
          _chrome_pfq_experimental_boards - _chrome_pfq_skylab_boards,
          internal_board_configs,
          site_config.templates.chrome_pfq,
          important=False,
      )
  )
  master_config.AddSlaves(
      site_config.AddForBoards(
          'chrome-pfq',
          _chrome_pfq_skylab_boards & _chrome_pfq_important_boards,
          internal_board_configs,
          site_config.templates.chrome_pfq,
          enable_skylab_hw_tests=True,
      )
  )

  master_config.AddSlaves(
      site_config.AddForBoards(
          'chrome-pfq',
          _chrome_pfq_skylab_boards & _chrome_pfq_experimental_boards,
          internal_board_configs,
          site_config.templates.chrome_pfq,
          enable_skylab_hw_tests=True,
          important=False,
      )
  )

  # Define the result of the build configs for tryjob purposes.
  site_config.AddForBoards(
      'chrome-pfq',
      _chrome_pfq_tryjob_boards,
      internal_board_configs,
      site_config.templates.chrome_pfq,
  )


def FirmwareBuilders(site_config, _boards_dict, _ge_build_config):
  """Create all firmware build configs.

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
    boards_dict: A dict mapping board types to board name collections.
    ge_build_config: Dictionary containing the decoded GE configuration file.
  """
  # Defines "interval", "branch", "boards" for firmwarebranch builds.
  #
  # Intervals:
  # NONE = ''  # Do not schedule automatically.
  ACTIVE = 'with 168h interval'  # 1 week interval
  INACTIVE = 'with 720h interval'  # 30 day interval
  firmware_branch_builders = [
      (INACTIVE, 'firmware-monroe-4921.B', ['monroe']),
      (INACTIVE, 'firmware-enguarde-5216.201.B', ['enguarde']),
      (INACTIVE, 'firmware-expresso-5216.223.B', ['expresso']),
      (INACTIVE, 'firmware-kip-5216.227.B', ['kip']),
      (INACTIVE, 'firmware-swanky-5216.238.B', ['swanky']),
      (INACTIVE, 'firmware-gnawty-5216.239.B', ['gnawty']),
      (INACTIVE, 'firmware-winky-5216.265.B', ['winky']),
      (INACTIVE, 'firmware-candy-5216.310.B', ['candy']),
      (INACTIVE, 'firmware-banjo-5216.334.B', ['banjo']),
      (INACTIVE, 'firmware-orco-5216.362.B', ['orco']),
      (INACTIVE, 'firmware-sumo-5216.382.B', ['sumo']),
      (INACTIVE, 'firmware-ninja-5216.383.B', ['ninja']),
      (INACTIVE, 'firmware-heli-5216.392.B', ['heli']),
      (INACTIVE, 'firmware-nyan-5771.B', ['nyan_big', 'nyan_blaze']),
      (INACTIVE, 'firmware-kitty-5771.61.B', ['nyan_kitty']),
      (INACTIVE, 'firmware-samus-6300.B', ['samus']),
      (INACTIVE, 'firmware-auron-6301.B', ['jecht']),
      (INACTIVE, 'firmware-paine-6301.58.B', ['auron_paine']),
      (INACTIVE, 'firmware-yuna-6301.59.B', ['auron_yuna']),
      (INACTIVE, 'firmware-guado-6301.108.B', ['guado']),
      (INACTIVE, 'firmware-tidus-6301.109.B', ['tidus']),
      (INACTIVE, 'firmware-rikku-6301.110.B', ['rikku']),
      (INACTIVE, 'firmware-lulu-6301.136.B', ['lulu']),
      (INACTIVE, 'firmware-gandof-6301.155.B', ['gandof']),
      (INACTIVE, 'firmware-buddy-6301.202.B', ['buddy']),
      (INACTIVE, 'firmware-veyron-6588.B', [
          'veyron_jerry', 'veyron_mighty',
          'veyron_speedy', 'veyron_jaq',
          'veyron_minnie',
          'veyron_mickey', 'veyron_rialto', 'veyron_tiger',
          'veyron_fievel']),
      (INACTIVE, 'firmware-glados-7820.B', [
          'glados', 'chell', 'lars',
          'sentry', 'cave', 'asuka', 'caroline']),
      (INACTIVE, 'firmware-strago-7287.B', [
          'wizpig', 'setzer', 'banon', 'kefka', 'relm']),
      (INACTIVE, 'firmware-cyan-7287.57.B', ['cyan']),
      (INACTIVE, 'firmware-celes-7287.92.B', ['celes']),
      (INACTIVE, 'firmware-ultima-7287.131.B', ['ultima']),
      (INACTIVE, 'firmware-reks-7287.133.B', ['reks']),
      (INACTIVE, 'firmware-terra-7287.154.B', ['terra']),
      (INACTIVE, 'firmware-edgar-7287.167.B', ['edgar']),
      (INACTIVE, 'firmware-smaug-7900.B', ['smaug']),
      (INACTIVE, 'firmware-gale-8281.B', ['gale']),
      (INACTIVE, 'firmware-oak-8438.B', ['oak', 'elm', 'hana']),
      (INACTIVE, 'firmware-gru-8785.B', ['gru', 'kevin', 'bob']),
      (INACTIVE, 'firmware-reef-9042.B', ['reef', 'pyro', 'sand', 'snappy']),
      (INACTIVE, 'firmware-cr50-9308.B', ['reef']),
      (INACTIVE, 'firmware-cr50-9308.24.B', ['reef']),
      (INACTIVE, 'firmware-cr50-release-9308.25.B', ['reef']),
      (INACTIVE, 'firmware-cr50-mp-release-9308.87.B', ['reef']),
      (INACTIVE, 'firmware-eve-9584.B', ['eve']),
      (INACTIVE, 'firmware-coral-10068.B', ['coral']),
      (INACTIVE, 'firmware-fizz-10139.B', ['fizz']),
      (INACTIVE, 'firmware-fizz-10139.94.B', ['fizz']),
      (INACTIVE, 'firmware-scarlet-10388.B', ['scarlet']),
      (INACTIVE, 'firmware-poppy-10431.B', ['poppy', 'soraka', 'nautilus']),
      (INACTIVE, 'firmware-nami-10775.B', ['nami']),
      (INACTIVE, 'firmware-nocturne-10984.B', ['nocturne']),
      (INACTIVE, 'firmware-servo-11011.B', ['oak']),
      (INACTIVE, 'firmware-grunt-11031.B', ['grunt']),
      (INACTIVE, 'firmware-nami-10775.108.B', ['nami']),
      (ACTIVE, 'firmware-rammus-11275.B', ['rammus']),
      (ACTIVE, 'firmware-octopus-11297.B', ['octopus']),
      (ACTIVE, 'firmware-octopus-11297.83.B', ['octopus']),
      (ACTIVE, 'firmware-kalista-11343.B', ['kalista']),
      (ACTIVE, 'firmware-atlas-11827.B', ['atlas']),
      (ACTIVE, 'firmware-sarien-12200.B', ['sarien']),
      (ACTIVE, 'firmware-mistral-12422.B', ['mistral']),
  ]

  for interval, branch, boards in firmware_branch_builders:
    site_config.Add(
        '%s-firmwarebranch' % branch,
        site_config.templates.firmwarebranch,
        boards=boards,
        workspace_branch=branch,
        schedule=interval,
    )


def FactoryBuilders(site_config, _boards_dict, _ge_build_config):
  """Create all factory build configs.

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
    boards_dict: A dict mapping board types to board name collections.
    ge_build_config: Dictionary containing the decoded GE configuration file.
  """
  # Intervals:
  # None: Do not schedule automatically.
  ACTIVE = 'with 168h interval'  # 1 week interval
  INACTIVE = 'with 720h interval'  # 30 day interval
  branch_builders = [
      (INACTIVE, 'factory-rambi-5517.B', [
          'enguarde', 'expresso', 'kip', 'swanky', 'winky']),
      (INACTIVE, 'factory-nyan-5772.B', [
          'nyan_big', 'nyan_blaze', 'nyan_kitty']),
      (INACTIVE, 'factory-rambi-6420.B', [
          'enguarde', 'candy', 'banjo',
          'ninja', 'sumo', 'orco', 'heli', 'gnawty']),
      (INACTIVE, 'factory-auron-6459.B', [
          'auron_paine', 'auron_yuna', 'lulu',
          'jecht', 'gandof', 'buddy']),
      (INACTIVE, 'factory-whirlwind-6509.B', ['whirlwind']),
      (INACTIVE, 'factory-veyron-6591.B', [
          'veyron_jerry', 'veyron_mighty',
          'veyron_speedy', 'veyron_jaq',
          'veyron_minnie', 'veyron_mickey']),
      (INACTIVE, 'factory-auron-6772.B', [
          'jecht', 'guado', 'tidus', 'rikku', 'buddy']),
      (INACTIVE, 'factory-whirlwind-6812.41.B', ['whirlwind']),
      (INACTIVE, 'factory-strago-7458.B', [
          'cyan', 'celes', 'ultima', 'reks', 'terra', 'edgar',
          'wizpig', 'setzer', 'banon', 'kefka', 'relm', 'kip']),
      (INACTIVE, 'factory-veyron-7505.B', [
          'veyron_mickey', 'veyron_tiger', 'veyron_fievel', 'veyron_rialto']),
      (INACTIVE, 'factory-glados-7657.B', ['glados', 'chell']),
      (INACTIVE, 'factory-glados-7828.B', [
          'glados', 'chell', 'lars',
          'sentry', 'cave', 'asuka', 'caroline']),
      (INACTIVE, 'factory-oak-8182.B', ['elm', 'hana']),
      (INACTIVE, 'factory-gru-8652.B', ['kevin']),
      (INACTIVE, 'factory-gale-8743.19.B', ['gale']),
      (INACTIVE, 'factory-reef-8811.B', ['reef', 'pyro', 'sand', 'snappy']),
      (INACTIVE, 'factory-gru-9017.B', ['gru', 'bob']),
      (INACTIVE, 'factory-eve-9667.B', ['eve']),
      (INACTIVE, 'factory-coral-10122.B', ['coral']),
      (INACTIVE, 'factory-fizz-10167.B', ['fizz', 'fizz-accelerator']),
      (INACTIVE, 'factory-scarlet-10211.B', ['scarlet']),
      (INACTIVE, 'factory-soraka-10323.39.B', ['soraka']),
      (INACTIVE, 'factory-poppy-10504.B', ['nautilus']),
      (INACTIVE, 'factory-nami-10715.B', ['nami', 'kalista']),
      (INACTIVE, 'factory-nocturne-11066.B', ['nocturne']),
      (INACTIVE, 'factory-grunt-11164.B', ['grunt']),
      (INACTIVE, 'factory-rammus-11289.B', ['rammus']),
      (ACTIVE, 'factory-octopus-11512.B', ['octopus']),
      (ACTIVE, 'factory-atlas-11907.B', ['atlas']),
      (ACTIVE, 'factory-sarien-12033.B', ['sarien']),
      (ACTIVE, 'factory-mistral-12361.B', ['mistral']),
      # This is intended to create master branch tryjobs, NOT for production
      # builds. Update the associated list of boards as needed.
      (None, 'master', ['atlas', 'octopus', 'rammus', 'coral', 'eve',
                        'sarien', 'mistral']),
  ]

  _FACTORYBRANCH_TIMEOUT = 12 * 60 * 60

  # Requires that you set boards, and workspace_branch.
  site_config.AddTemplate(
      'factorybranch',
      site_config.templates.factory,
      site_config.templates.workspace,
      sign_types=['factory'],
      build_type=constants.GENERIC_TYPE,
      uprev=True,
      overlays=constants.BOTH_OVERLAYS,
      push_overlays=constants.BOTH_OVERLAYS,
      useflags=config_lib.append_useflags(['-cros-debug', 'chrome_internal']),
      builder_class_name='workspace_builders.FactoryBranchBuilder',
      build_timeout=_FACTORYBRANCH_TIMEOUT,
      description='TOT builder to build a firmware branch.',
      doc='https://goto.google.com/tot-for-firmware-branches',
  )

  site_config.AddTemplate(
      'old_factorybranch_packages',
      packages=[
          'virtual/target-os',
          'virtual/target-os-dev',
          'virtual/target-os-test',
          'chromeos-base/chromeos-installshim',
          'chromeos-base/chromeos-factory',
          'chromeos-base/chromeos-hwid',
          'chromeos-base/autotest-factory-install',
          'chromeos-base/autotest-all',
      ],
  )

  # These branches require a differnt list of packages to build.
  old_package_branches = {
      'factory-beltino-5140.14.B',
      'factory-rambi-5517.B',
      'factory-nyan-5772.B',
      'factory-rambi-6420.B',
      'factory-auron-6459.B',
      'factory-whirlwind-6509.B',
      'factory-veyron-6591.B',
      'factory-samus-6658.B',
      'factory-auron-6772.B',
      'factory-whirlwind-6812.41.B',
      'factory-arkham-7077.113.B',
      'factory-strago-7458.B',
      'factory-veyron-7505.B',
      'factory-glados-7657.B',
      'factory-glados-7828.B',
  }

  for active, branch, boards in branch_builders:
    schedule = {}
    if active:
      schedule = {
          'schedule': active,
      }

    # Define the buildspec builder for the branch.
    branch_master = site_config.Add(
        '%s-buildspec' % branch,
        site_config.templates.buildspec,
        display_label=config_lib.DISPLAY_LABEL_FACTORY,
        workspace_branch=branch,
        build_timeout=_FACTORYBRANCH_TIMEOUT,
        **schedule
    )

    # Define the per-board builders for the branch.
    for board in boards:
      child = site_config.Add(
          '%s-%s-factorybranch' % (board, branch),
          site_config.templates.factorybranch,
          boards=[board],
          workspace_branch=branch,
      )
      if branch in old_package_branches:
        child.apply(site_config.templates.old_factorybranch_packages)
      branch_master.AddSlave(child)


def ReleaseBuilders(site_config, boards_dict, ge_build_config):
  """Create all release builders.

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
    boards_dict: A dict mapping board types to board name collections.
    ge_build_config: Dictionary containing the decoded GE configuration file.
  """
  board_configs = CreateInternalBoardConfigs(
      site_config, boards_dict, ge_build_config)

  unified_builds = config_lib.GetUnifiedBuildConfigAllBuilds(ge_build_config)
  unified_board_names = set([b[config_lib.CONFIG_TEMPLATE_REFERENCE_BOARD_NAME]
                             for b in unified_builds])

  def _IsLakituConfig(config):
    return 'lakitu' in config['name']

  def _CreateMasterConfig(name):
    return site_config.Add(
        name,
        site_config.templates.release,
        boards=[],
        master=True,
        slave_configs=[],
        sync_chrome=False,
        chrome_sdk=False,
        afdo_use=False,
        # Because PST is 8 hours from UTC, these times are the same in both. But
        # daylight savings time is NOT adjusted for
        schedule='  0 2,10,18 * * *',
    )

  ### Master release configs.
  master_config = _CreateMasterConfig('master-release')
  # pylint: disable=unused-variable
  lakitu_master_config = _CreateMasterConfig('master-lakitu-release')

  def _AssignToMaster(config):
    """Add |config| as a slave config to the appropriate master config."""
    # Default to chromeos master release builder.
    master = master_config

    # Add this config to 'master-lakitu-release' instead if this is an LTS
    # branch for lakitu. This is typically only done on a branch after it is
    # out of ChromeOS support window.
    # To do this, set 'lakitu_lts_branch' to 'True' and re-run
    # 'config/chromeos_config_unittest --update'.
    lakitu_lts_branch = False
    if lakitu_lts_branch and _IsLakituConfig(config):
      master = lakitu_master_config

    master.AddSlave(config)

  ### Release configs.

  # Used for future bvt migration.
  _release_experimental_boards = frozenset([
  ])

  _release_enable_skylab_hwtest = frozenset([
      'asuka',
      'coral',
      'nyan_blaze',
      'reef',
  ])

  _release_enable_skylab_partial_boards = {
      'coral': ['astronaut', 'nasher', 'lava'],
  }

  _release_enable_skylab_cts_hwtest = frozenset([
      'samus',
      'terra',
  ])

  def _get_skylab_settings(board_name):
    """Get skylab settings for release builder.

    Args:
      board_name: A string board name.

    Returns:
      A dict mapping suite types to booleans indicating whether this suite on
        this board is to be run on Skylab. Current suite types:
        - cts: all suites using pool:cts,
        - default: the rest of the suites.
    """
    return {
        'cts': board_name in _release_enable_skylab_cts_hwtest,
        'default': board_name in _release_enable_skylab_hwtest,
    }

  builder_to_boards_dict = config_lib.GroupBoardsByBuilder(
      ge_build_config[config_lib.CONFIG_TEMPLATE_BOARDS])

  _all_release_builder_boards = builder_to_boards_dict[
      config_lib.CONFIG_TEMPLATE_RELEASE]

  site_config.AddForBoards(
      config_lib.CONFIG_TYPE_RELEASE,
      ((boards_dict['all_release_boards'] | _all_release_builder_boards)
       - unified_board_names),
      board_configs,
      site_config.templates.release,
  )

  hw_test_list = HWTestList(ge_build_config)

  for unibuild in config_lib.GetUnifiedBuildConfigAllBuilds(ge_build_config):
    models = []
    for model in unibuild[config_lib.CONFIG_TEMPLATE_MODELS]:
      name = model[config_lib.CONFIG_TEMPLATE_MODEL_NAME]
      lab_board_name = model[config_lib.CONFIG_TEMPLATE_MODEL_BOARD_NAME]
      enable_skylab = True
      if (lab_board_name in _release_enable_skylab_hwtest and
          lab_board_name in _release_enable_skylab_partial_boards and
          name not in _release_enable_skylab_partial_boards[lab_board_name]):
        enable_skylab = False

      if config_lib.CONFIG_TEMPLATE_MODEL_TEST_SUITES in model:
        test_suites = model[config_lib.CONFIG_TEMPLATE_MODEL_TEST_SUITES]
        if 'bvt-arc' in test_suites:
          # TODO(crbug.com/814793)
          # We're tying these test suites to bvt-arc because it's not worth
          # plumbing this all the way through the GE UI since that architecture
          # was never fully implemented and we shouldn't have tied to it in
          # the first place.
          # Once test planning is properly implemented, this will fall out.
          test_suites.append('arc-cts-qual')
          test_suites.append('arc-gts-qual')
        models.append(config_lib.ModelTestConfig(
            name,
            lab_board_name,
            test_suites,
            enable_skylab=enable_skylab))
      else:
        no_model_test_suites = []
        models.append(config_lib.ModelTestConfig(
            name, lab_board_name, no_model_test_suites,
            enable_skylab=enable_skylab))

    reference_board_name = unibuild[
        config_lib.CONFIG_TEMPLATE_REFERENCE_BOARD_NAME]

    pool = constants.HWTEST_MACH_POOL
    config_name = '%s-release' % reference_board_name

    # Move unibuild to skylab.
    important = not unibuild[config_lib.CONFIG_TEMPLATE_EXPERIMENTAL]
    if reference_board_name in _release_experimental_boards:
      important = False

    enable_skylab_hw_tests = _get_skylab_settings(reference_board_name)
    site_config.AddForBoards(
        config_lib.CONFIG_TYPE_RELEASE,
        [reference_board_name],
        board_configs,
        site_config.templates.release,
        models=models,
        important=important,
        enable_skylab_hw_tests=enable_skylab_hw_tests['default'],
        enable_skylab_cts_hw_tests=enable_skylab_hw_tests['cts'],
        hw_tests=(hw_test_list.SharedPoolCanary(pool=pool) +
                  hw_test_list.CtsGtsQualTests()),
    )

    _AssignToMaster(site_config[config_name])

  def GetReleaseConfigName(board):
    """Convert a board name into a release config name."""
    return '%s-release' % board

  def GetConfigName(builder, board):
    """Convert a board name into a config name."""
    if builder == config_lib.CONFIG_TEMPLATE_RELEASE:
      return GetReleaseConfigName(board)
    else:
      # Currently just support RELEASE builders
      raise Exception('Do not support other builders.')

  def _GetConfigValues(board):
    """Get and return config values from template and user definitions."""
    important = not board[config_lib.CONFIG_TEMPLATE_EXPERIMENTAL]
    if board['name'] in _release_experimental_boards:
      important = False

    enable_skylab_hw_tests = _get_skylab_settings(board['name'])

    # Move non-unibuild to skylab.
    config_values = {
        'important': important,
        'enable_skylab_hw_tests': enable_skylab_hw_tests['default'],
        'enable_skylab_cts_hw_tests': enable_skylab_hw_tests['cts'],
    }

    return config_values

  def _AdjustUngroupedReleaseConfigs(builder_ungrouped_dict):
    """Adjust for ungrouped release boards"""
    for builder in builder_ungrouped_dict:
      for board in builder_ungrouped_dict[builder]:
        config_name = GetConfigName(builder,
                                    board[config_lib.CONFIG_TEMPLATE_NAME])
        site_config[config_name].apply(
            _GetConfigValues(board),
        )
        _AssignToMaster(site_config[config_name])

  def _AdjustGroupedReleaseConfigs(builder_group_dict):
    """Adjust leader and follower configs for grouped boards"""
    for builder in builder_group_dict:
      for group in builder_group_dict[builder]:
        board_group = builder_group_dict[builder][group]

        # Leaders are built on baremetal builders and run all tests needed by
        # the related boards.
        for board in board_group.leader_boards:
          config_name = GetConfigName(builder,
                                      board[config_lib.CONFIG_TEMPLATE_NAME])
          site_config[config_name].apply(
              _GetConfigValues(board),
          )
          _AssignToMaster(site_config[config_name])

        # Followers are built on GCE instances, and turn off testing that breaks
        # on GCE. The missing tests run on the leader board.
        for board in board_group.follower_boards:
          config_name = GetConfigName(builder,
                                      board[config_lib.CONFIG_TEMPLATE_NAME])
          site_config[config_name].apply(
              _GetConfigValues(board),
              chrome_sdk_build_chrome=False,
              vm_tests=[],
          )
          _AssignToMaster(site_config[config_name])

  def _AdjustReleaseConfigs():
    """Adjust ungrouped and grouped release configs"""
    (builder_group_dict, builder_ungrouped_dict) = (
        config_lib.GroupBoardsByBuilderAndBoardGroup(
            ge_build_config[config_lib.CONFIG_TEMPLATE_BOARDS]))
    _AdjustUngroupedReleaseConfigs(builder_ungrouped_dict)
    _AdjustGroupedReleaseConfigs(builder_group_dict)

    for board in chromeos_boards.moblab_boards:
      config_name = GetReleaseConfigName(board)
      if config_name not in site_config:
        continue
      # If the board is in moblab_boards, use moblab_release template
      site_config[config_name].apply(
          site_config.templates.moblab_release,
          board_configs[board],
      )

  _AdjustReleaseConfigs()


def PayloadBuilders(site_config, boards_dict):
  """Create <board>-payloads configs for all payload generating boards.

  We create a config named 'board-payloads' for every board which has a
  config with 'paygen' True. The idea is that we have a build that generates
  payloads, we need to have a tryjob to re-attempt them on failure.
  """
  for board in boards_dict['all_release_boards']:
    if site_config['%s-release' % board].paygen:
      site_config.Add(
          '%s-payloads' % board,
          site_config.templates.payloads,
          boards=[board],
      )


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
      'amd64-generic-chromium-pfq': {
          'useflags': [],
      },

      # No hw tests for any betty builders.  See crbug/998427.
      'betty-release': {
          'hw_tests': [],
          'hw_tests_override': []
      },

      'betty-arcnext-release': {
          'hw_tests': [],
          'hw_tests_override': []
      },

      'betty-arcmaster-release': {
          'hw_tests': [],
          'hw_tests_override': []
      },

      'betty-pi-arc-release': {
          'hw_tests': [],
          'hw_tests_override': []
      },

      'betty-qt-arc-release': {
          'hw_tests': [],
          'hw_tests_override': []
      },

      # No hw_tests for arkham, whirlwind, gale, mistral.  See b/140317527.
      'arkham-release': {
          'hw_tests': [],
          'hw_tests_override': [],
      },

      'whirlwind-release': {
          'dev_installer_prebuilts': True,
          'hw_tests': [],
          'hw_tests_override': [],
      },

      'gale-release': {
          'dev_installer_prebuilts': True,
          'hw_tests': [],
          'hw_tests_override': [],
      },

      'mistral-release': {
          'dev_installer_prebuilts': True,
          'hw_tests': [],
          'hw_tests_override': [],
      },

      'lakitu-release': config_lib.BuildConfig().apply(
          site_config.templates.lakitu_notification_emails,
          sign_types=['base'],
      ),

      # This is the full build of open-source overlay.
      'lakitu-full': config_lib.BuildConfig().apply(
          site_config.templates.lakitu_notification_emails,
      ),

      'lakitu-gpu-release': config_lib.BuildConfig().apply(
          site_config.templates.lakitu_notification_emails,
          sign_types=['base'],
          paygen=False,
      ),

      'lakitu-nc-release': config_lib.BuildConfig().apply(
          site_config.templates.lakitu_nc_customizations,
          site_config.templates.lakitu_notification_emails,
          paygen=False,
      ),

      'lakitu-st-release': config_lib.BuildConfig().apply(
          site_config.templates.lakitu_notification_emails,
          sign_types=['base'],
          paygen=False,
      ),

      'kumo-release': config_lib.BuildConfig().apply(
          site_config.templates.lakitu_notification_emails,
          sign_types=['base'],
          paygen=False,
          vm_tests=[config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                            test_suite='smoke')],
          vm_tests_override=None,
      ),

      'lakitu_next-release': config_lib.BuildConfig().apply(
          site_config.templates.lakitu_notification_emails,
      ),

      # TODO(yshaul): find out if hwqual needs to go as well
      # TODO(yshaul): fix apply method to merge base and test
      'guado_labstation-release': {
          'hwqual': False,
          'images': ['base', 'test'],
          'paygen': False,
      },

      'fizz-labstation-release': {
          'hwqual': False,
          'images': ['base', 'test'],
          'paygen': False,
      },

      'scarlet-paladin': {
          'useflags': config_lib.append_useflags(['-chrome_internal']),
      },

      # For crbug.com/961920.
      'scarlet-compile-only-pre-cq': {
          'useflags': config_lib.append_useflags(['-chrome_internal']),
      },

      # For crbug.com/961920.
      'scarlet-pre-cq': {
          'useflags': config_lib.append_useflags(['-chrome_internal']),
      },

      # Run TestSimpleChromeWorkflow only on kevin64-release instead of
      # arm64-generic/kevin64-chrome-pfq/kevin64-full.
      'arm64-generic-chromium-pfq': {
          'chrome_sdk_build_chrome': False,
      },
      'arm64-generic-full': {
          'chrome_sdk_build_chrome': False,
      },
      'kevin64-chrome-pfq': {
          'chrome_sdk_build_chrome': False,
      },
      'kevin64-full': {
          'chrome_sdk_build_chrome': False,
      },
      'kevin64-release': {
          'chrome_sdk_build_chrome': True,
      },

      # Currently factory and firmware branches will be created after DVT stage
      # therefore we need signed factory shim or accessory_rwsig firmware from
      # ToT temporarily.
      #
      # After factory and firmware branches are created, the configuation of
      # this project should be removed.
      # --- start from here ---
      'cheza-release': {
          'sign_types': ['recovery', 'factory'],
      },

      'flapjack-release': {
          'sign_types': ['recovery', 'factory'],
      },

      'hatch-release': {
          'sign_types': ['recovery', 'factory'],
      },

      'jacuzzi-release': {
          'sign_types': ['recovery', 'factory'],
      },

      'kukui-release': {
          'sign_types': ['recovery', 'factory'],
      },

      'puff-release': {
          'sign_types': ['recovery', 'factory'],
      },

      'sarien-release': {
          'sign_types': ['recovery', 'factory'],
      },

      'volteer-release': {
          'sign_types': ['recovery', 'factory'],
      },

      'zork-release': {
          'sign_types': ['recovery', 'factory'],
      },

      # --- end from here ---
  }

  # Some boards in toolchain builder are not using the same configuration as
  # release builders. Configure it here since it's easier, for both
  # llvm-toolchain and llvm-next-toolchain builders.
  for board in ['lakitu', 'fizz-moblab', 'gale', 'mistral', 'whirlwind']:
    if board == 'lakitu':
      overwritten_configs[board+'-llvm-toolchain'] = {
          'vm_tests': [config_lib.VMTestConfig(constants.VM_SUITE_TEST_TYPE,
                                               test_suite='smoke')],
          'gce_tests': [config_lib.GCETestConfig(constants.GCE_SUITE_TEST_TYPE,
                                                 test_suite='gce-sanity'),
                        config_lib.GCETestConfig(constants.GCE_SUITE_TEST_TYPE,
                                                 test_suite='gce-smoke')]
      }
    elif board == 'fizz-moblab':
      overwritten_configs[board+'-llvm-toolchain'] = {
          'enable_skylab_hw_tests': False,
          'hw_tests': [
              config_lib.HWTestConfig(
                  constants.HWTEST_MOBLAB_QUICK_SUITE)
          ],
          'hw_tests_override': [
              config_lib.HWTestConfig(
                  constants.HWTEST_MOBLAB_QUICK_SUITE)
          ],
      }
    else: # This is the case for gale, mistral and whirlwind
      overwritten_configs[board+'-llvm-toolchain'] = {
          'hw_tests': [
              config_lib.HWTestConfig(
                  constants.HWTEST_JETSTREAM_COMMIT_SUITE,
                  pool=constants.HWTEST_QUOTA_POOL,
                  quota_account='toolchain'
              )
          ],
          'hw_tests_override': [
              config_lib.HWTestConfig(
                  constants.HWTEST_JETSTREAM_COMMIT_SUITE,
                  pool=constants.HWTEST_QUOTA_POOL,
                  quota_account='toolchain'
              )
          ]
      }

    # Use the same configuration for llvm-next
    overwritten_configs[board+'-llvm-next-toolchain'] = \
      overwritten_configs[board+'-llvm-toolchain']

  for config_name, overrides  in overwritten_configs.items():
    # TODO: Turn this assert into a unittest.
    # config = site_config[config_name]
    # for k, v in overrides.items():
    #   assert config[k] != v, ('Unnecessary override: %s: %s' %
    #                           (config_name, k))
    site_config[config_name].apply(**overrides)


def SpecialtyBuilders(site_config, boards_dict, ge_build_config):
  """Add a variety of specialized builders or tryjobs.

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
    boards_dict: A dict mapping board types to board name collections.
    ge_build_config: Dictionary containing the decoded GE configuration file.
  """
  board_configs = CreateInternalBoardConfigs(
      site_config, boards_dict, ge_build_config)
  hw_test_list = HWTestList(ge_build_config)

  site_config.AddWithoutTemplate(
      'success-build',
      site_config.templates.external,
      site_config.templates.no_hwtest_builder,
      site_config.templates.no_vmtest_builder,
      boards=[],
      display_label=config_lib.DISPLAY_LABEL_TRYJOB,
      luci_builder=config_lib.LUCI_BUILDER_TRY,
      builder_class_name='test_builders.SucessBuilder',
      description='Builder always passes as quickly as possible.',
  )

  # Used by cbuildbot/stages/sync_stages_unittest
  site_config.AddWithoutTemplate(
      'sync-test-cbuildbot',
      site_config.templates.no_hwtest_builder,
      site_config.templates.no_vmtest_builder,
      boards=[],
      display_label=config_lib.DISPLAY_LABEL_TRYJOB,
      luci_builder=config_lib.LUCI_BUILDER_COMMITQUEUE,
      builder_class_name='test_builders.SucessBuilder',
      description='Used by sync_stages_unittest.',
  )

  site_config.AddWithoutTemplate(
      'fail-build',
      site_config.templates.external,
      site_config.templates.no_hwtest_builder,
      site_config.templates.no_vmtest_builder,
      boards=[],
      display_label=config_lib.DISPLAY_LABEL_TRYJOB,
      luci_builder=config_lib.LUCI_BUILDER_TRY,
      builder_class_name='test_builders.FailBuilder',
      description='Builder always fails as quickly as possible.',
  )

  site_config.AddWithoutTemplate(
      'chromiumos-sdk',
      site_config.templates.full,
      site_config.templates.no_hwtest_builder,
      # The amd64-host has to be last as that is when the toolchains
      # are bundled up for inclusion in the sdk.
      boards=[
          'arm-generic', 'amd64-generic'
      ],
      display_label=config_lib.DISPLAY_LABEL_UTILITY,
      build_type=constants.CHROOT_BUILDER_TYPE,
      builder_class_name='sdk_builders.ChrootSdkBuilder',
      use_sdk=False,
      prebuilts=constants.PUBLIC,
      build_timeout=18 * 60 * 60,
      description='Build the SDK and all the cross-compilers',
      doc='https://dev.chromium.org/chromium-os/build/builder-overview#'
          'TOC-Continuous',
      schedule='with 30m interval',
  )

  site_config.AddWithoutTemplate(
      'config-updater',
      site_config.templates.internal,
      site_config.templates.no_hwtest_builder,
      site_config.templates.no_vmtest_builder,
      site_config.templates.infra_builder,
      display_label=config_lib.DISPLAY_LABEL_UTILITY,
      description=('Build Config Updater reads updated GE config files from'
                   ' GS, and commits them to chromite after running tests.'),
      build_type=constants.GENERIC_TYPE,
      boards=[],
      builder_class_name='config_builders.UpdateConfigBuilder',
      binhost_test=True,
      schedule='@hourly',
  )

  site_config.AddWithoutTemplate(
      'luci-scheduler-updater',
      site_config.templates.internal,
      site_config.templates.no_hwtest_builder,
      site_config.templates.no_vmtest_builder,
      site_config.templates.infra_builder,
      display_label=config_lib.DISPLAY_LABEL_UTILITY,
      description=('Deploy changes to luci_scheduler.cfg.'),
      build_type=constants.GENERIC_TYPE,
      boards=[],
      builder_class_name='config_builders.LuciSchedulerBuilder',
      binhost_test=True,
      schedule='triggered',
      triggered_gitiles=[[
          'https://chromium.googlesource.com/chromiumos/chromite',
          ['refs/heads/master']
      ]],
  )

  site_config.Add(
      'betty-vmtest-informational',
      site_config.templates.informational,
      site_config.templates.internal,
      site_config.templates.no_hwtest_builder,
      description='VMTest Informational Builder for running long run tests.',
      build_type=constants.GENERIC_TYPE,
      boards=['betty'],
      builder_class_name='test_builders.VMInformationalBuilder',
      vm_tests=getInfoVMTest(),
      vm_tests_override=getInfoVMTest(),
      vm_test_report_to_dashboards=True,
      # 3 PM UTC is 7 AM PST (no daylight savings).
      schedule='0 15 * * *',
  )

  # Create our unittest stress build configs (used for tryjobs only)
  site_config.AddForBoards(
      'unittest-stress',
      boards_dict['all_boards'],
      board_configs,
      site_config.templates.unittest_stress,
      luci_builder=config_lib.LUCI_BUILDER_TRY,
      unittests=True,
  )

  site_config.AddGroup(
      'test-ap-group',
      site_config.Add('whirlwind-test-ap',
                      site_config.templates.test_ap,
                      boards=['whirlwind']),
      site_config.Add('gale-test-ap',
                      site_config.templates.test_ap,
                      boards=['gale']),
      description='Create images used to power access points in WiFi lab.',
  )

  # *-pre-flight-branch builders are in chromeos_release waterfall.
  site_config.Add(
      'samus-chrome-pre-flight-branch',
      site_config.templates.pre_flight_branch,
      display_label=config_lib.DISPLAY_LABEL_CHROME_PFQ,
      boards=['samus'],
      afdo_generate=True,
      afdo_update_ebuild=True,
      sync_chrome=True,
      chrome_rev=constants.CHROME_REV_STICKY,
      hw_tests=[hw_test_list.AFDORecordTest()],
      useflags=config_lib.append_useflags(['-transparent_hugepage',
                                           '-debug_fission',
                                           '-thinlto',
                                           '-cfi']),
      prebuilts=False,
      archive_build_debug=True,
  )

  site_config.Add(
      'chell-chrome-pre-flight-branch',
      site_config.templates.pre_flight_branch,
      display_label=config_lib.DISPLAY_LABEL_CHROME_PFQ,
      boards=['chell'],
      afdo_generate=True,
      afdo_update_ebuild=True,
      sync_chrome=True,
      chrome_rev=constants.CHROME_REV_STICKY,
      hw_tests=[hw_test_list.AFDORecordTest()],
      useflags=config_lib.append_useflags(['-transparent_hugepage',
                                           '-debug_fission',
                                           '-thinlto',
                                           '-cfi']),
      prebuilts=False,
      archive_build_debug=True,
  )

  site_config.Add(
      'gandof-android-nyc-pre-flight-branch',
      site_config.templates.pre_flight_branch,
      display_label=config_lib.DISPLAY_LABEL_NYC_ANDROID_PFQ,
      boards=['gandof'],
      sync_chrome=True,
      android_rev=constants.ANDROID_REV_LATEST,
      android_package='android-container-nyc',
      android_import_branch=constants.ANDROID_NYC_BUILD_BRANCH,
      prebuilts=False,
  )

  site_config.Add(
      'grunt-android-pi-pre-flight-branch',
      site_config.templates.pre_flight_branch,
      display_label=config_lib.DISPLAY_LABEL_PI_ANDROID_PFQ,
      boards=['grunt'],
      sync_chrome=True,
      android_rev=constants.ANDROID_REV_LATEST,
      android_package='android-container-pi',
      android_import_branch=constants.ANDROID_PI_BUILD_BRANCH,
      prebuilts=False,
      unittests=False,
  )

  site_config.AddWithoutTemplate(
      'chromeos-infra-go',
      site_config.templates.no_hwtest_builder,
      site_config.templates.no_unittest_builder,
      site_config.templates.no_vmtest_builder,
      site_config.templates.infra_builder,
      boards=[],
      display_label=config_lib.DISPLAY_LABEL_UTILITY,
      build_type=constants.GENERIC_TYPE,
      builder_class_name='infra_builders.InfraGoBuilder',
      use_sdk=True,
      prebuilts=constants.PUBLIC,
      build_timeout=60 * 60,
      description='Build Chromium OS infra Go binaries',
      doc='https://goto.google.com/cros-infra-go-packaging',
      schedule='triggered',
      triggered_gitiles=[[
          'https://chromium.googlesource.com/chromiumos/infra/lucifer',
          ['refs/heads/master']
      ], [
          'https://chromium.googlesource.com/chromiumos/infra/skylab_inventory',
          ['refs/heads/master'],
      ]],
  )


def TryjobMirrors(site_config):
  """Create tryjob specialized variants of every build config.

  This creates a new 'tryjob' config for every existing config, unless the
  existing config is already a tryjob config.

  Args:
    site_config: config_lib.SiteConfig to be modified by adding templates
                 and configs.
  """
  # Tryjobs which are unsafe to talk to Prod CIDB.
  cidb_unsafe = frozenset(['pre-cq-launcher'])

  tryjob_configs = {}

  for build_name, config in site_config.items():
    # Don't mirror builds that are already tryjob safe.
    if config_lib.isTryjobConfig(config):
      config.apply(hw_tests_override=None, vm_tests_override=None)
      continue

    tryjob_name = build_name + '-tryjob'

    # Don't overwrite mirrored versions that were explicitly created earlier.
    if tryjob_name in site_config:
      continue

    tryjob_config = copy.deepcopy(config)
    tryjob_config.apply(
        name=tryjob_name,
        display_label=config_lib.DISPLAY_LABEL_TRYJOB,
        luci_builder=config_lib.LUCI_BUILDER_TRY,
        # Generally make tryjobs safer.
        chroot_replace=True,
        debug=True,
        push_image=False,
        # Force uprev. This is so patched in changes are always built.
        uprev=True,
        gs_path=config_lib.GS_PATH_DEFAULT,
        schedule=None,
        suite_scheduling=False,
        triggered_gitiles=None,
        important=True,
        build_affinity=False,
    )

    # Force uprev. This is so patched in changes are always built.
    if tryjob_config.internal:
      tryjob_config.apply(overlays=constants.BOTH_OVERLAYS)

    # In trybots, we want to always run VM tests and all unit tests, so that
    # developers will get better testing for their changes.
    if tryjob_config.hw_tests_override is not None:
      tryjob_config.apply(hw_tests=tryjob_config.hw_tests_override,
                          hw_tests_override=None)

    if tryjob_config.vm_tests_override is not None:
      tryjob_config.apply(vm_tests=tryjob_config.vm_tests_override,
                          vm_tests_override=None)

    if build_name in cidb_unsafe or tryjob_config.master:
      tryjob_config.apply(debug_cidb=True)

    if tryjob_config.build_type != constants.PAYLOADS_TYPE:
      tryjob_config.apply(paygen=False)

    if tryjob_config.slave_configs:
      new_children = []
      for c in tryjob_config.slave_configs:
        if not config_lib.isTryjobConfig(site_config[c]):
          c = '%s-tryjob' % c
        new_children.append(c)

      tryjob_config.apply(slave_configs=new_children)

    # Save off the new config so we can insert into site_config.
    tryjob_configs[tryjob_name] = tryjob_config

  for tryjob_name, tryjob_config in tryjob_configs.items():
    site_config[tryjob_name] = tryjob_config


def BranchScheduleConfig():
  """Create a list of configs to schedule for branch builds.

  This function returns a list of build configs with just enough
  information to correctly schedule builds on branches. This function
  is only used by scripts/gen_luci_scheduler.

  After making changes to this function, they must be deployed to take
  effect. See gen_luci_scheduler --help for details.

  Returns:
    List of config_lib.BuildConfig instances.
  """
  #
  # Define each branched schedule with:
  #   branch_name: Name of the branch to build as a string.
  #   config_name: Name of the build config already present on the branch.
  #   label: Display label for UI use. Usually release, factory, firmware.
  #   schedule: When to do the build. Can take several formats.
  #     'triggered' for manual builds.
  #     Cron style in UTC timezone: '0 15 * * *'
  #     'with 30d interval' to run X time after previous build.
  #
  #     https://github.com/luci/luci-go/blob/master/scheduler/
  #                        appengine/messages/config.proto
  #
  # When updating this be sure to run
  # `config/chromeos_config_unittest --update`
  # or the change will fail chromite unittests.
  branch_builds = [
      # Add non release branch schedules here, if needed.
      # <branch>, <build_config>, <display_label>, <schedule>, <triggers>

      # NOTE: R69 & R73 are Long Term Support (LTS) milestones for lakitu and
      # they'd like to keep them a little longer. Please let
      # lakitu-dev@google.com know before deleting this.
      ('release-R69-10895.B', 'master-lakitu-release',
       config_lib.DISPLAY_LABEL_RELEASE, '0 4 * * *', None),
      ('release-R73-11647.B', 'master-lakitu-release',
       config_lib.DISPLAY_LABEL_RELEASE, '0 8 * * *', None),
  ]

  # The three active release branches.
  # (<branch>, [<android PFQs>], <chrome PFQ>)

  RELEASES = [
      ('release-R78-12499.B',
       ['gandof-android-nyc-pre-flight-branch',
        'grunt-android-pi-pre-flight-branch'],
       'chell-chrome-pre-flight-branch',
       ['orderfile-generate-toolchain',
        'orderfile-verify-toolchain']),

      ('release-R77-12371.B',
       ['gandof-android-nyc-pre-flight-branch',
        'grunt-android-pi-pre-flight-branch'],
       'samus-chrome-pre-flight-branch',
       None),

      ('release-R76-12239.B',
       ['reef-android-nyc-pre-flight-branch',
        'grunt-android-pi-pre-flight-branch'],
       'samus-chrome-pre-flight-branch',
       None),
  ]

  RELEASE_SCHEDULES = [
      '0 6 * * *',
      '0 5 * * *',
      '0 16 * * 0',
  ]

  PFQ_SCHEDULE = [
      '0 3,7,11,15,19,23 * * *',
      '0 2,6,10,14,18,22 * * *',
      '0 2,6,10,14,18,22 * * *',
      '0 3,7,11,15,19,23 * * *',
  ]

  ORDERFILE_SCHEDULES = [
      '0 8/12 * * *',
      '0 0/12 * * *',
  ]

  for ((branch, android_pfq, chrome_pfq, orderfile),
       schedule, android_schedule) in zip(
           RELEASES, RELEASE_SCHEDULES, PFQ_SCHEDULE):
    branch_builds.append([branch, 'master-release',
                          config_lib.DISPLAY_LABEL_RELEASE,
                          schedule, None])
    branch_builds.extend([[branch, pfq,
                           config_lib.DISPLAY_LABEL_RELEASE,
                           android_schedule, None]
                          for pfq in android_pfq])

    # We extract the release number from the branch, and use it to
    # watch for new chrome tags to trigger Chrome PFQ builds.
    # release-R71-11151.B -> 71 -> regexp:refs/tags/71\\..*
    release_num = re.search(r'release-R(\d+)-.*', branch).group(1)
    branch_builds.append(
        [branch, chrome_pfq, config_lib.DISPLAY_LABEL_RELEASE, 'triggered',
         [['https://chromium.googlesource.com/chromium/src',
           [r'regexp:refs/tags/%s\\..*' % release_num]]]])

    if orderfile:
      for b, s in zip(orderfile, ORDERFILE_SCHEDULES):
        branch_builds.append([branch, b,
                              config_lib.DISPLAY_LABEL_RELEASE,
                              s, None])

  # Convert all branch builds into scheduler config entries.
  default_config = config_lib.GetConfig().GetDefault()

  result = []
  for branch, config_name, label, schedule, trigger in branch_builds:
    result.append(default_config.derive(
        name=config_name,
        display_label=label,
        luci_builder=config_lib.LUCI_BUILDER_RELEASE,
        schedule_branch=branch,
        schedule=schedule,
        triggered_gitiles=trigger,
    ))

  return result


@factory.CachedFunctionCall
def GetConfig():
  """Create the Site configuration for all ChromeOS builds.

  Returns:
    A config_lib.SiteConfig.
  """
  defaults = DefaultSettings()

  ge_build_config = config_lib.LoadGEBuildConfigFromFile()

  # site_config with no templates or build configurations.
  site_config = config_lib.SiteConfig(defaults=defaults)
  boards_dict = GetBoardTypeToBoardsDict(ge_build_config)

  GeneralTemplates(site_config)

  chromeos_test.GeneralTemplates(site_config, ge_build_config)

  ToolchainBuilders(site_config, boards_dict, ge_build_config)

  ReleaseBuilders(site_config, boards_dict, ge_build_config)

  PayloadBuilders(site_config, boards_dict)

  SpecialtyBuilders(site_config, boards_dict, ge_build_config)

  PreCqBuilders(site_config, boards_dict, ge_build_config)

  CqBuilders(site_config, boards_dict, ge_build_config)

  IncrementalBuilders(site_config, boards_dict, ge_build_config)

  ReleaseAfdoBuilders(site_config, boards_dict, ge_build_config)

  InformationalBuilders(site_config, boards_dict, ge_build_config)

  FirmwareBuilders(site_config, boards_dict, ge_build_config)

  FactoryBuilders(site_config, boards_dict, ge_build_config)

  AndroidTemplates(site_config)

  chromeos_test.AndroidTemplates(site_config)

  AndroidPfqBuilders(site_config, boards_dict, ge_build_config)

  ChromePfqBuilders(site_config, boards_dict, ge_build_config)

  FullBuilders(site_config, boards_dict, ge_build_config)

  ApplyCustomOverrides(site_config)

  chromeos_test.ApplyConfig(site_config, boards_dict, ge_build_config)

  TryjobMirrors(site_config)

  return site_config
