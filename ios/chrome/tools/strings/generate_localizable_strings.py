# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


def Main(args):
  if len(args) < 5:
    sys.stderr.write(
        'usage: %s tool config include_dir datapack_dir output_dir locales ...'
        % os.path.basename(sys.argv[0]))
    sys.exit(1)

  tool, config_file, include_dir, datapack_dir, output_dir = args[:5]
  locales = args[5:]

  os.execv(tool, [tool, '-c', config_file, '-I', include_dir, '-p',
      datapack_dir, '-o', output_dir] + locales)
  sys.exit(1)


if __name__ == '__main__':
  Main(sys.argv[1:])
