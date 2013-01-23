#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Determine OS

Determine the name of the platform used to determine the correct Toolchain to
invoke.
"""

import optparse
import os
import re
import subprocess
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


if sys.version_info < (2, 6, 0):
  sys.stderr.write("python 2.6 or later is required run this script\n")
  sys.exit(1)


def ErrOut(text):
  sys.stderr.write(text + '\n')
  sys.exit(1)


def GetSDKPath():
  return os.getenv('NACL_SDK_ROOT', os.path.dirname(SCRIPT_DIR))


def GetPlatform():
  if sys.platform.startswith('cygwin') or sys.platform.startswith('win'):
    return 'win'

  if sys.platform.startswith('darwin'):
    return 'mac'

  if sys.platform.startswith('linux'):
    return 'linux'
  return None


def UseWin64():
  arch32 = os.environ.get('PROCESSOR_ARCHITECTURE', 'unk')
  arch64 = os.environ.get('PROCESSOR_ARCHITEW6432', 'unk')

  if arch32 == 'AMD64' or arch64 == 'AMD64':
    return True
  return False


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


def GetChromeArch(platform):
  if platform == 'win':
    if UseWin64():
      return 'x86_64'
    return 'x86_32'

  if platform in ['mac', 'linux']:
    chrome_path = os.getenv('CHROME_PATH', None)
    if not chrome_path:
      ErrOut('CHROME_PATH is undefined.')

    try:
      pobj = subprocess.Popen(['objdump', '-f', chrome_path],
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE)
      arch = pobj.communicate()[0]
      file_format = re.compile(r'(file format) ([a-zA-Z0-9_\-]+)')
      arch = file_format.search(arch).group(2)
      if 'arm' in arch:
        return 'arm'
      if '64' in arch:
        return 'x86_64'
      return 'x86_32'
    except Exception:
      print "FAILED"
      arch = None
  return arch


def GetLoaderPath(platform):
  sdk_path = GetSDKPath()
  arch = GetChromeArch(platform)
  return os.path.join(sdk_path, 'tools', 'sel_ldr_' + arch)


def GetHelperPath(platform):
  sdk_path = GetSDKPath()
  if platform != 'linux':
    return ''
  arch = GetChromeArch(platform)
  return os.path.join(sdk_path, 'tools', 'nacl_helper_bootstrap_' + arch)


def GetIrtBinPath(platform):
  sdk_path = GetSDKPath()
  arch = GetChromeArch(platform)
  return os.path.join(sdk_path, 'tools', 'irt_%s.nexe' % arch)


def GetPluginPath(platform):
  sdk_path = GetSDKPath()
  arch = GetChromeArch(platform)
  if platform == 'win':
    return os.path.join(sdk_path, 'tools', 'ppNaClPlugin_x86_32.dll')
  else:
    return os.path.join(sdk_path, 'tools', 'ppNaClPlugin_%s.so' % arch)


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--arch', help='Return architecture type.',
      action='store_true', dest='arch', default=False)
  parser.add_option('--chrome', help='Return chrome architecture type.',
      action='store_true', dest='chrome', default=False)
  parser.add_option('--helper', help='Return chrome helper path.',
      action='store_true', dest='helper', default=False)
  parser.add_option('--irtbin', help='Return irt binary path.',
      action='store_true', dest='irtbin', default=False)
  parser.add_option('--loader', help='Return NEXE loader path.',
      action='store_true', dest='loader', default=False)
  parser.add_option('--plugin', help='Return NaCl plugin path.',
      action='store_true', dest='plugin', default=False)

  options, _ = parser.parse_args(args[1:])

  platform = GetPlatform()
  if platform is None:
    print 'Unknown platform.'
    return 1

  if len(args) > 2:
    print 'Only specify one platform item.'
    return 1

  if len(args) == 1:
    print platform
    return 0

  if len(args) == 2:
    if options.arch:
      out = GetSystemArch(platform)
    if options.chrome:
      out = GetChromeArch(platform)
    if options.helper:
      out = GetHelperPath(platform)
    if options.irtbin:
      out = GetIrtBinPath(platform)
    if options.loader:
      out = GetLoaderPath(platform)
    if options.plugin:
      out = GetPluginPath(platform)
    print out
    return 0

  print 'Failed with args: ' + ' '.join(args)
  return 1


if __name__ == '__main__':
  sys.exit(main(sys.argv))
