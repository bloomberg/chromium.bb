#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(ferringb): remove this as soon as depot_tools is gutted of its
# import logic, and is just a re-exec.

import os
import sys

# We want to use correct version of libraries even when executed through symlink.
sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..', '..'))

from chromite.scripts import cros_sdk


def main():
  """Compatibility functor to keep depot_tools chromite_wrapper happy"""
  cros_sdk.main(sys.argv[1:])

if __name__ == '__main__':
  main()
