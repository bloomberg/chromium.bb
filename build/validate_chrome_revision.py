#!/usr/bin/python
# Copyright 2011 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

""" Check that the chrome_rev in DEPS has binaries for every platform.
"""

import sys
import chromebinaries


errmsg = """Chrome binaries are not available on every platform for revision %d.
  Try running find_chrome_revisions.py to find a better revision.
  The most recent usable revision is %d - check if it's green."""


def ValidateChromeRevision(path):
  rev = int(chromebinaries.GetChromeRevision(path=path))
  revs = chromebinaries.GetCommonRevisions(min_rev=None, max_rev=None,
                                           verbose=False)
  if rev not in revs:
    return errmsg % (rev, max(revs))
  else:
    return None


def Main():
  msg = ValidateChromeRevision(None)
  if msg is not None:
    print msg
    sys.exit(-1)


if __name__ == '__main__':
  Main()