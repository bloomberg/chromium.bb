#!/bin/bash

# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# this wrapper script ensures that the virtualenv and dependencies for sysmon
# are installed, and then runs sysmon.  Arguments are passed to sysmon.

# TODO(ayatane): Unify this and venv/create_env.sh after it is stable.
msg() {
  echo "$1" >&2
}

error() {
  echo "$1" >&2
  exit 1
}

VENV_DIR="$HOME/.virtualenvs/sysmon"

msg "Checking virtualenv."
if [[ -e $VENV_DIR ]]; then
  if [[ ! -d $VENV_DIR ]]; then
    rm -rf "$VENV_DIR"
  fi
else
  virtualenv "$VENV_DIR"
fi

msg "Entering virtualenv."
. "$VENV_DIR/bin/activate" || error "$VENV_DIR/bin/activate missing"

# The directory this script is located in.
DIR=$(dirname "$BASH_SOURCE")

CHROMITE_DIR=$(dirname "$DIR")
PKGDIR="$CHROMITE_DIR/venv/pip_packages"
SYSMON_DIR="$CHROMITE_DIR/scripts/sysmon"

msg "Ensuring dependencies are installed."
pip install --no-index -f "$PKGDIR" \
    -r "$SYSMON_DIR/requirements.txt" \
  || error "Failed to install deps"

msg "Starting sysmon."
exec "$DIR/sysmon" "$@"
