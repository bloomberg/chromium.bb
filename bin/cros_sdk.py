#!/usr/bin/env python
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script fetches and prepares an SDK chroot.
"""

import optparse
import os
import subprocess
import sys
import urllib
import urlparse

sys.path.insert(0, os.path.abspath(__file__ + '/../..'))
import lib.cros_build_lib as cros_build_lib
import buildbot.constants as constants

SDK_DIR = os.path.join(constants.SOURCE_ROOT, 'sdks')
#TODO(zbehan): Rename back to chroot.
CHROOT_DIR = os.path.join(constants.SOURCE_ROOT, 'sdk-chroot')

DEFAULT_URL = 'http://commondatastorage.googleapis.com/cros-sdk/'
DEFAULT_VERSION = '2010.03.09'

def GetHostArch():
  """Returns a string for the host architecture"""
  out = cros_build_lib.RunCommand(['uname', '-m'],
      redirect_stdout=True).output
  return out.rstrip('\n')

def GetArchStageTarball(tarballArch, version = DEFAULT_VERSION):
  """Returns the URL for a given arch/version"""
  D = { 'x86_64': 'cros-sdk-' }
  try:
    return DEFAULT_URL + '/' + D[tarballArch] + version + '.tar.bz2'
  except KeyError:
    sys.exit('Unsupported arch: ' + arch)

def CreateChroot(sdk_path, sdk_url, replace):
  """Creates a new chroot from a given SDK"""
  if options.sdk_path and options.sdk_url:
    sys.exit('You can either select path or url, not both!')

  if not os.path.exists(SDK_DIR):
    cros_build_lib.RunCommand(['mkdir', '-p', SDK_DIR])

  # Based on selections, fetch the tarball
  if options.sdk_path:
    print 'Using "%s" as sdk' % options.sdk_path
    if not os.path.exists(options.sdk_path):
      sys.exit('No such file: %s' % options.sdk_path)

    cros_build_lib.RunCommand(['cp', '-f', options.sdk_path, SDK_DIR])
    tarball_dest = os.path.join(SDK_DIR,
        os.path.basename(options.sdk_path))
  else:
    if options.sdk_url:
      url = options.sdk_url
    else:
      arch = GetHostArch()
      if options.version:
        url = GetArchStageTarball(arch, options.version)
      else:
        url = GetArchStageTarball(arch)

    tarball_dest = os.path.join(SDK_DIR,
        os.path.basename(urlparse.urlparse(url).path))

    print 'Downloading sdk: "%s"' % url
    cros_build_lib.RunCommand(['curl', '-f', '--retry', '5', '-L',
        '-y', '30', '--output', tarball_dest, url])

  # TODO(zbehan): Unpack and install
  # For now, we simply call make_chroot on the prebuilt chromeos-sdk.
  # make_chroot provides a variety of hacks to make the chroot useable.
  # These should all be eliminated/minimised, after which, we can change
  # this to just unpacking the sdk.
  print 'Deferring to make_chroot'
  if replace:
    extra_args = '--replace'
  else:
    extra_args = '--noreplace'
  cros_build_lib.RunCommand([os.path.join(constants.SOURCE_ROOT,
      'src/scripts/make_chroot'), '--stage3_path', tarball_dest,
      '--chroot', CHROOT_DIR, extra_args])

if __name__ == '__main__':
  usage="""usage: %prog [options]

This script downloads and installs a CrOS SDK. If an SDK already
exists, it will do nothing at all, and every call will be a noop.
To replace, use --replace."""
  parser = optparse.OptionParser(usage)
  parser.add_option('-p', '--path',
                    dest='sdk_path', default='',
                    help=('Use sdk tarball located on this path'))
  parser.add_option('-u', '--url',
                    dest='sdk_url', default='',
                    help=('Use sdk tarball located on this url'))
  parser.add_option('-v', '--version',
                    dest='sdk_version', default=DEFAULT_VERSION,
                    help=('Use this sdk version'))
  parser.add_option('-r', '--replace',
                    action='store_true', dest='replace', default=False,
                    help=('Replace an existing chroot'))
  (options, remaining_arguments) = parser.parse_args()

  if not os.path.exists(CHROOT_DIR) or options.replace:
    CreateChroot(options.sdk_path, options.sdk_url, options.replace)

