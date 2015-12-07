#!/bin/bash

# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This uses lipo to thin out the specified file.

set -e

if [ $# -ne 1 ] ; then
  echo "usage: ${0} FILEPATH" >& 2
  exit 1
fi

FILEPATH="${1}"

lipo -thin x86_64 "${FILEPATH}" -o "${FILEPATH}"
