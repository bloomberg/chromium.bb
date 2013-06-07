#!/bin/bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Bash functions used by buildbot annotator scripts for the android
# build of chromium.  Executing this script should not perform actions
# other than setting variables and defining of functions.

# Number of jobs on the compile line; e.g.  make -j"${JOBS}"
JOBS="${JOBS:-4}"

# Parse named arguments passed into the annotator script
# and assign them global variable names.
function bb_parse_args {
  while [[ $1 ]]; do
    case "$1" in
      --factory-properties=*)
        FACTORY_PROPERTIES="$(echo "$1" | sed 's/^[^=]*=//')"
        BUILDTYPE=$(bb_get_json_prop "$FACTORY_PROPERTIES" target)
        ;;
      --build-properties=*)
        BUILD_PROPERTIES="$(echo "$1" | sed 's/^[^=]*=//')"
        ;;
      --slave-properties=*)
        SLAVE_PROPERTIES="$(echo "$1" | sed 's/^[^=]*=//')"
        ;;
      *)
        echo "@@@STEP_WARNINGS@@@"
        echo "Warning, unparsed input argument: '$1'"
        ;;
    esac
    shift
  done
}

# Basic setup for all bots to run after a source tree checkout.
# Args:
#   $1: source root.
#   $2 and beyond: key value pairs which are parsed by bb_parse_args.
function bb_baseline_setup {
  SRC_ROOT="$1"
  # Remove SRC_ROOT param
  shift
  cd $SRC_ROOT

  bb_parse_args "$@"

  export GYP_GENERATORS=ninja
  export GOMA_DIR=/b/build/goma
  . build/android/envsetup.sh ""

  local extra_gyp_defines="$(bb_get_json_prop "$SLAVE_PROPERTIES" \
     extra_gyp_defines)"
  export GYP_DEFINES+=" fastbuild=1 $extra_gyp_defines"
  if echo $extra_gyp_defines | grep -qE 'clang|asan'; then
    unset CXX_target
  fi

  local build_path="${SRC_ROOT}/out/${BUILDTYPE}"
  local landmines_triggered_path="$build_path/.landmines_triggered"
  python "$SRC_ROOT/build/landmines.py"

  if [[ $BUILDBOT_CLOBBER || -f "$landmines_triggered_path" ]]; then
    echo "@@@BUILD_STEP Clobber@@@"

    if [[ -z $BUILDBOT_CLOBBER ]]; then
      echo "Clobbering due to triggered landmines: "
      cat "$landmines_triggered_path"
    else
      # Also remove all the files under out/ on an explicit clobber
      find "${SRC_ROOT}/out" -maxdepth 1 -type f -exec rm -f {} +
    fi

    # Sdk key expires, delete android folder.
    # crbug.com/145860
    rm -rf ~/.android
    rm -rf "$build_path"
    if [[ -e $build_path ]] ; then
      echo "Clobber appeared to fail?  $build_path still exists."
      echo "@@@STEP_WARNINGS@@@"
    fi
  fi
}

# Retrieve a packed json property using python
function bb_get_json_prop {
  local JSON="$1"
  local PROP="$2"

  python -c "import json; print json.loads('$JSON').get('$PROP', '')"
}
