#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import sys

if len(sys.argv) != 5:
  print ("Usage: strip_chrome_binary.py /path/to/eu-strip /path/to/chrome"
         "/path/to/chrome.debug /path/to/chrome.stripped")
  sys.exit(1)
eu_strip = sys.argv[1]
chrome = sys.argv[2]
chrome_debug = sys.argv[3]
chrome_stripped = sys.argv[4]
sys.exit(subprocess.call([eu_strip, '-o', chrome_stripped, '-f', chrome_debug,
                          chrome]))
