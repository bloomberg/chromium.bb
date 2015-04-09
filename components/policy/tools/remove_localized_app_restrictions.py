#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''python %prog resources_dir

resources_dir specifies the root directory of the localized app restrictions
resources to purge'''

import os
import re
import sys

def main(resources_dir):
  match = re.compile('^values-.*-v21$')
  for dir in os.listdir(resources_dir):
    if match.search(dir):
      path = os.path.join(resources_dir, dir)
      os.remove(os.path.join(path, 'restriction_values.xml'))
      os.rmdir(path)
  open(os.path.join(resources_dir, 'remove_localized_resources.d.stamp'), 'w')
  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv[1]))
