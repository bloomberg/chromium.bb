#!/bin/bash
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Shim script to allow chrome tree to control how nacl's tests get run.
# NOTE: if you change this file, also change the corresponding .bat file.

# TODO(bradnelson): Make this a nop until we can rev nacl safely.
exit 0

SCRIPT_DIR=$(cd $(dirname $0) ; pwd)
ROOT_DIR=${SCRIPT_DIR}/../../..
cd ${ROOT_DIR}
python ../../../scripts/slave/runtest.py --target Release \
    --build-dir src/build nacl_ui_tests --gtest_print_time
