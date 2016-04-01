#!/usr/bin/env bash
export EDITOR=${EDITOR:=notepad}
WIN_BASE=`dirname $0`
UNIX_BASE=`cygpath "$WIN_BASE"`
export PATH=$PATH:$UNIX_BASE/SVN_BIN_DIR:$UNIX_BASE/PYTHON_BIN_DIR:$UNIX_BASE/PYTHON_BIN_DIR/Scripts
export PYTHON_DIRECT=1
export PYTHONUNBUFFERED=1
if [[ $# > 0 ]]; then
  $UNIX_BASE/GIT_BIN_DIR/bin/bash.exe "$@"
else
  $UNIX_BASE/GIT_BIN_DIR/git-bash.exe &
fi
