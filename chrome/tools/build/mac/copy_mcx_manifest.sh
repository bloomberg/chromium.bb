#!/bin/bash

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

if [[ ${#} -ne 4 ]]; then
  echo "usage: ${0} APP_NAME IDENTIFIER SOURCE DESTINATION" >&2
  exit 1
fi

APP_NAME="${1}"
IDENTIFIER=${2}
SOURCE=${3}
DESTINATION=${4}

# All hail sed.
exec sed -e "s/@APP_NAME@/${APP_NAME}/g" \
         -e "s/@CHROMIUM_BUNDLE_ID@/${IDENTIFIER}/g" \
         "${SOURCE}" > "${DESTINATION}"
