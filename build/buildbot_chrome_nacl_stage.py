#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Do all the steps required to build and test against nacl."""


import optparse
import os.path
import re
import shutil
import subprocess
import sys

import chromebinaries


# Copied from buildbot/buildbot_lib.py
def TryToCleanContents(path, file_name_filter=lambda fn: True):
  """
  Remove the contents of a directory without touching the directory itself.
  Ignores all failures.
  """
  if os.path.exists(path):
    for fn in os.listdir(path):
      TryToCleanPath(os.path.join(path, fn), file_name_filter)


# Copied from buildbot/buildbot_lib.py
def TryToCleanPath(path, file_name_filter=lambda fn: True):
  """
  Removes a file or directory.
  Ignores all failures.
  """
  if os.path.exists(path):
    if file_name_filter(path):
      print 'Trying to remove %s' % path
      if os.path.isdir(path):
        shutil.rmtree(path, ignore_errors=True)
      else:
        try:
          os.remove(path)
        except Exception:
          pass
    else:
      print 'Skipping %s' % path


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
      # Mac Chromium make builder
      'out/%s/Chromium.app/Contents/MacOS/Chromium' % mode,
      # Mac release make builder
      'out/%s/Google Chrome.app/Contents/MacOS/Google Chrome' % mode,
      # Mac Chromium xcode builder
      'xcodebuild/%s/Chromium.app/Contents/MacOS/Chromium' % mode,
      # Mac release xcode builder
      'xcodebuild/%s/Google Chrome.app/Contents/MacOS/Google Chrome' % mode,
  ]

  # Pick the first one we find.
  for chrome in chrome_locations:
    chrome_filename = os.path.join(src_dir, chrome)
    if os.path.exists(chrome_filename):
      return chrome_filename
  raise Exception('Cannot find a chome binary - specify one with '
                  '--browser_path?')


# TODO(ncbray): this is somewhat unsafe.  We should fix the underlying problem.
def CleanTempDir():
  # Only delete files and directories like:
  # a) C:\temp\83C4.tmp
  # b) /tmp/.org.chromium.Chromium.EQrEzl
  file_name_re = re.compile(
      r'[\\/]([0-9a-fA-F]+\.tmp|\.org\.chrom\w+\.Chrom\w+\..+)$')
  file_name_filter = lambda fn: file_name_re.search(fn) is not None

  path = os.environ.get('TMP', os.environ.get('TEMP', '/tmp'))
  if len(path) >= 4 and os.path.isdir(path):
    print
    print "Cleaning out the temp directory."
    print
    TryToCleanContents(path, file_name_filter)
  else:
    print
    print "Cannot find temp directory, not cleaning it."
    print


def RunCommand(cmd, cwd, env):
  sys.stdout.write('\nRunning %s\n\n' % ' '.join(cmd))
  sys.stdout.flush()
  retcode = subprocess.call(cmd, cwd=cwd, env=env)
  if retcode != 0:
    sys.stdout.write('\nFailed: %s\n\n' % ' '.join(cmd))
    sys.exit(retcode)


def BuildAndTest(options):
  # Refuse to run under cygwin.
  if sys.platform == 'cygwin':
    raise Exception('I do not work under cygwin, sorry.')

  # By default, use the version of Python is being used to run this script.
  python = sys.executable
  if sys.platform == 'darwin':
    # Mac 10.5 bots tend to use a particularlly old version of Python, look for
    # a newer version.
    macpython27 = '/Library/Frameworks/Python.framework/Versions/2.7/bin/python'
    if os.path.exists(macpython27):
      python = macpython27

  script_dir = os.path.dirname(os.path.abspath(__file__))
  nacl_dir = os.path.dirname(script_dir)
  src_dir = os.path.dirname(nacl_dir)

  # Decide platform specifics.
  env = dict(os.environ)
  if sys.platform in ['win32', 'cygwin']:
    if options.bits == 64:
      bits = 64
    elif options.bits == 32:
      bits = 32
    elif '64' in os.environ.get('PROCESSOR_ARCHITECTURE', '') or \
         '64' in os.environ.get('PROCESSOR_ARCHITEW6432', ''):
      bits = 64
    else:
      bits = 32
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
    scons = [python, 'scons.py']
  elif sys.platform == 'darwin':
    bits = 32
    scons = [python, 'scons.py']
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
    # xvfb-run has a 2-second overhead per invocation, so it is cheaper to wrap
    # the entire build step rather than each test (browser_headless=1).
    scons = ['xvfb-run', '--auto-servernum', python, 'scons.py']

  chrome_filename = FindChrome(src_dir, options)

  if options.jobs > 1:
    scons.append('-j%d' % options.jobs)

  scons.append('disable_tests=%s' % options.disable_tests)

  if options.buildbot is not None:
    scons.append('buildbot=%s' % (options.buildbot,))

  # Clean the output of the previous build.
  # Incremental builds can get wedged in weird ways, so we're trading speed
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
  # Note that we have to add nacl_irt_test to --mode in order to get
  # inbrowser_test_runner to run.
  # TODO(mseaborn): Change it so that inbrowser_test_runner is not a
  # special case.
  cmd = scons + ['--verbose', '-k', 'platform=x86-%d' % bits,
      '--mode=opt-host,nacl,nacl_irt_test',
      'disable_dynamic_plugin_loading=1',
      'chrome_browser_path=%s' % chrome_filename,
  ]
  if not options.integration_bot:
    cmd.append('disable_flaky_tests=1')
  cmd.append('chrome_browser_tests')
  if options.integration_bot:
    cmd.append('pyauto_tests')


  # Load the nacl DEPS file.
  # TODO(ncbray): move this out of chromebinaries into a common library?
  deps = chromebinaries.EvalDepsFile(os.path.join(nacl_dir, 'DEPS'))

  # Download the toolchain(s).
  if options.integration_bot:
    pnacl_toolchain = ['--pnacl-version',
                       deps['vars']['pnacl_toolchain_version']]
  else:
    pnacl_toolchain = ['--no-pnacl']
  RunCommand([python,
              os.path.join(script_dir, 'download_toolchains.py'),
              '--x86-version', deps['vars']['x86_toolchain_version'],
              '--no-arm-trusted'] + pnacl_toolchain, nacl_dir, os.environ)

  CleanTempDir()

  if not options.disable_newlib:
    sys.stdout.write('\n\nBuilding files needed for nacl-newlib testing...\n\n')
    RunCommand(cmd + ['do_not_run_tests=1', '-j8'], nacl_dir, env)
    sys.stdout.write('\n\nRunning nacl-newlib tests...\n\n')
    RunCommand(cmd, nacl_dir, env)

  if not options.disable_glibc:
    sys.stdout.write('\n\nBuilding files needed for nacl-glibc testing...\n\n')
    RunCommand(cmd + ['--nacl_glibc', 'do_not_run_tests=1', '-j8'], nacl_dir,
               env)
    sys.stdout.write('\n\nRunning nacl-glibc tests...\n\n')
    RunCommand(cmd + ['--nacl_glibc'], nacl_dir, env)

  # The chromium bots don't get the pnacl compiler, but the integration
  # bots do. So, only run pnacl tests on the integration bot.
  if not options.disable_pnacl and options.integration_bot:
    # TODO(dschuff) Remove this when we pass pyauto tests
    cmd = [opt for opt in cmd if opt != 'pyauto_tests']
    sys.stdout.write('\n\nBuilding files needed for pnacl testing...\n\n')
    RunCommand(cmd + ['bitcode=1', 'do_not_run_tests=1', '-j8'], nacl_dir, env)
    sys.stdout.write('\n\nRunning pnacl tests...\n\n')
    RunCommand(cmd + ['bitcode=1'], nacl_dir, env)


def MakeCommandLineParser():
  parser = optparse.OptionParser()
  parser.add_option('-m', '--mode', dest='mode', default='Debug',
                    help='Debug/Release mode')
  parser.add_option('-j', dest='jobs', default=1, type='int',
                    help='Number of parallel jobs')
  parser.add_option('--disable_glibc', dest='disable_glibc',
                    action='store_true', default=False,
                    help='Do not test using glibc.')
  parser.add_option('--disable_newlib', dest='disable_newlib',
                    action='store_true', default=False,
                    help='Do not test using newlib.')
  parser.add_option('--disable_pnacl', dest='disable_pnacl',
                    action='store_true', default=False,
                    help='Do not test using pnacl.')
  parser.add_option('--disable_tests', dest='disable_tests',
                    type='string', default='',
                    help='Comma-separated list of tests to omit')
  # TODO(ncbray): don't default based on CWD.
  pwd = os.environ.get('PWD', '')
  is_integration_bot = 'nacl-chrome' in pwd
  parser.add_option('--integration_bot', dest='integration_bot',
                    type='int', default=int(is_integration_bot),
                    help='Is this an integration bot?')

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
