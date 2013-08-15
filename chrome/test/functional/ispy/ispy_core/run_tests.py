#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

if __name__ == '__main__':
  suite = unittest.TestLoader().discover('.', pattern='*test.py')
  unittest.TextTestRunner(verbosity=2).run(suite)
