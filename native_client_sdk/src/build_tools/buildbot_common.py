# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common utilities for all buildbot scripts that specifically don't rely
on having a full chromium checkout.
"""

import os
import subprocess
import sys

from build_paths import SDK_SRC_DIR, NACL_DIR

sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))
import oshelpers
import getos


verbose = True


def IsSDKBuilder():
  """Returns True if this script is running on an SDK builder.

  False means it is either running on a trybot, or a user's machine.

  Trybot names:
    (win|mac|linux)_nacl_sdk

  Build-only Trybot names:
    (win|mac|linux)_nacl_sdk_build

  Builder names:
    (windows|mac|linux)-sdk-multi(bionic)(rel)?"""
  bot =  os.getenv('BUILDBOT_BUILDERNAME', '')
  return '-sdk-multi' in bot or '-sdk-bionic-multi' in bot


def IsSDKTrybot():
  """Returns True if this script is running on an SDK trybot.

  False means it is either running on an SDK builder, or a user's machine.

  See IsSDKBuilder above for trybot/buildbot names."""
  return '_nacl_sdk' in os.getenv('BUILDBOT_BUILDERNAME', '')


def ErrorExit(msg):
  """Write and error to stderr, then exit with 1 signaling failure."""
  sys.stderr.write(str(msg) + '\n')
  sys.exit(1)


def Trace(msg):
  if verbose:
    sys.stderr.write(str(msg) + '\n')


def GetWindowsEnvironment():
  sys.path.append(os.path.join(NACL_DIR, 'buildbot'))
  import buildbot_standard

  # buildbot_standard.SetupWindowsEnvironment expects a "context" object. We'll
  # fake enough of that here to work.
  class FakeContext(object):
    def __init__(self):
      self.env = os.environ

    def GetEnv(self, key):
      return self.env[key]

    def __getitem__(self, key):
      # The nacl side script now needs gyp_vars to return a list.
      if key == 'gyp_vars':
        return []
      return self.env[key]

    def SetEnv(self, key, value):
      self.env[key] = value

    def __setitem__(self, key, value):
      self.env[key] = value

  context = FakeContext()
  buildbot_standard.SetupWindowsEnvironment(context)

  # buildbot_standard.SetupWindowsEnvironment adds the directory which contains
  # vcvarsall.bat to the path, but not the directory which contains cl.exe,
  # link.exe, etc.
  # Running vcvarsall.bat adds the correct directories to the path, which we
  # extract below.
  process = subprocess.Popen('vcvarsall.bat x86 > NUL && set',
      stdout=subprocess.PIPE, env=context.env, shell=True)
  stdout, _ = process.communicate()

  # Parse environment from "set" command above.
  # It looks like this:
  # KEY1=VALUE1\r\n
  # KEY2=VALUE2\r\n
  # ...
  return dict(line.split('=', 1) for line in stdout.split('\r\n')[:-1])


def BuildStep(name):
  """Annotate a buildbot build step."""
  sys.stdout.flush()
  sys.stderr.write('\n@@@BUILD_STEP %s@@@\n' % name)


def Run(args, cwd=None, env=None, shell=False):
  """Start a process with the provided arguments.

  Starts a process in the provided directory given the provided arguments. If
  shell is not False, the process is launched via the shell to provide shell
  interpretation of the arguments.  Shell behavior can differ between platforms
  so this should be avoided when not using platform dependent shell scripts."""

  # We need to modify the environment to build host on Windows.
  if not env and getos.GetPlatform() == 'win':
    env = GetWindowsEnvironment()

  Trace('Running: ' + ' '.join(args))
  sys.stdout.flush()
  sys.stderr.flush()
  try:
    subprocess.check_call(args, cwd=cwd, env=env, shell=shell)
  except subprocess.CalledProcessError as e:
    sys.stdout.flush()
    sys.stderr.flush()
    ErrorExit('buildbot_common: %s' % e)

  sys.stdout.flush()
  sys.stderr.flush()


def CopyDir(src, dst, excludes=('.svn', '*/.svn')):
  """Recursively copy a directory using."""
  args = ['-r', src, dst]
  for exc in excludes:
    args.append('--exclude=' + exc)
  Trace('cp -r %s %s' % (src, dst))
  if os.path.abspath(src) == os.path.abspath(dst):
    ErrorExit('ERROR: Copying directory onto itself: ' + src)
  oshelpers.Copy(args)


def CopyFile(src, dst):
  Trace('cp %s %s' % (src, dst))
  if os.path.abspath(src) == os.path.abspath(dst):
    ErrorExit('ERROR: Copying file onto itself: ' + src)
  args = [src, dst]
  oshelpers.Copy(args)


def RemoveDir(dst):
  """Remove the provided path."""
  Trace('rm -fr ' + dst)
  oshelpers.Remove(['-fr', dst])


def MakeDir(dst):
  """Create the path including all parent directories as needed."""
  Trace('mkdir -p ' + dst)
  oshelpers.Mkdir(['-p', dst])


def Move(src, dst):
  """Move the path src to dst."""
  Trace('mv -f %s %s' % (src, dst))
  oshelpers.Move(['-f', src, dst])


def RemoveFile(dst):
  """Remove the provided file."""
  Trace('rm ' + dst)
  oshelpers.Remove(['-f', dst])


BOT_GSUTIL = '/b/build/scripts/slave/gsutil'
# On Windows, the current working directory may be on a different drive than
# gsutil.
WIN_BOT_GSUTIL = 'E:' + BOT_GSUTIL
LOCAL_GSUTIL = 'gsutil'


def GetGsutil():
  if os.environ.get('BUILDBOT_BUILDERNAME') \
     and not os.environ.get('BUILDBOT_FAKE'):
    if getos.GetPlatform() == 'win':
      return WIN_BOT_GSUTIL
    return BOT_GSUTIL
  else:
    return LOCAL_GSUTIL


def Archive(filename, bucket_path, cwd=None, step_link=True):
  """Upload the given filename to Google Store."""
  full_dst = 'gs://%s/%s' % (bucket_path, filename)

  # Since GetGsutil() might just return 'gsutil' and expect it to be looked
  # up in the PATH, we must pass shell=True on windows.
  # Without shell=True the windows implementation of subprocess.call will not
  # search the PATH for the executable: http://bugs.python.org/issue8557
  shell = getos.GetPlatform() == 'win'

  cmd = [GetGsutil(), 'cp', '-a', 'public-read', filename, full_dst]
  Run(cmd, shell=shell, cwd=cwd)
  url = 'https://storage.googleapis.com/%s/%s' % (bucket_path, filename)
  if step_link:
    sys.stdout.flush()
    sys.stderr.write('@@@STEP_LINK@download@%s@@@\n' % url)
