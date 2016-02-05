#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys


# This unfortunate little utility is needed by GYP to intersperse "--typemap"
# flags before each entry in a list of typemaps. The resulting argument list
# can then be fed to the bindings generator.


def main():
  for filename in sys.argv[1:]:
    print "--typemap"
    print filename


if __name__ == "__main__":
  sys.exit(main())
