#!/bin/bash

if [ -z "$*" ]; then
  exit 0
fi
set -o pipefail
dir="$1"
solution="${1%%/*}"
cd "$solution"
if [ "$solution" = "$1" ]; then
  shift
  # Don't "pull" if checkout is not on a named branch
  if test "$2" = "pull" && ( ! git symbolic-ref HEAD >/dev/null 2>/dev/null ); then
    first_args="$1 fetch"
  else
    first_args="$1 $2"
  fi
  shift 2
  $first_args $@ | sed "s/^/[$solution] /g" 1>&2
  if [ $? -ne 0 ]; then
    exit $?
  fi
  "$GIT_EXE" submodule --quiet sync
  "$GIT_EXE" ls-files -s | grep ^160000 | awk '{print $4}' |
  while read submod; do
    # Check whether this submodule should be ignored or updated.
    # If it's a new submodule, match the os specified in .gitmodules against
    # the os specified in .git/config.
    update_policy=$(git config "submodule.$submod.update" 2>/dev/null)
    if [ -z "$update_policy" ]; then
      target_os=$(git config target.os 2>/dev/null)
      submod_os=$(git config -f .gitmodules "submodule.$submod.os" 2>/dev/null)
      if [ -n "$target_os" -a -n "$submod_os" ] &&
          [ "$submod_os" != "all" -a "$submod_os" != "$target_os" ]; then
        update_policy=none
      else
        update_policy=checkout
      fi
      git config "submodule.$submod.update" $update_policy 2>/dev/null
    fi
    if [ "$update_policy" != "none" ]; then
      echo "$solution/$submod"
    fi
  done
  status=$?
else
  submodule="${1#*/}"
  echo "[$solution] updating $submodule ..."
  "$GIT_EXE" submodule update --quiet --init "$submodule" |
  ( grep -v '^Skipping submodule' || true ) | sed "s|^|[$1] |g"
  status=$?
  if [ "$status" -ne "0" ]; then
    echo "[$solution] FAILED to update $submodule"
  fi
fi
exit $status
