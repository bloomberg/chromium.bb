#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to install a Debian Wheezy sysroot for making official Google Chrome
# Linux builds.
# The sysroot is needed to make Chrome work for Debian Wheezy.
# This script can be run manually but is more often run as part of gclient
# hooks. When run from hooks this script should be a no-op on non-linux
# platforms.

# The sysroot image could be constructed from scratch based on the current
# state or Debian Wheezy but for consistency we currently use a pre-built root
# image. The image will normally need to be rebuilt every time chrome's build
# dependancies are changed.

import hashlib
import platform
import optparse
import os
import re
import shutil
import subprocess
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
URL_PREFIX = 'https://commondatastorage.googleapis.com'
URL_PATH = 'chrome-linux-sysroot/toolchain'
REVISION = 264817
TARBALL_AMD64 = 'debian_wheezy_amd64_sysroot.tgz'
TARBALL_I386 = 'debian_wheezy_i386_sysroot.tgz'
TARBALL_AMD64_SHA1SUM = '74b7231e12aaf45c5c5489d9aebb56bd6abb3653'
TARBALL_I386_SHA1SUM = 'fe3d284926839683b00641bc66c9023f872ea4b4'
SYSROOT_DIR_AMD64 = 'debian_wheezy_amd64-sysroot'
SYSROOT_DIR_I386 = 'debian_wheezy_i386-sysroot'


def get_sha1(filename):
  sha1 = hashlib.sha1()
  with open(filename, 'rb') as f:
    while True:
      # Read in 1mb chunks, so it doesn't all have to be loaded into memory.
      chunk = f.read(1024*1024)
      if not chunk:
        break
      sha1.update(chunk)
  return sha1.hexdigest()


def main(args):
  if options.arch not in ['amd64', 'i386']:
    print 'Unknown architecture: %s' % options.arch
    return 1

  if options.linux_only:
    # This argument is passed when run from the gclient hooks.
    # In this case we return early on non-linux platforms.
    if not sys.platform.startswith('linux'):
      return 0

    # Only install the sysroot for an Official Chrome Linux build.
    defined = ['branding=Chrome', 'buildtype=Official']
    undefined = ['chromeos=1']
    gyp_defines = os.environ.get('GYP_DEFINES', '')
    for option in defined:
      if option not in gyp_defines:
        return 0
    for option in undefined:
      if option in gyp_defines:
        return 0

    # Check for optional target_arch and only install for that architecture.
    # If target_arch is not specified, then only install for the host
    # architecture.
    host_arch = ''
    if 'target_arch=x64' in gyp_defines:
      host_arch = 'amd64'
    elif 'target_arch=ia32' in gyp_defines:
      host_arch = 'i386'
    else:
      # Figure out host arch using build/detect_host_arch.py.
      SRC_DIR = os.path.abspath(
          os.path.join(SCRIPT_DIR, '..', '..', '..', '..'))
      sys.path.append(os.path.join(SRC_DIR, 'build'))
      import detect_host_arch

      detected_host_arch = detect_host_arch.HostArch()
      if detected_host_arch == 'x64':
        host_arch = 'amd64'
      elif detected_host_arch == 'ia32':
        host_arch = 'i386'
    if host_arch != options.arch:
      return 0

  # The sysroot directory should match the one specified in build/common.gypi.
  # TODO(thestig) Consider putting this else where to avoid having to recreate
  # it on every build.
  linux_dir = os.path.dirname(SCRIPT_DIR)
  if options.arch == 'amd64':
    sysroot = os.path.join(linux_dir, SYSROOT_DIR_AMD64)
    tarball_filename = TARBALL_AMD64
    tarball_sha1sum = TARBALL_AMD64_SHA1SUM
  else:
    sysroot = os.path.join(linux_dir, SYSROOT_DIR_I386)
    tarball_filename = TARBALL_I386
    tarball_sha1sum = TARBALL_I386_SHA1SUM
  url = '%s/%s/%s/%s' % (URL_PREFIX, URL_PATH, REVISION, tarball_filename)

  stamp = os.path.join(sysroot, '.stamp')
  if os.path.exists(stamp):
    with open(stamp) as s:
      if s.read() == url:
        print 'Debian Wheezy %s root image already up-to-date: %s' % \
            (options.arch, sysroot)
        return 0

  print 'Installing Debian Wheezy %s root image: %s' % (options.arch, sysroot)
  if os.path.isdir(sysroot):
    shutil.rmtree(sysroot)
  os.mkdir(sysroot)
  tarball = os.path.join(sysroot, tarball_filename)
  subprocess.check_call(['curl', '-L', url, '-o', tarball])
  sha1sum = get_sha1(tarball)
  if sha1sum != tarball_sha1sum:
    print 'Tarball sha1sum is wrong.'
    print 'Expected %s, actual: %s' % (tarball_sha1sum, sha1sum)
    return 1
  subprocess.check_call(['tar', 'xf', tarball, '-C', sysroot])
  os.remove(tarball)

  with open(stamp, 'w') as s:
    s.write(url)
  return 0


if __name__ == '__main__':
  parser = optparse.OptionParser('usage: %prog [OPTIONS]')
  parser.add_option('', '--linux-only', dest='linux_only', action='store_true',
                    default=False, help='Only install sysroot for official '
                                        'Linux builds')
  parser.add_option('', '--arch', dest='arch',
                    help='Sysroot architecture, i386 or amd64')
  options, args = parser.parse_args()
  sys.exit(main(options))
