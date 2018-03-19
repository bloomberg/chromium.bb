#!/bin/bash
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Check that all C++ and header files conform to clang-format style.
fail=0
for f in $(git diff --cached --name-only); do
  if echo $f | sed -n '/\.\(cc\|h\)$/q1;q0'; then
    continue;
  fi
  if ! cmp -s <(clang-format -style=file -assume-filename="$f" < <(git show ":$f")) <(git show ":$f"); then
    echo "Needs format: $f"
    fail=1
  fi
done

exit $fail
