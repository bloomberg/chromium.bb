#!/bin/bash

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This utility finds the different processes in a running instance of Chrome.
# It then attempts to identify their types (e.g. browser, extension, plugin,
# zygote, renderer). It also prints out information on whether a sandbox is
# active and what type of sandbox has been identified.

# This script is likely to only work on Linux or systems that closely mimick
# Linux's /proc filesystem.
[ -x /proc/self/exe ] || {
  echo "This script cannot be run on your system" >&2
  exit 1
}

# Find the browser's process id. If there are multiple active instances of
# Chrome, the caller can provide a pid on the command line. The provided pid
# must match a process in the browser's process hierarchy. When using the
# zygote inside of the setuid sandbox, renderers are in a process tree separate
# from the browser process. You cannot use any of their pids.
# If no pid is provided on the command line, the script will randomly pick
# one of the running instances.
if [ $# -eq 0 ]; then
  pid=$(ls -l /proc/*/exe 2>/dev/null |
        sed '/\/chrome$/s,.*/proc/\([^/]*\)/exe.*,\1,;t;d' |
        while read p; do
          xargs -0 </proc/$p/cmdline 2>/dev/null|grep -q -- --type= && continue
          echo "$p"
          break
        done)
else
  pid="$1"
fi
ls -l "/proc/$pid/exe" 2>/dev/null|egrep -q '/chrome$' || {
  echo "Cannot find any running instance of Chrome" >&2; exit 1; }
while :; do
  ppid="$(ps h --format ppid --pid "$pid" 2>/dev/null)"
  [ -n "$ppid" ] || {
    echo "Cannot find any running instance of Chrome" >&2; exit 1; }
  ls -l "/proc/$ppid/exe" 2>/dev/null|egrep -q '/chrome$' &&
    pid="$ppid" || break
done
xargs -0 </proc/$p/cmdline 2>/dev/null|grep -q -- --type= && {
  echo "Cannot find any running instance of Chrome" >&2; exit 1; }

# Iterate over child processes and try to identify them
identify() {
  local child cmd foundzygote plugin seccomp type
  foundzygote=0
  for child in $(ps h --format pid --ppid $1); do
    cmd="$(xargs -0 </proc/$child/cmdline|sed 's/ -/\n-/g')" 2>/dev/null
    type="$(echo "$cmd" | sed 's/--type=//;t1;d;:1;q')"
    case $type in
      '')
        echo "Process $child is part of the browser"
        identify "$child"
        ;;
      extension)
        echo "Process $child is an extension"
        ;;
      plugin)
        plugin="$(echo "$cmd" |
                 sed 's/--plugin-path=//;t1;d;:1
                      s,.*/lib,,;s,.*/npwrapper[.]lib,,;s,^np,,;s,[.]so$,,;q')"
        echo "Process $child is a \"$plugin\" plugin"
        identify "$child"
        ;;
      renderer|worker)
        # The seccomp sandbox has exactly one child process that has no other
        # threads. This is the trusted helper process.
        seccomp="$(ps h --format pid --ppid $child|xargs)"
        if [ $(echo "$seccomp" | wc -w) -eq 1 ] &&
           [ $(ls /proc/$seccomp/task 2>/dev/null | wc -w) -eq 1 ] &&
           ls -l /proc/$seccomp/exe 2>/dev/null | egrep -q '/chrome$'; then
          echo -n "Process $child is a sandboxed $type (seccomp helper:" \
                  "$seccomp)"
          [ -d /proc/$child/cwd/. ] || echo -n "; setuid sandbox is active"
          echo
        else
          echo -n "Process $child is a $type"
          [ -d /proc/$child/cwd/. ] || echo -n "; setuid sandbox is active"
          echo
          identify "$child"
        fi
        ;;
      zygote)
        foundzygote=1
        echo "Process $child is the zygote"
        identify "$child"
        ;;
      *)
        echo "Process $child is of unknown type \"$type\""
        identify "$child"
        ;;
    esac
  done
  return $foundzygote
}


echo "The browser's main pid is: $pid"
if identify "$pid"; then
  # The zygote can make it difficult to locate renderers, as the setuid
  # sandbox causes it to be reparented to "init". When this happens, we can
  # no longer associate it with the browser with 100% certainty. We make a
  # best effort by comparing command line strings.
  cmdline="$(xargs -0 </proc/$pid/cmdline |
             sed 's,\(/chrome \),\1--type=zygote ,;t
                  s,\(/chrome\)$,\1 --type=zygote,;t;d')" 2>/dev/null
  [ -n "$cmdline" ] &&
    for i in $(ps h --format pid --ppid 1); do
      if [ "$cmdline" = "$(xargs -0 </proc/$i/cmdline)" ]; then
        echo -n "Process $i is the zygote"
        [ -d /proc/$i/cwd/. ] || echo -n "; setuid sandbox is active"
        echo
        identify "$i"
      fi
    done
fi
