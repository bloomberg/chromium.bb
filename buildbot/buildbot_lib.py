#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Enable 'with' statements in Python 2.5
from __future__ import with_statement

import optparse
import os.path
import shutil
import subprocess
import sys
import time
import traceback


ARCH_MAP = {
    '32': {
        'gyp_arch': 'ia32',
        'scons_platform': 'x86-32',
        },
    '64': {
        'gyp_arch': 'x64',
        'scons_platform': 'x86-64',
        },
    'arm': {
        'gyp_arch': 'arm',
        'scons_platform': 'arm',
        },
    }


def GetHostPlatform():
  sys_platform = sys.platform.lower()
  if sys_platform.startswith('linux'):
    return 'linux'
  elif sys_platform in ('win', 'win32', 'windows', 'cygwin'):
    return 'win'
  elif sys_platform in ('darwin', 'mac'):
    return 'mac'
  else:
    raise Exception('Can not determine the platform!')

def SetDefaultContextAttributes(context):
  """
  Set default values for the attributes needed by the SCons function, so that
  SCons can be run without needing ParseStandardCommandLine
  """
  context['platform'] = GetHostPlatform()
  context['mode'] = 'opt'
  context['default_scons_mode'] = ['opt-host', 'nacl']
  context['clang'] = False
  context['asan'] = False
  context['pnacl'] = False
  context['use_glibc'] = False
  context['use_breakpad_tools'] = False
  context['max_jobs'] = 8

def ParseStandardCommandLine(context):
  """
  The standard buildbot scripts require 3 arguments to run.  The first
  argument (dbg/opt) controls if the build is a debug or a release build.  The
  second argument (32/64) controls the machine architecture being targeted.
  The third argument (newlib/glibc) controls which c library we're using for
  the nexes.  Different buildbots may have different sets of arguments.
  """

  parser = optparse.OptionParser()
  parser.add_option('-n', '--dry-run', dest='dry_run', default=False,
                    action='store_true', help='Do not execute any commands.')
  parser.add_option('--inside-toolchain', dest='inside_toolchain',
                    default=bool(os.environ.get('INSIDE_TOOLCHAIN')),
                    action='store_true', help='Inside toolchain build.')
  parser.add_option('--clang', dest='clang', default=False,
                    action='store_true', help='Build trusted code with Clang.')
  parser.add_option('--coverage', dest='coverage', default=False,
                    action='store_true',
                    help='Build and test for code coverage.')
  parser.add_option('--validator', dest='validator', default=False,
                    action='store_true',
                    help='Only run validator regression test')
  parser.add_option('--asan', dest='asan', default=False,
                    action='store_true', help='Build trusted code with ASan.')
  parser.add_option('--step-suffix', metavar='SUFFIX', default='',
                    help='Append SUFFIX to buildbot step names.')
  parser.add_option('--no-gyp', dest='no_gyp', default=False,
                    action='store_true', help='Do not run the gyp build')
  parser.add_option('--use-breakpad-tools', dest='use_breakpad_tools',
                    default=False, action='store_true',
                    help='Use breakpad tools for testing')

  options, args = parser.parse_args()

  if len(args) != 3:
    parser.error('Expected 3 arguments: mode arch clib')

  # script + 3 args == 4
  mode, arch, clib = args
  if mode not in ('dbg', 'opt', 'coverage'):
    parser.error('Invalid mode %r' % mode)

  if arch not in ARCH_MAP:
    parser.error('Invalid arch %r' % arch)

  if clib not in ('newlib', 'glibc', 'pnacl'):
    parser.error('Invalid clib %r' % clib)

  # TODO(ncbray) allow a command-line override
  platform = GetHostPlatform()

  context['platform'] = platform
  context['mode'] = mode
  context['arch'] = arch
  # ASan is Clang, so set the flag to simplify other checks.
  context['clang'] = options.clang or options.asan
  context['validator'] = options.validator
  context['asan'] = options.asan
  # TODO(ncbray) turn derived values into methods.
  context['gyp_mode'] = {
      'opt': 'Release',
      'dbg': 'Debug',
      'coverage': 'Debug'}[mode]
  context['gyp_arch'] = ARCH_MAP[arch]['gyp_arch']
  context['gyp_vars'] = []
  if context['clang']:
    context['gyp_vars'].append('clang=1')
  if context['asan']:
    context['gyp_vars'].append('asan=1')
  context['default_scons_platform'] = ARCH_MAP[arch]['scons_platform']
  context['default_scons_mode'] = ['nacl']
  # Only Linux can build trusted code on ARM.
  # TODO(mcgrathr): clean this up somehow
  if arch != 'arm' or platform == 'linux':
    context['default_scons_mode'] += [mode + '-host']
  context['use_glibc'] = clib == 'glibc'
  context['pnacl'] = clib == 'pnacl'
  context['max_jobs'] = 8
  context['dry_run'] = options.dry_run
  context['inside_toolchain'] = options.inside_toolchain
  context['step_suffix'] = options.step_suffix
  context['no_gyp'] = options.no_gyp
  context['coverage'] = options.coverage
  context['use_breakpad_tools'] = options.use_breakpad_tools
  # Don't run gyp on coverage builds.
  if context['coverage']:
    context['no_gyp'] = True

  for key, value in sorted(context.config.items()):
    print '%s=%s' % (key, value)


def EnsureDirectoryExists(path):
  """
  Create a directory if it does not already exist.
  Does not mask failures, but there really shouldn't be any.
  """
  if not os.path.exists(path):
    os.makedirs(path)


def TryToCleanContents(path, file_name_filter=lambda fn: True):
  """
  Remove the contents of a directory without touching the directory itself.
  Ignores all failures.
  """
  if os.path.exists(path):
    for fn in os.listdir(path):
      TryToCleanPath(os.path.join(path, fn), file_name_filter)


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


def Retry(op, *args):
  # Windows seems to be prone to having commands that delete files or
  # directories fail.  We currently do not have a complete understanding why,
  # and as a workaround we simply retry the command a few times.
  # It appears that file locks are hanging around longer than they should.  This
  # may be a secondary effect of processes hanging around longer than they
  # should.  This may be because when we kill a browser sel_ldr does not exit
  # immediately, etc.
  # Virus checkers can also accidently prevent files from being deleted, but
  # that shouldn't be a problem on the bots.
  if GetHostPlatform() == 'win':
    count = 0
    while True:
      try:
        op(*args)
        break
      except Exception:
        print "FAILED: %s %s" % (op.__name__, repr(args))
        count += 1
        if count < 5:
          print "RETRY: %s %s" % (op.__name__, repr(args))
          time.sleep(pow(2, count))
        else:
          # Don't mask the exception.
          raise
  else:
    op(*args)


def _RemoveDirectory(path):
  print 'Removing %s' % path
  if os.path.exists(path):
    shutil.rmtree(path)
    print '    Succeeded.'
  else:
    print '    Path does not exist, nothing to do.'


def RemoveDirectory(path):
  """
  Remove a directory if it exists.
  Does not mask failures, although it does retry a few times on Windows.
  """
  Retry(_RemoveDirectory, path)


# This is a sanity check so Command can print out better error information.
def FileCanBeFound(name, paths):
  # CWD
  if os.path.exists(name):
    return True
  # Paths with directories are not resolved using the PATH variable.
  if os.path.dirname(name):
    return False
  # In path
  for path in paths.split(os.pathsep):
    full = os.path.join(path, name)
    if os.path.exists(full):
      return True
  return False


def RemoveGypBuildDirectories():
  # Remove all directories on all platforms.  Overkill, but it allows for
  # straight-line code.
  # Windows
  RemoveDirectory('build/Debug')
  RemoveDirectory('build/Release')
  RemoveDirectory('build/Debug-Win32')
  RemoveDirectory('build/Release-Win32')
  RemoveDirectory('build/Debug-x64')
  RemoveDirectory('build/Release-x64')

  # Linux and Mac
  RemoveDirectory('../xcodebuild')
  RemoveDirectory('../out')
  RemoveDirectory('src/third_party/nacl_sdk/arm-newlib')


def RemoveSconsBuildDirectories():
  RemoveDirectory('scons-out')
  RemoveDirectory('breakpad-out')


# Execute a command using Python's subprocess module.
def Command(context, cmd, cwd=None):
  print 'Running command: %s' % ' '.join(cmd)

  # Python's subprocess has a quirk.  A subprocess can execute with an
  # arbitrary, user-defined environment.  The first argument of the command,
  # however, is located using the PATH variable of the Python script that is
  # launching the subprocess.  Modifying the PATH in the environment passed to
  # the subprocess does not affect Python's search for the first argument of
  # the command (the executable file.)  This is a little counter intuitive,
  # so we're forcing the search to use the same PATH variable as is seen by
  # the subprocess.
  env = context.MakeCommandEnv()
  script_path = os.environ['PATH']
  os.environ['PATH'] = env['PATH']

  try:
    if FileCanBeFound(cmd[0], env['PATH']) or context['dry_run']:
      # Make sure that print statements before the subprocess call have been
      # flushed, otherwise the output of the subprocess call may appear before
      # the print statements.
      sys.stdout.flush()
      if context['dry_run']:
        retcode = 0
      else:
        retcode = subprocess.call(cmd, cwd=cwd, env=env)
    else:
      # Provide a nicer failure message.
      # If subprocess cannot find the executable, it will throw a cryptic
      # exception.
      print 'Executable %r cannot be found.' % cmd[0]
      retcode = 1
  finally:
    os.environ['PATH'] = script_path

  print 'Command return code: %d' % retcode
  if retcode != 0:
    raise StepFailed()
  return retcode


# A specialized version of CommandStep.
def SCons(context, mode=None, platform=None, parallel=False, browser_test=False,
          args=(), cwd=None):
  python = sys.executable
  if mode is None: mode = context['default_scons_mode']
  if platform is None: platform = context['default_scons_platform']
  if parallel:
    jobs = context['max_jobs']
  else:
    jobs = 1
  cmd = []
  if browser_test and context.Linux():
    # Although we could use the "browser_headless=1" Scons option, it runs
    # xvfb-run once per Chromium invocation.  This is good for isolating
    # the tests, but xvfb-run has a stupid fixed-period sleep, which would
    # slow down the tests unnecessarily.
    cmd.extend(['xvfb-run', '--auto-servernum'])
  cmd.extend([
      python, 'scons.py',
      '--verbose',
      '-k',
      '-j%d' % jobs,
      '--mode='+','.join(mode),
      'platform='+platform,
      ])
  if context['clang']: cmd.append('--clang')
  if context['asan']: cmd.append('--asan')
  if context['use_glibc']: cmd.append('--nacl_glibc')
  if context['pnacl']: cmd.append('bitcode=1')
  if context['use_breakpad_tools']:
    cmd.append('breakpad_tools_dir=breakpad-out')
  # Append used-specified arguments.
  cmd.extend(args)
  Command(context, cmd, cwd)


class StepFailed(Exception):
  """
  Thrown when the step has failed.
  """


class StopBuild(Exception):
  """
  Thrown when the entire build should stop.  This does not indicate a failure,
  in of itself.
  """


class Step(object):
  """
  This class is used in conjunction with a Python "with" statement to ensure
  that the preamble and postamble of each build step gets printed and failures
  get logged.  This class also ensures that exceptions thrown inside a "with"
  statement don't take down the entire build.
  """

  def __init__(self, name, status, halt_on_fail=True):
    self.status = status

    if 'step_suffix' in status.context:
      suffix = status.context['step_suffix']
    else:
      suffix = ''
    self.name = name + suffix
    self.halt_on_fail = halt_on_fail
    self.step_failed = False

  # Called on entry to a 'with' block.
  def __enter__(self):
    print
    print '@@@BUILD_STEP %s@@@' % self.name
    self.status.ReportBegin(self.name)

  # The method is called on exit from a 'with' block - even for non-local
  # control flow, i.e. exceptions, breaks, continues, returns, etc.
  # If an exception is thrown inside a block wrapped with a 'with' statement,
  # the __exit__ handler can suppress the exception by returning True.  This is
  # used to isolate each step in the build - if an exception occurs in a given
  # step, the step is treated as a failure.  This allows the postamble for each
  # step to be printed and also allows the build to continue of the failure of
  # a given step doesn't halt the build.
  def __exit__(self, type, exception, trace):
    if exception is None:
      # If exception is None, no exception occurred.
      step_failed = False
    elif isinstance(exception, StepFailed):
      step_failed = True
      print
      print 'Halting build step because of failure.'
      print
    else:
      step_failed = True
      print
      print 'The build step threw an exception...'
      print
      traceback.print_exception(type, exception, trace, file=sys.stdout)
      print

    if step_failed:
      self.status.ReportFail(self.name)
      print '@@@STEP_FAILURE@@@'
      if self.halt_on_fail:
        print
        print 'Entire build halted because %s failed.' % self.name
        raise StopBuild()
    else:
      self.status.ReportPass(self.name)

    # Suppress any exception that occurred.
    return True


# Adds an arbitrary link inside the build stage on the waterfall.
def StepLink(text, link):
  print '@@@STEP_LINK@%s@%s@@@' % (text, link)


# Adds arbitrary text inside the build stage on the waterfall.
def StepText(text):
  print '@@@STEP_TEXT@%s@@@' % (text)


class BuildStatus(object):
  """
  Keeps track of the overall status of the build.
  """

  def __init__(self, context):
    self.context = context
    self.ever_failed = False
    self.steps = []

  def ReportBegin(self, name):
    pass

  def ReportPass(self, name):
    self.steps.append((name, 'passed'))

  def ReportFail(self, name):
    self.steps.append((name, 'failed'))
    self.ever_failed = True

  # Handy info when this script is run outside of the buildbot.
  def DisplayBuildStatus(self):
    print
    for step, status in self.steps:
      print '%-40s[%s]' % (step, status)
    print

    if self.ever_failed:
      print 'Build failed.'
    else:
      print 'Build succeeded.'

  def ReturnValue(self):
    return int(self.ever_failed)


class BuildContext(object):
  """
  Encapsulates the information needed for running a build command.  This
  includes environment variables and default arguments for SCons invocations.
  """

  # Only allow these attributes on objects of this type.
  __slots__ = ['status', 'global_env', 'config']

  def __init__(self):
    # The contents of global_env override os.environ for any commands run via
    # self.Command(...)
    self.global_env = {}
    # PATH is a special case. See: Command.
    self.global_env['PATH'] = os.environ.get('PATH', '')

    self.config = {}
    self['dry_run'] = False

  # Emulate dictionary subscripting.
  def __getitem__(self, key):
    return self.config[key]

  # Emulate dictionary subscripting.
  def __setitem__(self, key, value):
    self.config[key] = value

  # Emulate dictionary membership test
  def __contains__(self, key):
    return key in self.config

  def Windows(self):
    return self.config['platform'] == 'win'

  def Linux(self):
    return self.config['platform'] == 'linux'

  def Mac(self):
    return self.config['platform'] == 'mac'

  def GetEnv(self, name):
    return self.global_env[name]

  def SetEnv(self, name, value):
    self.global_env[name] = str(value)

  def MakeCommandEnv(self):
    # The external environment is not sanitized.
    e = dict(os.environ)
    # Arbitrary variables can be overridden.
    e.update(self.global_env)
    return e


def RunBuild(script, status):
  try:
    script(status, status.context)
  except StopBuild:
    pass

  # Workaround for an annotator bug.
  # TODO(bradnelson@google.com) remove when the bug is fixed.
  if status.ever_failed:
    with Step('summary', status):
        print 'There were failed stages.'

  # Display a summary of the build.  Useful when running outside the buildbot.
  status.DisplayBuildStatus()
  sys.exit(status.ReturnValue())
