#!/usr/bin/python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper for trychange.py for WebKit changes."""

import errno
import logging
import os
import sys
import tempfile

import git_cl
from scm import GIT
import trychange


class ScopedTemporaryFile(object):
  def __init__(self, **kwargs):
    file_handle, self._path = tempfile.mkstemp(**kwargs)
    os.close(file_handle)

  def __enter__(self):
    return self._path

  def __exit__(self, _exc_type, _exc_value, _exc_traceback):
    try:
      os.remove(self._path)
    except OSError, e:
      if e.errno != errno.ENOENT:
        raise e


def generateDiff(path_to_write, chromium_src_root):
  """Create the diff file to send to the try server. We prepend the WebKit
  revision to the beginning of the diff so we can patch against ToT or other
  versions of WebKit."""
  diff_lines = ['third_party/WebKit@HEAD\n']

  raw_diff = GIT.GenerateDiff(os.path.join(chromium_src_root,
      'third_party/WebKit'), full_move=True).splitlines(True)
  diff_lines.extend(trychange.GetMungedDiff('third_party/WebKit',
      raw_diff)[0])

  open(path_to_write, 'wb').write(''.join(diff_lines))


def addLayoutBotsIfNeeded(argv):
  for flag in argv:
    if flag == '--bot' or flag.startswith('--bot=') or flag.startswith('-b'):
      return argv
  argv.extend(('--bot', 'linux_layout_rel,mac_layout_rel,win_layout_rel'))


def chromiumSrcRoot():
  root = GIT.GetCheckoutRoot('.')
  parent_path, leaf_path = os.path.split(root)
  if leaf_path == 'WebKit':
    root = GIT.GetCheckoutRoot(parent_path)
  return root


def main(argv):
  chromium_src_root = chromiumSrcRoot()
  os.chdir(chromium_src_root)
  argv = argv[1:]
  addLayoutBotsIfNeeded(argv)

  with ScopedTemporaryFile() as diff_file:
    generateDiff(diff_file, chromium_src_root)
    args = [
        '--sub_rep', 'third_party/WebKit',
        '--root', 'src',
        '--rietveld_url', 'https://codereview.chromium.org',
        '--diff', diff_file,
    ]
    args.extend(argv)
    cl = git_cl.Changelist()
    change = cl.GetChange(cl.GetUpstreamBranch(), None)
    logging.getLogger().handlers = []
    return trychange.TryChange(args, change,
        swallow_exception=False, prog='git wktry')


if __name__ == '__main__':
  sys.exit(main(sys.argv))
