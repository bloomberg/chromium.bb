#!/usr/bin/env python
#
# Copyright 2012 Google Inc. All Rights Reserved.

import os
import re
import shutil
import subprocess
import sys
import update_patched_files
import urllib


def GetLkgr():
  f = urllib.urlopen('http://chromium-status.appspot.com/lkgr')
  try:
    return int(f.read())
  finally:
    f.close()


def ReadDepsVars(path):
  exec_globals = {
    'Var': lambda name: exec_globals['vars'][name],
  }
  execfile(path, exec_globals)
  return exec_globals['vars']


def GetRevision(path, name):
  return int(ReadDepsVars(path)[name])


def main(argv):
  CHROMIUM_DEPS_FILE = 'DEPS'
  DARTIUM_DEPS_FILE = '../dartium.deps/DEPS'
  CHROMIUM_DEPS_COPY = '../dartium.deps/DEPS.chromium'
  REV_PATTERN = '"chromium_revision": "(\d+)",'

  deps = file(DARTIUM_DEPS_FILE).read()
  current_chrome_rev = int(re.search(REV_PATTERN, deps).group(1))

  if len(argv) < 2:
    next_chrome_rev = GetLkgr()
  else:
    next_chrome_rev = int(argv[1])

  print 'Chromium roll: %d -> %d' % (current_chrome_rev, next_chrome_rev)

  if current_chrome_rev == next_chrome_rev:
    return

  # Update patched files.
  os.chdir('..')
  update_patched_files.update_overridden_files(current_chrome_rev, next_chrome_rev)
  os.chdir('src')

  # Update DEPS.
  subprocess.check_call(['svn', 'up', '-r', str(current_chrome_rev), CHROMIUM_DEPS_FILE])
  current_webkit_rev = GetRevision(CHROMIUM_DEPS_FILE, 'webkit_revision')
  subprocess.check_call(['svn', 'up', '-r', str(next_chrome_rev), CHROMIUM_DEPS_FILE])
  next_webkit_rev = GetRevision(CHROMIUM_DEPS_FILE, 'webkit_revision')

  shutil.copyfile(CHROMIUM_DEPS_FILE, CHROMIUM_DEPS_COPY)
  deps = deps.replace('"chromium_revision": "%d",' % current_chrome_rev, '"chromium_revision": "%d",' % next_chrome_rev)
  file(DARTIUM_DEPS_FILE, 'w').write(deps)

  # Do webkit roll.
  WEBKIT_DIR = 'third_party/WebKit'
  subprocess.check_call(['git', 'svn', 'rebase'], cwd=WEBKIT_DIR)
  print 'WebKit roll: %d -> %d' % (current_webkit_rev, next_webkit_rev)

  if current_webkit_rev < next_webkit_rev:
    subprocess.check_call(['bash',
          '../../dartium_tools/roll_webkit.sh',
          str(current_webkit_rev), str(next_webkit_rev)], cwd=WEBKIT_DIR)

  # Update the checkout.
  subprocess.check_call(['gclient', 'sync', '-j17'])



if __name__ == '__main__':
  sys.exit(main(sys.argv))
