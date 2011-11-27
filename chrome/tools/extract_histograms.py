#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Extract UMA histogram strings from the Chrome source.

This program generates the list of known histograms we expect to see in
the user behavior logs.  It walks the Chrome source, looking for calls
to UMA histogram macros.

Run it from the chrome/browser directory like:
  extract_histograms.py > histogram_list
"""

# TODO(evanm): get all the jankometer histogram names.

__author__ = 'evanm (Evan Martin)'

import os
import re
import sys

def GrepForHistograms(path, histograms):
  """Grep a source file for calls to histogram macros functions.

  Arguments:
    path: path to the file
    histograms: set of histograms to add to
  """

  histogram_re = re.compile(r'HISTOGRAM_\w+\(L"(.*)"')
  for line in open(path):
    match = histogram_re.search(line)
    if match:
      histograms.add(match.group(1))


def WalkDirectory(root_path, histograms):
  for path, dirs, files in os.walk(root_path):
    if '.svn' in dirs:
      dirs.remove('.svn')
    for file in files:
      ext = os.path.splitext(file)[1]
      if ext == '.cc':
        GrepForHistograms(os.path.join(path, file), histograms)


def main(argv):
  histograms = set()

  # Walk the source tree to process all .cc files.
  WalkDirectory('..', histograms)

  # Print out the histograms as a sorted list.
  for histogram in sorted(histograms):
    print histogram
  return 0


if '__main__' == __name__:
  sys.exit(main(sys.argv))
