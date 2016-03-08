#!/bin/bash

# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

stunnel \
  -p /engine/data/stunnel.pem \
  -P /engine/stunnel.pid \
  -d 25466 -r 25467 -f &
/engine/blimp_engine_app \
  --disable-gpu \
  --use-remote-compositing \
  --disable-cached-picture-raster \
  --blimp-client-token-path=/engine/data/client_token \
  $@ &

# Stop execution if either stunnel or blimp_engine_app die.
wait -n
