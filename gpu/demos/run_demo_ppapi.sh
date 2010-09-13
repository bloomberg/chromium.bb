#!/bin/sh
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if [ -z "$1" ]; then
    echo "Usage: $(basename "$0") <demo_name> [--debug]" >&2
    echo >&2
    echo "Builds and runs PPAPI demo inside Chrome." >&2
    echo "Debug flag attaches the debugger to renderer process." >&2
    echo >&2
    echo "Try: $0 hello_triangle" >&2
    echo >&2
    exit 1
fi
if [ "$2" == "--debug" ]; then
    CHROME_DEBUG="--renderer-cmd-prefix=xterm -geometry 150x40 -e gdb --args"
else
    CHROME_DEBUG=
fi
CHROME_SRC="$(dirname "$(dirname "$(dirname "$(readlink -f "$0")")")")"
PLUGIN_LIB="$CHROME_SRC/out/Debug/lib${1}_ppapi.so"
PLUGIN_MIME="pepper-application/x-gpu-demo"
cd "$CHROME_SRC"
make "${1}_ppapi" && out/Debug/chrome --use-gl=osmesa --enable-gpu-plugin \
    "${CHROME_DEBUG}" \
    --register-pepper-plugins="$PLUGIN_LIB;$PLUGIN_MIME" \
    "file://$CHROME_SRC/gpu/demos/pepper_gpu_demo.html"

