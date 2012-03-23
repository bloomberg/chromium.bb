#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions and common data for downloading NaCl toolchain binaries.
"""

import sys


BASE_DOWNLOAD_URL = \
    'http://commondatastorage.googleapis.com/nativeclient-archive2'

PLATFORM_MAPPING = {
    'windows': {
        'x86-32': ['win_x86',
                   'win_x86_newlib',
                   # When the pnacl windows tests are enabled, uncomment this.
                   # 'pnacl_win_x86_32',
                   'pnacl_translator'],
        'x86-64': ['win_x86',
                   'win_x86_newlib',
                   # When the pnacl windows tests are enabled, uncomment this.
                   # 'pnacl_win_x86_32',
                   'pnacl_translator'],
    },
    'linux': {
        'x86-32': ['linux_x86',
                   'linux_x86_newlib',
                   'pnacl_linux_x86_32',
                   'linux_arm-trusted',
                   'pnacl_translator'],
        'x86-64': ['linux_x86',
                   'linux_x86_newlib',
                   'pnacl_linux_x86_64',
                   'linux_arm-trusted',
                   'pnacl_translator'],
        'arm'   : ['pnacl_translator'],
    },
    'mac': {
        'x86-32': ['mac_x86',
                   'mac_x86_newlib',
                   'pnacl_mac_x86_32',
                   'pnacl_translator'],
        'x86-64': ['mac_x86',
                   'mac_x86_newlib',
                   'pnacl_mac_x86_32',
                   'pnacl_translator'],
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
  else:
    return '%s/toolchain/%s/naclsdk_%s.tgz' % (
      base_url, version, flavor)


def IsArmTrustedFlavor(flavor):
  return 'arm' in flavor


def IsPnaclFlavor(flavor):
  return 'pnacl' in flavor


def IsNaClNewlibFlavor(flavor):
  return flavor.endswith('_newlib') and not flavor.startswith('pnacl')
