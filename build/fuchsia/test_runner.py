#!/usr/bin/env python
#
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Stub for script that will run a Fuchsia binary on QEMU or hardware."""

import sys


def main():
  print sys.argv
  print 'TODO(fuchsia): Implement test runner. https://crbug.com/706592.'
  return 1


if __name__ == '__main__':
  sys.exit(main())
