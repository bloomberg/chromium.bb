#!/bin/bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# DO NOT MODIFY. Scheduled to be removed.
# Buildbot configs will be changed to call bb_run_bot.py directly.
exec "$(dirname $0)"/bb_run_bot.py "$@" --bot-id perf-builder-rel
