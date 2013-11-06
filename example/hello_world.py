#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""This script is meant to be run on a Swarm slave."""

import sys


def main():
  print('Hello world: ' + sys.argv[1])
  return 0


if __name__ == '__main__':
  sys.exit(main())
