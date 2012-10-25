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
  $@ | sed "s/^/[$solution] /g" 1>&2
  if [ $? -ne 0 ]; then
    exit $?
  fi
  "$GIT_EXE" submodule --quiet sync
  "$GIT_EXE" ls-files -s | grep ^160000 | awk '{print $4}' |
  sed "s/^/$solution\//g"
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
