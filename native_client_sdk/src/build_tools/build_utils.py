# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Small utility library of python functions used by the various package
installers.
"""

import datetime
import errno
import fileinput
import os
import platform
import re
import shutil
import subprocess
import sys

from nacl_sdk_scons import nacl_utils


def ChromeMilestone():
  '''Extract chrome_milestone from src/DEPS

  Returns:
    Chrome milestone variable value from src/DEPS.
  '''
  parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
  def From(x, y):
    return ''
  def Var(x):
    return ''
  def File(x):
    return ''
  with open(os.path.join(parent_dir, 'DEPS'), 'r') as fh:
    exec(fh.read())
  return vars.get('chrome_milestone')


#------------------------------------------------------------------------------
# Parameters

# Revision numbers for the SDK
PLATFORM_VERSION = 'pepper_' + ChromeMilestone()

TOOLCHAIN_AUTODETECT = "AUTODETECT"

#------------------------------------------------------------------------------
# Functions

# Make all the directories in |abs_path|.  If |abs_path| points to a regular
# file, it is removed before an attempt to make the directories.  If |abs_path|
# already points to a directory, this method does nothing.
def ForceMakeDirs(abs_path, mode=0755):
  if os.path.isdir(abs_path):
    return
  try:
    # Remove an existing regular file; ignore errors (e.g. file doesn't exist).
    # If there are permission problems, they will be caught by the exception
    # handler around the os.makedirs call.
    os.remove(abs_path)
  except:
    pass
  try:
    os.makedirs(abs_path, mode)
  except OSError, (os_errno, os_strerr):
    # If the error is anything but EEXIST (file already exists), then print an
    # informative message and re-raise.  It is not and error if the directory
    # already exists.
    if os_errno != errno.EEXIST:
      print 'ForceMakeDirs(%s, 0%o) FAILED: %s' % (abs_path, mode, os_strerr)
      raise
    pass


# patch version 2.6 doesn't work.  Most of our Linux distros use patch 2.6.
# Returns |True| if the version of patch is usable (that is, not version 2.6).
# |shell_env| is the enviromnent used to run the subprocesses like patch and
# sed.  If |shell_env| is None, then os.environ is used.
def CheckPatchVersion(shell_env=None):
  if shell_env is None:
    shell_env = os.environ
  patch = subprocess.Popen("patch --version",
                            shell=True,
                            env=shell_env,
                            stdout=subprocess.PIPE)
  sed = subprocess.Popen("sed q",
                         shell=True,
                         env=shell_env,
                         stdin=patch.stdout,
                         stdout=subprocess.PIPE)
  sed_output = sed.communicate()[0]
  if sed_output.strip() == 'patch 2.6':
    print "patch 2.6 is incompatible with these scripts."
    print "Please install either version 2.5.9 (or earlier)"
    print "or version 2.6.1 (or later)."
    return False
  return True


# Build a toolchain path based on the platform type.  |base_dir| is the root
# directory which includes the platform-specific toolchain.  This could be
# something like "/usr/local/mydir/nacl_sdk/src".  If |base_dir| is None, then
# the environment variable NACL_SDK_ROOT is used (if it's set).
# This method assumes that the platform-specific toolchain is found under
# <base_dir>/toolchain/<platform_variant>.
def NormalizeToolchain(toolchain=TOOLCHAIN_AUTODETECT,
                       base_dir=None,
                       arch=nacl_utils.DEFAULT_TOOLCHAIN_ARCH,
                       variant=nacl_utils.DEFAULT_TOOLCHAIN_VARIANT):
  if toolchain == TOOLCHAIN_AUTODETECT:
    if base_dir is None:
      base_dir = os.getenv('NACL_SDK_ROOT', '')
    normalized_toolchain = nacl_utils.ToolchainPath(base_dir=base_dir,
                                                    arch=arch,
                                                    variant=variant)
  else:
    normalized_toolchain = os.path.abspath(toolchain)
  return normalized_toolchain


def SupportedNexeBitWidths():
  '''Return a list of .nexe bit widths that are supported by the host.

  Each supported bit width means the host can run a .nexe with the corresponding
  instruction set architecture.  For example, if this function returns the
  list [32, 64], then the host can run both 32- and 64-bit .nexes.

  Note: on Windows, environment variables are used to determine the host's bit
  width instead of the |platform| package.  This is because (up until python
  2.7) the |platform| package returns the bit-width used to build python, not
  the host's bit width.

  Returns: A list of supported nexe word widths (in bits) supported by the host
      (typically 32 or 64).  Returns an empty list if the word_width cannot be
      determined.
  '''
  bit_widths = []
  if sys.platform == 'win32':
    # On Windows, the best way to detect the word size is to look at these
    # env vars.  python 2.6 and earlier on Windows (in particular the
    # python on the Windows bots) generally always say they are 32-bits,
    # even though the host is 64-bits.  See this thread for more:
    # http://stackoverflow.com/questions/7164843/
    #     in-python-how-do-you-determine-whether-the-kernel-is-running-in-\
    #     32-bit-or-64-bit
    if ('64' in os.environ.get('PROCESSOR_ARCHITECTURE', '') or
        '64' in os.environ.get('PROCESSOR_ARCHITEW6432', '')):
      bit_widths = [64]
    else:
      bit_widths = [32]
  elif sys.platform == 'darwin':
    # Mac can handle 32- and 64-bit .nexes.
    bit_widths = [32, 64]
  else:
    # Linux 64 can handle both 32- and 64-bit.
    machine = platform.machine()
    bit_widths = [32, 64] if '64' in machine else [32]

  return bit_widths


def RawVersion():
  '''Returns the Raw version number of the SDK in a dotted format'''
  return '.'.join(GetVersionNumbers())


def GetVersionNumbers():
  '''Returns a list of 3 strings containing the version identifier'''
  rev = str(SVNRevision())
  return [PLATFORM_VERSION, rev]


def SVNRevision():
  '''Returns the Subversion revision of this file.

  This file either needs to be in either a subversion repository or
  a git repository that is sync'd to a subversion repository using git-svn.'''
  run_path = os.path.dirname(os.path.abspath(__file__))
  p = subprocess.Popen('svn info', shell=True, stdout=subprocess.PIPE,
                       cwd=run_path)
  if p.wait() != 0:
    p = subprocess.Popen('git svn info', shell=True, stdout=subprocess.PIPE,
                         cwd=run_path)
    if p.wait() != 0:
      raise AssertionError('Cannot determine SVN revision of this repository');

  svn_info = p.communicate()[0]
  m = re.search('Revision: ([0-9]+)', svn_info)
  if m:
    return int(m.group(1))
  else:
    raise AssertionError('Cannot extract revision number from svn info')


def VersionString():
  '''Returns the version of native client based on the svn revision number.'''
  return 'native_client_sdk_%s' % '_'.join(GetVersionNumbers())


def JoinPathToNaClRepo(*args, **kwargs):
  '''Use os.path.join() to join the argument list to the NaCl repo location.

  Assumes that the Native Client repo is DEPSed into this repo under
  third_party/native_client.  This has to match the target dirs in the DEPS
  file.

  If the key 'root_dir' is set, then this path is prepended to the NaCl repo
  path.

  Args:
    args: A list of path elements to append to the NaCl repo root.
    kwargs: If the 'root_dir' key is present, this gets prepended to the
        final path.

  Return: An OS-native path to the DEPSed in root of the NaCl repo.
  '''
  nacl_path = os.path.join('third_party', 'native_client', *args)
  root_path = kwargs.get('root_dir')
  return os.path.join(root_path, nacl_path) if root_path else nacl_path


class BotAnnotator:
  '''Interface to Bot Annotations

  See http://www.chromium.org/developers/testing/chromium-build-infrastructure/buildbot-annotations
  '''

  def __init__(self, stream=sys.stdout):
    self._stream = stream

  def Print(self, message):
    '''Display a message to the output stream and flush so the bots see it'''
    self._stream.write("%s\n" % message)
    self._stream.flush()

  def BuildStep(self, name):
    self.Print("BUILD_STEP %s" % name)
    # mball: Disabling buildbot annotations because it's more confusing than
    #   useful when running multiple jobs simultaneously.  To re-enable
    #   annotations, using the following line instead of the previous:
    # self.Print("@@@BUILD_STEP %s@@@" % name)

  def BuildStepFailure(self):
    '''Signal a failure in the current build step to the annotator'''
    self.Print("@@@STEP_FAILURE@@@")

  def Run(self, *popenargs, **kwargs):
    '''Implements the functionality of subprocess.check_output, but also
    prints out the command-line and the command output.

    Do not set stdout to anything because this function will redirect it
    using a pipe.

    Arguments:
      See subprocess.Popen

    returns:
      a string containing the command output
    '''
    if 'stdout' in kwargs or 'stderr' in kwargs:
      raise ValueError('stdout or stderr argument not allowed.')
    command = kwargs.get("args")
    if command is None:
      command = popenargs[0]
    self.Print('Running %s' % command)
    process = subprocess.Popen(stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE,
                               *popenargs,
                               **kwargs)
    output, error_output = process.communicate()
    if error_output:
      self.Print("%s\nStdErr for %s:\n%s" % (output, command, error_output))
    else:
      self.Print(output)

    retcode = process.poll() # Note - calling wait() can cause a deadlock
    if retcode != 0:
      raise subprocess.CalledProcessError(retcode, command)
    return output

  #TODO(mball) Add the other possible build annotations, as needed


def UpdateReadMe(filename):
  '''Updates the README file in the SDK with the current date and version'''

  for line in fileinput.input(filename, inplace=1):
    sys.stdout.write(line.replace('${VERSION}', PLATFORM_VERSION)
                     .replace('${REVISION}', str(SVNRevision()))
                     .replace('${DATE}', str(datetime.date.today())))

def CleanDirectory(dir):
  '''Cleans all the contents of a given directory.
  This works even when there are Windows Junctions in the directory

  Args:
    dir: The directory to clean
  '''
  if sys.platform != 'win32':
    shutil.rmtree(dir, ignore_errors=True)
  else:
    # Intentionally ignore return value since a directory might be in use.
    subprocess.call(['rmdir', '/Q', '/S', dir],
                    env=os.environ.copy(),
                    shell=True)
