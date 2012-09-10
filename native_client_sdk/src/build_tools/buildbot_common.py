# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common utilities for all buildbot scripts that specifically don't rely
on having a full chromium checkout.
"""

import os
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SDK_SRC_DIR = os.path.dirname(SCRIPT_DIR)
SDK_DIR = os.path.dirname(SDK_SRC_DIR)
SRC_DIR = os.path.dirname(SDK_DIR)
sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))

import oshelpers

def IsSDKBuilder():
  """Returns True if this script is running on an SDK builder.

  False means it is either running on a trybot, or a user's machine.

  Trybot names:
    naclsdkm-((pnacl-)?linux|mac|windows(32|64))

  Builder names:
    (pnacl-)?(windows|mac|linux)-sdk-multi(rel)?

    except there are currently no pnacl multirel bots, and
    pnacl-windows-sdk-multi is actually called pnacl-win-sdk-multi."""
  return '-sdk-multi' in os.getenv('BUILDBOT_BUILDERNAME', '')


def ErrorExit(msg):
  """Write and error to stderr, then exit with 1 signaling failure."""
  sys.stderr.write(msg + '\n')
  sys.exit(1)


def BuildStep(name):
  """Annotate a buildbot build step."""
  sys.stdout.flush()
  print '\n@@@BUILD_STEP %s@@@' % name
  sys.stdout.flush()


def Run(args, cwd=None, env=None, shell=False):
  """Start a process with the provided arguments.
  
  Starts a process in the provided directory given the provided arguments. If
  shell is not False, the process is launched via the shell to provide shell
  interpretation of the arguments.  Shell behavior can differ between platforms
  so this should be avoided when not using platform dependent shell scripts."""
  print 'Running: ' + ' '.join(args)
  sys.stdout.flush()
  sys.stderr.flush()
  subprocess.check_call(args, cwd=cwd, env=env, shell=shell)
  sys.stdout.flush()
  sys.stderr.flush()


def CopyDir(src, dst, excludes=('.svn', '*/.svn')):
  """Recursively copy a directory using."""
  args = ['-r', src, dst]
  for exc in excludes:
    args.append('--exclude=' + exc)
  print 'cp -r %s %s' % (src, dst)
  if os.path.abspath(src) == os.path.abspath(dst):
    ErrorExit('ERROR: Copying directory onto itself: ' + src)
  oshelpers.Copy(args)


def CopyFile(src, dst):
  print 'cp -r %s %s' % (src, dst)
  if os.path.abspath(src) == os.path.abspath(dst):
    ErrorExit('ERROR: Copying file onto itself: ' + src)
  args = [src, dst]
  oshelpers.Copy(args)


def RemoveDir(dst):
  """Remove the provided path."""
  print 'rm -fr ' + dst
  oshelpers.Remove(['-fr', dst])


def MakeDir(dst):
  """Create the path including all parent directories as needed."""
  print 'mkdir -p ' + dst
  oshelpers.Mkdir(['-p', dst])


def Move(src, dst):
  """Move the path src to dst."""
  print 'mv -f %s %s' % (src, dst)
  oshelpers.Move(['-f', src, dst])


def RemoveFile(dst):
  """Remove the provided file."""
  print 'rm ' + dst
  oshelpers.Remove(['-f', dst])


BOT_GSUTIL = '/b/build/scripts/slave/gsutil'
LOCAL_GSUTIL = 'gsutil'


def GetGsutil():
  if os.environ.get('BUILDBOT_BUILDERNAME'):
    return BOT_GSUTIL
  else:
    return LOCAL_GSUTIL


def Archive(filename, bucket_path, cwd=None, step_link=True):
  """Upload the given filename to Google Store."""
  full_dst = 'gs://%s/%s' % (bucket_path, filename)

  subprocess.check_call(
      '%s cp -a public-read %s %s' % (GetGsutil(), filename, full_dst),
      shell=True,
      cwd=cwd)
  url = 'https://commondatastorage.googleapis.com/'\
        '%s/%s' % (bucket_path, filename)
  if step_link:
    print '@@@STEP_LINK@download@%s@@@' % url
    sys.stdout.flush()
