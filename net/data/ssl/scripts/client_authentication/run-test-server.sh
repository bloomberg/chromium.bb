#!/bin/bash

# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Helper script for local manual testing. Runs an openssl test server at
# localhost:4433. The server will be accessible for www connections and will
# require a client certificate issued by Client Auth Test Root 1.
openssl s_server \
  -accept 4433 \
  -cert out/root_1.pem \
  -key out/root_1.key \
  -www \
  -Verify 5 \
  -CAfile out/root_1.pem
