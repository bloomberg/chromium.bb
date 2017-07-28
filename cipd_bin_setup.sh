# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

function cipd_bin_setup {
    local MYPATH=$(dirname "${BASH_SOURCE[0]}")

    "$MYPATH/cipd" ensure \
        -log-level warning \
        -ensure-file "$MYPATH/cipd_manifest.txt" \
        -root "$MYPATH/.cipd_bin"
}
