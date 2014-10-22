#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

msg = '''\
ERROR: This script has merged with install-debian.wheezy.sysroot.py.
Please use that instead (with --arch=arm).
'''
sys.stderr.write(msg)
