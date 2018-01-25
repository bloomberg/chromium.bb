#!/usr/bin/python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used to update our local AFDO profiles.

This uses profiles of Chrome provided by our friends from Chrome OS. Though the
profiles are available externally, the bucket they sit in is otherwise
unreadable by non-Googlers. Gsutil usage with this bucket is therefore quite
awkward: you can't do anything but `cp` certain files with an external account,
and you can't even do that if you're not yet authenticated.

No authentication is necessary if you pull these profiles directly over
https."""

import argparse
import os
import subprocess
import sys
import urllib2

GS_BASE_URL = 'https://storage.googleapis.com/chromeos-prebuilt/afdo-job/llvm'
PROFILE_DIRECTORY = os.path.abspath(os.path.dirname(__file__))
LOCAL_PROFILE_PATH = os.path.join(PROFILE_DIRECTORY, 'afdo.prof')

# We use these to track the local profile; newest.txt is owned by git and tracks
# the name of the newest profile we should pull, and local.txt is the most
# recent profile we've successfully pulled.
NEWEST_PROFILE_NAME_PATH = os.path.join(PROFILE_DIRECTORY, 'newest.txt')
LOCAL_PROFILE_NAME_PATH = os.path.join(PROFILE_DIRECTORY, 'local.txt')


def ReadUpToDateProfileName():
  with open(NEWEST_PROFILE_NAME_PATH) as f:
    return f.read().strip()


def ReadLocalProfileName():
  try:
    with open(LOCAL_PROFILE_NAME_PATH) as f:
      return f.read().strip()
  except IOError:
    # Assume it either didn't exist, or we couldn't read it. In either case, we
    # should probably grab a new profile (and, in doing so, make this file sane
    # again)
    return None


def WriteLocalProfileName(name):
  with open(LOCAL_PROFILE_NAME_PATH, 'w') as f:
    f.write(name)


def CheckCallOrExit(cmd):
  proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  stdout, stderr = proc.communicate()
  exit_code = proc.wait()
  if not exit_code:
    return

  complaint_lines = [
      '## %s failed with exit code %d' % (cmd[0], exit_code),
      '## Full command: %s' % cmd,
      '## Stdout:\n' + stdout,
      '## Stderr:\n' + stderr,
  ]
  print >>sys.stderr, '\n'.join(complaint_lines)
  sys.exit(1)


def RetrieveProfile(desired_profile_name, out_path):
  # vpython is > python 2.7.9, so we can expect urllib to validate HTTPS certs
  # properly.
  compressed_path = out_path + '.bz2'
  u = urllib2.urlopen(GS_BASE_URL + '/' + desired_profile_name)
  with open(compressed_path, 'wb') as f:
    while True:
      buf = u.read(4096)
      if not buf:
        break
      f.write(buf)

  # NOTE: we can't use Python's bzip module, since it doesn't support
  # multi-stream bzip files. It will silently succeed and give us a garbage
  # profile.
  # bzip2 removes the compressed file on success.
  CheckCallOrExit(['bzip2', '-d', compressed_path])


def CleanProfilesDirectory():
  # Start with a clean slate, removing old profiles/downloads/etc.
  old_artifacts = (p for p in os.listdir(PROFILE_DIRECTORY) if
                   p.startswith('chromeos-chrome-'))
  for artifact in old_artifacts:
    os.remove(os.path.join(PROFILE_DIRECTORY, artifact))


def main():
  parser = argparse.ArgumentParser('Downloads profiles provided by Chrome OS')
  parser.add_argument('-f', '--force', action='store_true',
                      help='Fetch a profile even if the local one is current')
  args = parser.parse_args()

  up_to_date_profile = ReadUpToDateProfileName()
  if not args.force:
    local_profile_name = ReadLocalProfileName()
    # In a perfect world, the local profile should always exist if we
    # successfully read local_profile_name. If it's gone, though, the user
    # probably removed it as a way to get us to download it again.
    if local_profile_name == up_to_date_profile \
        and os.path.exists(LOCAL_PROFILE_PATH):
      return 0

  CleanProfilesDirectory()

  new_tmpfile = LOCAL_PROFILE_PATH + '.new'
  RetrieveProfile(up_to_date_profile, new_tmpfile)
  os.rename(new_tmpfile, LOCAL_PROFILE_PATH)
  WriteLocalProfileName(up_to_date_profile)


if __name__ == '__main__':
  sys.exit(main())
