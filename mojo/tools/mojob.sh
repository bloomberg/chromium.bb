#!/bin/bash
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This a simple script to make building/testing Mojo components easier (on
# Linux).

# TODO(vtl): Maybe make the test runner smart and not run unchanged test
# binaries.
# TODO(vtl) Maybe also provide a way to pass command-line arguments to the test
# binaries.

# We're in src/mojo/tools. We want to get to src.
cd "$(realpath "$(dirname "$0")")/../.."

build() {
  ninja -C "out/$1" || exit 1
}

unittests() {
  "out/$1/mojo_system_unittests" || exit 1
  "out/$1/mojo_public_unittests" || exit 1
  "out/$1/mojo_bindings_test" || exit 1
}

perftests() {
  "out/$1/mojo_public_perftests" || exit 1
}

for arg in "$@"; do
  case "$arg" in
    help|--help)
      cat << EOF
Usage: $(basename "$0") [command ...]

command should be one of:
  build - Build Release and Debug.
  build-release - Build Release.
  build-debug - Build Debug.
  test - Run Release and Debug unit tests (does not build).
  test-release - Run Release unit tests (does not build).
  test-debug - Run Debug unit tests (does not build).
  perftest - Run Release and Debug perf tests (does not build).
  perftest-release - Run Release perf tests (does not build).
  perftest-debug - Run Debug perf tests (does not build).
  gyp - Run gyp for mojo (does not sync), with clang.
  gyp-gcc - Run gyp for mojo (does not sync), without clang.
  gyp-clang - Run gyp for mojo (does not sync), with clang.
  sync - Sync using gclient (does not run gyp).
  show-bash-alias - Outputs an appropriate bash alias for mojob. In bash do:
      \$ eval \`mojo/tools/mojob.sh show-bash-alias\`

Note: It will abort on the first failure (if any).
EOF
      exit 0
      ;;
    build)
      build Release
      build Debug
      ;;
    build-release)
      build Release
      ;;
    build-debug)
      build Debug
      ;;
    test)
      unittests Release
      unittests Debug
      ;;
    test-release)
      unittests Release
      ;;
    test-debug)
      unittests Debug
      ;;
    perftest)
      perftests Release
      perftests Debug
      ;;
    perftest-release)
      perftests Release
      ;;
    perftest-debug)
      perftests Debug
      ;;
    gyp)
      # Default to clang.
      GYP_DEFINES=clang=1 build/gyp_chromium mojo/mojo.gyp
      ;;
    gyp-gcc)
      GYP_DEFINES=clang=0 build/gyp_chromium mojo/mojo.gyp
      ;;
    gyp-clang)
      GYP_DEFINES=clang=1 build/gyp_chromium mojo/mojo.gyp
      ;;
    sync)
      # Note: sync only, no gyp-ing.
      gclient sync --nohooks
      ;;
    show-bash-alias)
      # You want to type something like:
      #   alias mojob=\
      #       '"$(pwd | sed '"'"'s/\(.*\/src\).*/\1/'"'"')/mojo/tools/mojob.sh"'
      # This is quoting hell, so we simply escape every non-alphanumeric
      # character.
      echo alias\ mojob\=\'\"\$\(pwd\ \|\ sed\ \'\"\'\"\'s\/\\\(\.\*\\\/src\\\)\
\.\*\/\\1\/\'\"\'\"\'\)\/mojo\/tools\/mojob\.sh\"\'
      ;;
    *)
      echo "Unknown command \"${arg}\". Try \"$(basename "$0") help\"."
      exit 1
      ;;
  esac
done
