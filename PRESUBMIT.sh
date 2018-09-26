#!/bin/bash
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Check that:
#  - all C++ and header files conform to clang-format style.
#  - all header files have appropriate include guards.
#  - all GN files are formatted with `gn format`.
fail=0

function check_clang_format() {
  if ! cmp -s <(clang-format -style=file "$1") "$1"; then
    echo "Needs format: $1"
    fail=1
  fi
}

function check_include_guard() {
  guard_name=$(echo "$1" | sed ':a;s/\//_/;ta;s/\.h$/_h_/')
  guard_name=${guard_name^^}
  ifndef_count=$(grep -E "^#ifndef $guard_name\$" "$1" | wc -l)
  define_count=$(grep -E "^#define $guard_name\$" "$1" | wc -l)
  endif_count=$(grep -E "^#endif  // $guard_name\$" "$1" | wc -l)
  if [ $ifndef_count -ne 1 -o $define_count -ne 1 -o \
       $endif_count -ne 1 ]; then
    echo "Include guard missing/incorrect: $1"
    fail=1
  fi
}

function check_gn_format() {
  if ! which gn &>/dev/null; then
    echo "Please add gn to your PATH or manually check $1 for format errors."
  else
    if ! cmp -s <(cat "$1" | gn format --stdin) "$1"; then
      echo "Needs format: $1"
      fail=1
    fi
  fi
}

for f in $(git diff --name-only --diff-filter=d @{u}); do
  if echo $f | sed -n '/^third_party/q0;q1'; then
    continue;
  fi
  if echo $f | sed -n '/\.\(cc\|h\)$/q0;q1'; then
    # clang-format check.
    check_clang_format "$f"
    # Include guard check.
    if echo $f | sed -n '/\.h$/q0;q1'; then
      check_include_guard "$f"
    fi
  elif echo $f | sed -n '/\.gni\?$/q0;q1'; then
    check_gn_format "$f"
  fi
done

exit $fail
