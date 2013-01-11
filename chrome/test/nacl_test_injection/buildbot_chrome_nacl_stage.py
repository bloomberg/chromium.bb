#!/usr/bin/python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Do all the steps required to build and test against nacl."""


import optparse
import os.path
import re
import shutil
import subprocess
import sys


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
      # For Linux buildbots.  scripts/slave/extract_build.py extracts builds
      # to src/sconsbuild/ rather than src/out/.
      'sconsbuild/%s/chrome' % mode,
      # Windows Chromium ninja builder
      'out/%s/chrome.exe' % mode,
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


def RunTests(name, cmd, nacl_dir, env):
  sys.stdout.write('\n\nBuilding files needed for %s testing...\n\n' % name)
  RunCommand(cmd + ['do_not_run_tests=1', '-j8'], nacl_dir, env)
  sys.stdout.write('\n\nRunning %s tests...\n\n' % name)
  RunCommand(cmd, nacl_dir, env)


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
  src_dir = os.path.dirname(os.path.dirname(os.path.dirname(script_dir)))
  nacl_dir = os.path.join(src_dir, 'native_client')

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
    env['VS90COMNTOOLS'] = ('c:\\Program Files (x86)\\'
                            'Microsoft Visual Studio 9.0\\Common7\\Tools\\')
    env['VS80COMNTOOLS'] = ('c:\\Program Files (x86)\\'
                            'Microsoft Visual Studio 8.0\\Common7\\Tools\\')
  else:
    # 32bit HOST
    env['VS90COMNTOOLS'] = ('c:\\Program Files\\Microsoft Visual Studio 9.0\\'
                            'Common7\\Tools\\')
    env['VS80COMNTOOLS'] = ('c:\\Program Files\\Microsoft Visual Studio 8.0\\'
                            'Common7\\Tools\\')

  # Run nacl/chrome integration tests.
  # Note that we have to add nacl_irt_test to --mode in order to get
  # the "_irt" variant of "chrome_browser_tests" to run.
  cmd = scons + ['--verbose', '-k', 'platform=x86-%d' % bits,
      '--mode=opt-host,nacl,nacl_irt_test',
      'chrome_browser_path=%s' % chrome_filename,
  ]
  if not options.integration_bot and not options.morenacl_bot:
    cmd.append('disable_flaky_tests=1')
  cmd.append('chrome_browser_tests_irt')

  # Download the toolchain(s).
  if options.enable_pnacl:
    pnacl_toolchain = []
  else:
    pnacl_toolchain = ['--no-pnacl']
  RunCommand([python,
              os.path.join(nacl_dir, 'build', 'download_toolchains.py'),
              '--no-arm-trusted'] + pnacl_toolchain + ['TOOL_REVISIONS'],
             nacl_dir, os.environ)

  CleanTempDir()

  # Until we are sure that it is OK to switch to the new
  # Chrome-IPC-based PPAPI proxy (see
  # http://code.google.com/p/chromium/issues/detail?id=116317), we
  # also test the old SRPC-based PPAPI proxy.
  # TODO(mseaborn): Remove the second run when the switch is complete.
  for test_old_srpc_proxy in (False, True):
    env2 = env.copy()
    if test_old_srpc_proxy:
      env2['NACL_BROWSER_FLAGS'] = '--enable-nacl-srpc-proxy'

    if options.enable_newlib:
      RunTests('nacl-newlib', cmd, nacl_dir, env2)

    if options.enable_glibc:
      RunTests('nacl-glibc', cmd + ['--nacl_glibc'], nacl_dir, env2)

    if options.enable_pnacl:
      # TODO(dschuff): remove this when streaming is the default
      os.environ['NACL_STREAMING_TRANSLATION'] = 'true'
      RunTests('pnacl', cmd + ['bitcode=1'], nacl_dir, env2)


def MakeCommandLineParser():
  parser = optparse.OptionParser()
  parser.add_option('-m', '--mode', dest='mode', default='Debug',
                    help='Debug/Release mode')
  parser.add_option('-j', dest='jobs', default=1, type='int',
                    help='Number of parallel jobs')

  parser.add_option('--enable_newlib', dest='enable_newlib', default=-1,
                    type='int', help='Run newlib tests?')
  parser.add_option('--enable_glibc', dest='enable_glibc', default=-1,
                    type='int', help='Run glibc tests?')
  parser.add_option('--enable_pnacl', dest='enable_pnacl', default=-1,
                    type='int', help='Run pnacl tests?')


  # Deprecated, but passed to us by a script in the Chrome repo.
  # Replaced by --enable_glibc=0
  parser.add_option('--disable_glibc', dest='disable_glibc',
                    action='store_true', default=False,
                    help='Do not test using glibc.')

  parser.add_option('--disable_tests', dest='disable_tests',
                    type='string', default='',
                    help='Comma-separated list of tests to omit')
  builder_name = os.environ.get('BUILDBOT_BUILDERNAME', '')
  is_integration_bot = 'nacl-chrome' in builder_name
  parser.add_option('--integration_bot', dest='integration_bot',
                    type='int', default=int(is_integration_bot),
                    help='Is this an integration bot?')
  is_morenacl_bot = (
      'More NaCl' in builder_name or
      'naclmore' in builder_name)
  parser.add_option('--morenacl_bot', dest='morenacl_bot',
                    type='int', default=int(is_morenacl_bot),
                    help='Is this a morenacl bot?')

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
  if options.integration_bot and options.morenacl_bot:
    parser.error('ERROR: cannot be both an integration bot and a morenacl bot')

  # Set defaults for enabling newlib.
  if options.enable_newlib == -1:
    options.enable_newlib = 1

  # Set defaults for enabling glibc.
  if options.enable_glibc == -1:
    if options.integration_bot or options.morenacl_bot:
      options.enable_glibc = 1
    else:
      options.enable_glibc = 0

  # Set defaults for enabling pnacl.
  if options.enable_pnacl == -1:
    if options.integration_bot or options.morenacl_bot:
      options.enable_pnacl = 1
    else:
      options.enable_pnacl = 0

  if args:
    parser.error('ERROR: invalid argument')
  BuildAndTest(options)


if __name__ == '__main__':
  Main()
