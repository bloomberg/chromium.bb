#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple wrapper around the bundletool tool.

Bundletool is distributed as a versioned jar file. This script abstracts the
location and version of this jar file, as well as the JVM invokation."""

import os
import subprocess
import sys

# Assume this is stored under build/android/gyp/
BUNDLETOOL_DIR = os.path.abspath(os.path.join(
    sys.argv[0], '..', '..', '..', '..', 'third_party', 'android_build_tools',
    'bundletool'))

BUNDLETOOL_VERSION = '0.4.2'

BUNDLETOOL_JAR_PATH = os.path.join(
    BUNDLETOOL_DIR, 'bundletool-all-%s.jar' % BUNDLETOOL_VERSION)

args = ['java', '-jar', BUNDLETOOL_JAR_PATH] + sys.argv[1:]
subprocess.check_call(args)
