#!/bin/bash

# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Adds commit-msg hook to .git/hooks folder. This facilitates reviews with
# Gerrit by adding Change-Id fields to commit messages.

curl -Lo $(git rev-parse --git-dir)/hooks/commit-msg https://chromium-review.googlesource.com/tools/hooks/commit-msg
chmod +x $(git rev-parse --git-dir)/hooks/commit-msg
