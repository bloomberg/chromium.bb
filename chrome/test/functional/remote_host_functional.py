#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import pyauto_functional
import pyauto
import remote_host


if __name__ == '__main__':
  pyauto_suite = pyauto.PyUITestSuite(sys.argv)
  remote_host.RemoteHost(('', 7410))
  del pyauto_suite
