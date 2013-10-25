#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions and common data for downloading NaCl toolchain binaries.
"""


BASE_DOWNLOAD_URL = (
    'https://commondatastorage.googleapis.com/nativeclient-archive2')

BASE_ONCE_DOWNLOAD_URL = (
    'https://commondatastorage.googleapis.com/nativeclient-once/object')

# TODO(dschuff): these mappings are now identical for x86 32/64. collapse them.
PLATFORM_MAPPING = {
    'windows': {
        'x86': ['win_x86',
                'win_x86_newlib',
                'pnacl_win_x86',
                'pnacl_translator',
                ('win_arm_newlib',
                 'WIN_GCC_ARM',
                 'WIN_BINUTILS_ARM',
                 'ALL_NEWLIB_ARM',
                 'ALL_GCC_LIBS_ARM')],
    },
    'linux': {
        'x86': ['linux_x86',
                'linux_x86_newlib',
                'pnacl_linux_x86',
                'linux_arm-trusted',
                'pnacl_translator',
                ('linux_arm_newlib',
                 'LINUX_GCC_ARM',
                 'LINUX_BINUTILS_ARM',
                 'ALL_NEWLIB_ARM',
                 'ALL_GCC_LIBS_ARM')],
        'arm': ['pnacl_translator'],
    },
    'mac': {
        'x86': ['mac_x86',
                'mac_x86_newlib',
                'pnacl_mac_x86',
                'pnacl_translator',
                ('mac_arm_newlib',
                 'MAC_GCC_ARM',
                 'MAC_BINUTILS_ARM',
                 'ALL_NEWLIB_ARM',
                 'ALL_GCC_LIBS_ARM')],
    },
}

def EncodeToolchainUrl(base_url, version, flavor):
  if 'pnacl' in flavor:
    return '%s/toolchain/%s/naclsdk_%s.tgz' % (
        base_url, version, flavor)
  elif flavor.endswith('_newlib'):
    return '%s/toolchain/%s/naclsdk_%s.tgz' % (
      base_url, version, flavor[:-len('_newlib')])
  elif 'x86' in flavor:
    return '%s/x86_toolchain/r%s/toolchain_%s.tar.bz2' % (
      base_url, version, flavor)
  elif 'new' == flavor:
    return '%s/%s.tgz' % (
      base_url, version)
  elif 'arm-trusted' in flavor:
    return '%s/toolchain/%s/sysroot-arm-trusted.tgz' % (
      base_url, version)
  else:
    return '%s/toolchain/%s/naclsdk_%s.tgz' % (
      base_url, version, flavor)


def IsArmUntrustedFlavor(flavor):
  return '_arm_newlib' in flavor

def IsArmTrustedFlavor(flavor):
  return 'arm-trusted' in flavor

def IsPnaclFlavor(flavor):
  return 'pnacl' in flavor

def IsSandboxedTranslatorFlavor(flavor):
  return 'translator' in flavor

def IsX86Flavor(flavor):
  return not IsPnaclFlavor(flavor) and not IsArmTrustedFlavor(flavor)

def IsNotNaClNewlibFlavor(flavor):
  return not flavor.endswith('_newlib') or flavor.startswith('pnacl')
