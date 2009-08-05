#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script helps workaround IncrediBuild problem on Windows.
# See http://crbug.com/17706.

import os
import sys

_SRC_PATH = os.path.join(os.path.dirname(__file__), '..', '..')

sys.path.append(os.path.join(_SRC_PATH, 'tools', 'grit'))
import grit.exception
import grit.grd_reader

# We need to apply the workaround only on Windows.
if os.name != 'nt':
  sys.exit(0)

def total_split(path):
  components = []
  while path:
    head, tail = os.path.split(path)
    if not tail:
      break
    components.append(tail)
    path = head
  return list(reversed(components))

for path in sys.argv[1:]:
  path = os.path.join('src', path)
  path_components = total_split(path)
  try:
    root = grit.grd_reader.Parse(path)
  except grit.exception.Base, exc:
    # This hook exploded badly a few times on the buildbot with exception
    # at this point. Do not exit with an error, just print more information
    # for debugging.
    # TODO(phajdan.jr): Make exception fatal when the root cause is fixed.
    print 'Unexpected GRIT exception while processing ' + path
    print exc
    continue
  output_files = [node.GetOutputFilename() for node in root.GetOutputFiles()]
  output_headers = [file for file in output_files if file.endswith('.h')]
  for build_type in ('Debug', 'Release'):
    build_path = os.path.join(_SRC_PATH, 'chrome', build_type)

    # We guess target file output based on path of the grd file (the first
    # path component after 'src').
    intermediate_path = os.path.join(build_path, 'obj',
                                     'global_intermediate', path_components[1])

    for header in output_headers:
      full_path = os.path.join(intermediate_path, header)
      try:
        os.remove(full_path)
        print 'Clobbered ' + full_path
      except OSError:
        print 'Could not remove ' + full_path + '. Continuing.'
