#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Do all the steps required to build and test against nacl."""


import optparse
import os.path
import shutil
import subprocess
import sys

import chromebinaries


def FindChrome(src_dir, options):
  if options.browser_path:
    return options.browser_path

  # List of places that chrome could live.
  # In theory we should be more careful about what platform we're actually
  # building for.
  # As currently constructed, this will also hork people who have debug and
  # release builds sitting side by side who build locally.
  mode = options.mode
  chrome_locations = [
      'build/%s/chrome.exe' % mode,
      'chrome/%s/chrome.exe' % mode,
      'out/%s/chrome' % mode,
      'xcodebuild/%s/Chromium.app/Contents/MacOS/Chromium' % mode,
      'xcodebuild/%s/Chrome.app/Contents/MacOS/Chrome' % mode,
  ]

  # Pick the first one we find.
  for chrome in chrome_locations:
    chrome_filename = os.path.join(src_dir, chrome)
    if os.path.exists(chrome_filename):
      return chrome_filename
  raise Exception('Cannot find a chome binary - specify one with '
                  '--browser_path?')


def BuildAndTest(options):
  # Refuse to run under cygwin.
  if sys.platform == 'cygwin':
    raise Exception('I do not work under cygwin, sorry.')

  script_dir = os.path.dirname(os.path.abspath(__file__))
  nacl_dir = os.path.dirname(script_dir)
  src_dir = os.path.dirname(nacl_dir)

  # Load the nacl DEPS file.
  # TODO(ncbray): move this out of chromebinaries into a common library?
  deps = chromebinaries.EvalDepsFile(os.path.join(nacl_dir, 'DEPS'))

  # Get out the toolchain revs.
  x86_rev = deps['vars']['x86_toolchain_version']
  arm_rev = deps['vars']['arm_toolchain_version']

  # Download the toolchain.
  subprocess.check_call([sys.executable,
                        os.path.join(script_dir, 'download_toolchains.py'),
                        '--x86-version', x86_rev,
                        '--arm-version', arm_rev])

  # Decide platform specifics.
  env = dict(os.environ)
  if sys.platform in ['win32', 'cygwin']:
    shell = True
    if options.bits == 64:
      bits = 64
    elif options.bits == 32:
      bits = 32
    elif '64' in os.environ.get('PROCESSOR_ARCHITECTURE', '') or \
         '64' in os.environ.get('PROCESSOR_ARCHITEW6432', ''):
      bits = 64
    else:
      bits = 32
    tools_bits = {32: 'x86', 64: 'x64'}[bits]
    msvs_path = ';'.join([
        r'c:\Program Files\Microsoft Visual Studio 9.0\VC',
        r'c:\Program Files (x86)\Microsoft Visual Studio 9.0\VC',
        r'c:\Program Files\Microsoft Visual Studio 9.0\Common7\Tools',
        r'c:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\Tools',
        r'c:\Program Files\Microsoft Visual Studio 8\VC',
        r'c:\Program Files (x86)\Microsoft Visual Studio 8\VC',
        r'c:\Program Files\Microsoft Visual Studio 8\Common7\Tools',
        r'c:\Program Files (x86)\Microsoft Visual Studio 8\Common7\Tools',
    ])
    env['PATH'] += ';' + msvs_path
    scons = ['vcvarsall %s && scons.bat' % tools_bits]
  elif sys.platform == 'darwin':
    bits = 32
    scons = ['./scons']
    shell = False
  else:
    p = subprocess.Popen(
        'uname -m | '
        'sed -e "s/i.86/ia32/;s/x86_64/x64/;s/amd64/x64/;s/arm.*/arm/"',
        shell=True, stdout=subprocess.PIPE)
    (p_stdout, _) = p.communicate()
    assert p.returncode == 0
    if options.bits == 64:
      bits = 64
    elif options.bits == 32:
      bits = 32
    elif p_stdout.find('64') >= 0:
      bits = 64
    else:
      bits = 32
    scons = ['./scons']
    shell = False

  chrome_filename = FindChrome(src_dir, options)

  if options.jobs > 1:
    scons.append('-j%d' % options.jobs)

  if options.buildbot is not None:
    scons.append('buildbot=%s' % (options.buildbot,))

  # Clean the output of the previous build.
  # Incremental builds can get wedged in wierd ways, so we're trading speed
  # for reliability.
  shutil.rmtree(os.path.join(nacl_dir, 'scons-out'), True)

  # check that the HOST (not target) is 64bit
  # this is emulating what msvs_env.bat is doing
  if '64' in os.environ.get('PROCESSOR_ARCHITECTURE', '') or \
     '64' in os.environ.get('PROCESSOR_ARCHITEW6432', ''):
    # 64bit HOST
    env['VS90COMNTOOLS'] = 'c:\\Program Files (x86)\\Microsoft Visual Studio 9.0\\Common7\\Tools\\'
    env['VS80COMNTOOLS'] = 'c:\\Program Files (x86)\\Microsoft Visual Studio 8.0\\Common7\\Tools\\'
  else:
    # 32bit HOST
    env['VS90COMNTOOLS'] = 'c:\\Program Files\\Microsoft Visual Studio 9.0\\Common7\\Tools\\'
    env['VS80COMNTOOLS'] = 'c:\\Program Files\\Microsoft Visual Studio 8.0\\Common7\\Tools\\'

  # Run nacl/chrome integration tests.
  cmd = scons + ['-k', 'platform=x86-%d' % bits,
      'browser_headless=1',
      'disable_flaky_tests=1',
      'disable_dynamic_plugin_loading=1',
      'chrome_browser_path=' + chrome_filename,
      'chrome_browser_tests',
  ]
  subprocess.check_call(cmd, shell=shell, cwd=nacl_dir, env=env)


def MakeCommandLineParser():
  parser = optparse.OptionParser()
  parser.add_option('-m', '--mode', dest='mode', default='Debug',
                    help='Debug/Release mode')
  parser.add_option('-j', dest='jobs', default=1, type='int',
                    help='Number of parallel jobs')

  # Not used on the bots, but handy for running the script manually.
  parser.add_option('--bits', dest='bits', action='store',
                    type='int', default=None,
                    help='32/64')
  parser.add_option('--browser_path', dest='browser_path', action='store',
                    type='string', default=None,
                    help='Path to the chrome browser.')
  parser.add_option('--buildbot', dest='buildbot', action='store',
                    type='string', default=None,
                    help='Value passed to scons as buildbot= option.')
  return parser


def Main():
  parser = MakeCommandLineParser()
  options, args = parser.parse_args()
  if args:
    parser.error('ERROR: invalid argument')
  BuildAndTest(options)


if __name__ == '__main__':
  Main()
