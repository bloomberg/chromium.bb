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

do_help() {
  cat << EOF
Usage: $(basename "$0") [command|option ...]

command should be one of:
  build - Build.
  test - Run unit tests (does not build).
  perftest - Run perf tests (does not build).
  pytest - Run Python unit tests (does not build).
  gn - Run gn for mojo (does not sync).
  sync - Sync using gclient (does not run gn).
  show-bash-alias - Outputs an appropriate bash alias for mojob. In bash do:
      \$ eval \`mojo/tools/mojob.sh show-bash-alias\`

option (which will only apply to commands which follow) should be one of:
  General options (specify before everything):
    --debug / --release / --debug-and-release - Debug (default) build /
        Release build / Debug and Release builds.
  gn options (specify before gn):
    --clang / --gcc - Use clang (default) / gcc.
    --use-goma / --no-use-goma - Use goma (if \$GOMA_DIR is set or \$HOME/goma
        exists; default) / don't use goma.

Note: It will abort on the first failure (if any).
EOF
}

get_gn_arg_value() {
  grep -m 1 "^[[:space:]]*\<$2\>" "$1/args.gn" | \
      sed -n 's/.* = "\?\([^"]*\)"\?$/\1/p'
}

do_build() {
  echo "Building in out/$1 ..."
  if [ "$(get_gn_arg_value "out/$1" use_goma)" = "true" ]; then
    # Use the configured goma directory.
    local goma_dir="$(get_gn_arg_value "out/$1" goma_dir)"
    echo "Ensuring goma (in ${goma_dir}) started ..."
    "${goma_dir}/goma_ctl.py" ensure_start

    ninja -j 1000 -l 100 -C "out/$1" mojo || exit 1
  else
    ninja -C "out/$1" mojo || exit 1
  fi
}

do_unittests() {
  echo "Running unit tests in out/$1 ..."
  mojo/tools/test_runner.py mojo/tools/data/unittests "out/$1" \
      mojob_test_successes || exit 1
}

do_perftests() {
  echo "Running perf tests in out/$1 ..."
  "out/$1/mojo_public_system_perftests" || exit 1
}

do_gn() {
  local gn_args="$(make_gn_args $1)"
  echo "Running gn with --args=\"${gn_args}\" ..."
  gn gen --args="${gn_args}" "out/$1"
}

do_sync() {
  # Note: sync only (with hooks, but no gyp-ing).
  GYP_CHROMIUM_NO_ACTION=1 gclient sync || exit 1
}

# Valid values: Debug, Release, or Debug_and_Release.
BUILD_TYPE=Debug_and_Release
should_do_Debug() {
  test "$BUILD_TYPE" = Debug -o "$BUILD_TYPE" = Debug_and_Release
}
should_do_Release() {
  test "$BUILD_TYPE" = Release -o "$BUILD_TYPE" = Debug_and_Release
}

# Valid values: clang or gcc.
COMPILER=clang
# Valid values: auto or disabled.
GOMA=auto
make_gn_args() {
  local args=()
  # TODO(vtl): It's a bit of a hack to infer the build type from the output
  # directory name, but it's what we have right now (since we support "debug and
  # release" mode).
  case "$1" in
    Debug)
      # (Default.)
      ;;
    Release)
      args+=("is_debug=false")
      ;;
  esac
  case "$COMPILER" in
    clang)
      # (Default.)
      ;;
    gcc)
      args+=("is_clang=false")
      ;;
  esac
  case "$GOMA" in
    auto)
      if [ -v GOMA_DIR ]; then
        args+=("use_goma=true" "goma_dir=\"${GOMA_DIR}\"")
      elif [ -d "${HOME}/goma" ]; then
        args+=("use_goma=true" "goma_dir=\"${HOME}/goma\"")
      else
        :  # (Default.)
      fi
      ;;
    disabled)
      # (Default.)
      ;;
  esac
  echo "${args[*]}"
}

# We're in src/mojo/tools. We want to get to src.
cd "$(realpath "$(dirname "$0")")/../.."

if [ $# -eq 0 ]; then
  do_help
  exit 0
fi

for arg in "$@"; do
  case "$arg" in
    # Commands -----------------------------------------------------------------
    help|--help)
      do_help
      exit 0
      ;;
    build)
      should_do_Debug && do_build Debug
      should_do_Release && do_build Release
      ;;
    test)
      should_do_Debug && do_unittests Debug
      should_do_Release && do_unittests Release
      ;;
    perftest)
      should_do_Debug && do_perftests Debug
      should_do_Release && do_perftests Release
      ;;
    gn)
      should_do_Debug && do_gn Debug
      should_do_Release && do_gn Release
      ;;
    sync)
      do_sync
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
    # Options ------------------------------------------------------------------
    --debug)
      BUILD_TYPE=Debug
      ;;
    --release)
      BUILD_TYPE=Release
      ;;
    --debug-and-release)
      BUILD_TYPE=Debug_and_Release
      ;;
    --clang)
      COMPILER=clang
      ;;
    --gcc)
      COMPILER=gcc
      ;;
    --use-goma)
      GOMA=auto
      ;;
    --no-use-goma)
      GOMA=disabled
      ;;
    *)
      echo "Unknown command \"${arg}\". Try \"$(basename "$0") help\"."
      exit 1
      ;;
  esac
done

exit 0
