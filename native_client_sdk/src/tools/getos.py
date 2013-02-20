#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Determine OS and various other system properties.

Determine the name of the platform used and other system properties such as
the location of Chrome.  This is used, for example, to determine the correct
Toolchain to invoke.
"""

import optparse
import os
import re
import subprocess
import sys

import oshelpers


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CHROME_EXE_BASENAME = 'google-chrome'


if sys.version_info < (2, 6, 0):
  sys.stderr.write("python 2.6 or later is required run this script\n")
  sys.exit(1)


class Error(Exception):
  pass


def GetSDKPath():
  return os.getenv('NACL_SDK_ROOT', os.path.dirname(SCRIPT_DIR))


def GetPlatform():
  if sys.platform.startswith('cygwin') or sys.platform.startswith('win'):
    return 'win'
  elif sys.platform.startswith('darwin'):
    return 'mac'
  elif sys.platform.startswith('linux'):
    return 'linux'
  else:
    raise Error("Unknown platform: %s" % sys.platform)


def UseWin64():
  arch32 = os.environ.get('PROCESSOR_ARCHITECTURE')
  arch64 = os.environ.get('PROCESSOR_ARCHITEW6432')

  if arch32 == 'AMD64' or arch64 == 'AMD64':
    return True
  return False


def GetSDKVersion():
  root = GetSDKPath()
  readme = os.path.join(root, "README")
  if not os.path.exists(readme):
    raise Error("README not found in SDK root: %s" % root)

  version = None
  revision = None
  for line in open(readme):
    if ':' in line:
      name, value = line.split(':', 1)
      if name == "Version":
        version = value.strip()
      if name == "Revision":
        revision = value.strip()

  if revision == None or version == None:
    raise Error("error parsing SDK README: %s" % readme)

  try:
    revision = int(revision)
    version = int(version)
  except ValueError:
    raise Error("error parsing SDK README: %s" % readme)

  return (version, revision)


def GetSystemArch(platform):
  if platform == 'win':
    if UseWin64():
      return 'x86_64'
    return 'x86_32'

  if platform in ['mac', 'linux']:
    try:
      pobj = subprocess.Popen(['uname', '-m'], stdout= subprocess.PIPE)
      arch = pobj.communicate()[0]
      arch = arch.split()[0]
      if arch.startswith('arm'):
        arch = 'arm'
    except Exception:
      arch = None
  return arch


def GetChromePath():
  chrome_path = os.environ.get('CHROME_PATH')
  if chrome_path:
    if not os.path.exists(chrome_path):
      raise Error('Invalid CHROME_PATH: %s' % chrome_path)
  else:
    chrome_path = oshelpers.FindExeInPath(CHROME_EXE_BASENAME)
    if not chrome_path:
      raise Error('CHROME_PATH is undefined, and %s not found in PATH.' %
                  CHROME_EXE_BASENAME)

  return os.path.realpath(chrome_path)


def GetChromeArch(platform):
  if platform == 'win':
    if UseWin64():
      return 'x86_64'
    return 'x86_32'

  chrome_path = GetChromePath()

  # If CHROME_PATH is set to point to google-chrome or google-chrome
  # was found in the PATH and we are running on UNIX then google-chrome
  # is a bash script that points to 'chrome' in the same folder.
  if os.path.basename(chrome_path) == 'google-chrome':
    chrome_path = os.path.join(os.path.dirname(chrome_path), 'chrome')

  try:
    pobj = subprocess.Popen(['objdump', '-f', chrome_path],
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    output, stderr = pobj.communicate()
    # error out here if objdump failed
    if pobj.returncode:
      raise Error(output + stderr.strip())
  except OSError as e:
    # This will happen if objdump is not installed
    raise Error("Error running objdump: %s" % e)

  pattern = r'(file format) ([a-zA-Z0-9_\-]+)'
  match = re.search(pattern, output)
  if not match:
    raise Error("Error running objdump on: %s" % chrome_path)

  arch = match.group(2)
  if 'arm' in arch:
    return 'arm'
  if '64' in arch:
    return 'x86_64'
  return 'x86_32'


def GetLoaderPath(platform):
  sdk_path = GetSDKPath()
  arch = GetChromeArch(platform)
  sel_ldr = os.path.join(sdk_path, 'tools', 'sel_ldr_' + arch)
  if not os.path.exists(sel_ldr):
    raise Error("sel_ldr not found: %s" % sel_ldr)
  return sel_ldr


def GetHelperPath(platform):
  sdk_path = GetSDKPath()
  if platform != 'linux':
    return ''
  arch = GetChromeArch(platform)
  helper = os.path.join(sdk_path, 'tools', 'nacl_helper_bootstrap_' + arch)
  if not os.path.exists(helper):
    raise Error("helper not found: %s" % helper)
  return helper


def GetIrtBinPath(platform):
  sdk_path = GetSDKPath()
  arch = GetChromeArch(platform)
  irt =  os.path.join(sdk_path, 'tools', 'irt_core_%s.nexe' % arch)
  if not os.path.exists(irt):
    raise Error("irt not found: %s" % irt)
  return irt


def ParseVersion(version):
  if '.' in version:
    version = version.split('.')
  else:
    version = (version, '0')

  try:
    return tuple(int(x) for x in version)
  except ValueError:
    raise Error('error parsing SDK version: %s' % version)


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--arch', action='store_true',
      help='Print architecture of current machine (x86_32, x86_64 or arm).')
  parser.add_option('--chrome', action='store_true',
      help='Print the path chrome (by first looking in $CHROME_PATH and '
           'then $PATH).')
  parser.add_option('--chrome-arch', action='store_true',
      help='Print architecture of chrome executable.')
  parser.add_option('--helper', action='store_true',
      help='Print chrome helper path.')
  parser.add_option('--irtbin', action='store_true',
      help='Print irt binary path.')
  parser.add_option('--loader', action='store_true',
      help='Print NEXE loader path.')
  parser.add_option('--sdk-version', action='store_true',
      help='Print major version of the NaCl SDK.')
  parser.add_option('--sdk-revision', action='store_true',
      help='Print revision number of the NaCl SDK.')
  parser.add_option('--check-version',
      help='Check that the SDK version is at least as great as the '
           'version passed in.')

  options, _ = parser.parse_args(args)

  platform = GetPlatform()

  if len(args) > 1:
    parser.error('Only one option can be specified at a time.')

  if not args:
    print platform
    return 0

  if options.arch:
    out = GetSystemArch(platform)
  elif options.chrome_arch:
    out = GetChromeArch(platform)
  elif options.chrome:
    out = GetChromePath()
  elif options.helper:
    out = GetHelperPath(platform)
  elif options.irtbin:
    out = GetIrtBinPath(platform)
  elif options.loader:
    out = GetLoaderPath(platform)
  elif options.sdk_version:
    out = GetSDKVersion()[0]
  elif options.sdk_revision:
    out = GetSDKVersion()[1]
  elif options.check_version:
    required_version = ParseVersion(options.check_version)
    version = GetSDKVersion()
    if version < required_version:
      raise Error("SDK version too old (current: %s.%s, required: %s.%s)"
             % (version[0], version[1],
                required_version[0], required_version[1]))
    out = None

  if out:
    print out
  return 0


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except Error as e:
    sys.stderr.write(str(e) + '\n')
    sys.exit(1)
