#!/usr/bin/env python

import sys

# TODO(mark): sys.path manipulation is some temporary testing stuff.
try:
  import gyp
except ImportError, e:
  import os.path
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), 'pylib'))
  import gyp

if __name__ == '__main__':
  sys.exit(gyp.main(sys.argv[1:]))
