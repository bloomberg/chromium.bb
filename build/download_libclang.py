#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to download libclang binaries from google storage."""

import os
import sys

sys.path.append(os.path.join(os.path.abspath(os.path.dirname(__file__)), '..',
                             'tools', 'clang', 'scripts'))

import update

# TODO(hans): Remove this forwarding hack after all callers of this script have
# been updated to call update.py instead.
if __name__ == '__main__':
  sys.argv = [sys.argv[0], '--package=libclang']
  sys.exit(update.main())
