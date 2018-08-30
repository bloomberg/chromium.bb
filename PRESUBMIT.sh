#!/bin/bash
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Check that:
#  - all C++ and header files conform to clang-format style.
#  - all header files have appropriate include guards.
fail=0
for f in $(git diff --name-only @{u}); do
  if echo $f | sed -n '/^third_party/q0;/\.\(cc\|h\)$/q1;q0'; then
    continue;
  fi
  # clang-format check.
  if ! cmp -s <(clang-format -style=file "$f") "$f"; then
    echo "Needs format: $f"
    fail=1
  fi
  # Include guard check.
  if echo $f | sed -n '/\.h$/q0;q1'; then
    guard_name=$(echo $f | sed ':a;s/\//_/;ta;s/\.h$/_h_/')
    guard_name=${guard_name^^}
    ifndef_count=$(grep -E "^#ifndef $guard_name\$" $f | wc -l)
    define_count=$(grep -E "^#define $guard_name\$" $f | wc -l)
    endif_count=$(grep -E "^#endif  // $guard_name\$" $f | wc -l)
    if [ $ifndef_count -ne 1 -o $define_count -ne 1 -o \
         $endif_count -ne 1 ]; then
      echo "Include guard missing/incorrect: $f"
      fail=1
    fi
  fi
done

exit $fail
