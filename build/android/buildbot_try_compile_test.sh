#!/bin/bash -ex
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Buildbot annotator script for trybots.  Compile and test.

BB_SRC_ROOT="$(cd "$(dirname $0)/../.."; pwd)"
. "${BB_SRC_ROOT}/build/android/buildbot_functions.sh"

# SHERIFF: if you need to quickly turn "android_test" trybots green,
# uncomment the next line (and send appropriate email out):
## bb_force_bot_green_and_exit

bb_baseline_setup "$BB_SRC_ROOT" "$@"
bb_compile
bb_reboot_phones
bb_run_unit_tests
bb_run_instrumentation_tests
