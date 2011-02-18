# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dictionary of configuration types for cbuildbot.

Each dictionary entry is in turn a dictionary of config_param->value.

config_param's:
board -- The board of the image to build.

master -- This bot pushes changes to the overlays.
important -- Master bot uses important bots to determine overall status.
             i.e. if master bot succeeds and other important slaves succeed
             then the master will uprev packages.  This should align
             with info vs. closer except for the master.and options.tests
hostname -- Needed for 'important' slaves.  The hostname of the bot.  Should
            match hostname in slaves.cfg in buildbot checkout.

uprev -- Uprevs the local ebuilds to build new changes since last stable.
         build.  If master then also pushes these changes on success.
rev_overlays -- Select what overlays to look at for revving. This can be
                'public', 'private' or 'both'.
push_overlays -- Select what overlays to push at. This should be a subset of
                 rev_overlays for the particular builder.  Must be None if
                 not a master.  There should only be one master bot pushing
                 changes to each overlay per branch.

unittests -- Runs unittests for packages.
vm_tests -- Runs the smoke suite and au test harness in a qemu-based VM
            using KVM.

usepkg -- Use binary packages to bootstrap, when possible. (emerge --usepkg)
chroot_replace -- wipe and replace chroot, but not source.

build_type -- Upload prebuilts under the specified category. Can be any of
              [preflight|full|chrome].
test_mod -- Create a test mod image for archival.
factory_install_mod -- Create a factory install image for archival.
factory_test_mod -- Create a factory test image for archival.
"""

# TODO(dgarrett) Make test_mod, factory_install_mod, factory_test_mod options
# go away when these options work for arm.

default = {
  # 'board' No default value

  'master' : False,
  'important' : False,
  # 'hostname' No default value

  'uprev' : False,
  'rev_overlays': 'public',
  'push_overlays': None,

  'unittests' : True,
  'vm_tests' : True,

  'usepkg' : True,
  'chroot_replace' : False,

  'build_type' : False,
  'archive_build_debug' : False,
  'test_mod' : False,
  'factory_install_mod' : False,
  'factory_test_mod' : False,
}

arm = {
  # VM/tests are broken on arm.
  'unittests' : False,
  'vm_tests' : False,

  # These images don't work for arm.
  'factory_install_mod' : False,
  'factory_test_mod' : False,
}

full = {
  # Full builds are test build to show that we can build from scratch,
  # so use settings to build from scratch, and archive the results.
  'usepkg' : False,
  'chroot_replace' : True,

  'build_type': 'full',
  'archive_build_debug' : True,
  'test_mod' : True,
  'factory_install_mod' : True,
  'factory_test_mod' : True,
}


config = {}

config['x86-generic-pre-flight-queue'] = default.copy()
config['x86-generic-pre-flight-queue'].update({
  'board' : 'x86-generic',
  'master' : True,
  'hostname' : 'chromeosbuild2',

  'uprev' : True,
  'build_type': 'preflight',
  'rev_overlays': 'public',
  'push_overlays': 'public',
})

config['x86-generic-chrome-pre-flight-queue'] = default.copy()
config['x86-generic-chrome-pre-flight-queue'].update({
  'board' : 'x86-generic',
  'master' : True,

  'build_type': 'chrome',
  'uprev' : False,
  'rev_overlays': 'public',
  'push_overlays': 'public',
})


config['x86-mario-pre-flight-queue'] = default.copy()
config['x86-mario-pre-flight-queue'].update({
  'board' : 'x86-mario',
  'master' : True,

  'uprev' : True,
  'rev_overlays': 'both',
  'push_overlays': 'private',
})

config['x86-mario-pre-flight-branch'] = default.copy()
config['x86-mario-pre-flight-branch'].update({
  'board' : 'x86-mario',
  'master' : True,

  'uprev' : True,
  'rev_overlays': 'both',
  'push_overlays': 'both',
})

config['x86-agz-bin'] = default.copy()
config['x86-agz-bin'].update({
  'board' : 'x86-agz',

  'uprev' : True,
  'rev_overlays': 'both',
  'push_overlays': None,
})

config['x86-dogfood-bin'] = default.copy()
config['x86-dogfood-bin'].update({
  'board' : 'x86-dogfood',

  'uprev' : True,
  'rev_overlays': 'both',
  'push_overlays': None,
})

config['x86-pineview-bin'] = default.copy()
config['x86-pineview-bin'].update({
  'board' : 'x86-pineview',

  'uprev' : True,
  'rev_overlays': 'public',
  'push_overlays': None,
})

config['arm-tegra2-bin'] = default.copy()
config['arm-tegra2-bin'].update(arm)
config['arm-tegra2-bin'].update({
  'board' : 'tegra2_dev-board',

  'uprev' : True,
  'rev_overlays': 'public',
  'push_overlays': None,
})

config['arm-generic-bin'] = default.copy()
config['arm-generic-bin'].update(arm)
config['arm-generic-bin'].update({
  'board' : 'arm-generic',

  'uprev' : True,
  'rev_overlays': 'public',
  'push_overlays': None,
})

config['arm-generic-full'] = default.copy()
config['arm-generic-full'].update(arm)
config['arm-generic-full'].update(full)
config['arm-generic-full'].update({
  'board' : 'arm-generic',
})

config['arm-tegra2-full'] = default.copy()
config['arm-tegra2-full'].update(arm)
config['arm-tegra2-full'].update(full)
config['arm-tegra2-full'].update({
  'board' : 'tegra2_dev-board',
})

config['arm-tegra2-seaboard-full'] = default.copy()
config['arm-tegra2-seaboard-full'].update(arm)
config['arm-tegra2-seaboard-full'].update(full)
config['arm-tegra2-seaboard-full'].update({
  'board' : 'tegra2_seaboard',
})

config['x86-generic-full'] = default.copy()
config['x86-generic-full'].update(full)
config['x86-generic-full'].update({
  'board' : 'x86-generic',
})

config['x86-pineview-full'] = default.copy()
config['x86-pineview-full'].update(full)
config['x86-pineview-full'].update({
  'board' : 'x86-pineview',
})

# TODO(dgarrett) delete when buildbot updated to use new names
config['x86_agz_bin'] = config['x86-agz-bin']
config['x86_dogfood_bin'] = config['x86-dogfood-bin']
config['x86_pineview_bin'] = config['x86-pineview-bin']
config['arm_tegra2_bin'] = config['arm-tegra2-bin']
config['arm_generic_bin'] = config['arm-generic-bin']
