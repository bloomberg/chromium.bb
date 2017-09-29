#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import sys

# Generates a file whose contents are based on the current datetime. This file
# may be used as a build-wide dependency and input into the mojom bindings
# generator's --scrambled_message_id_salt flag to scramble message IDs in a
# unique but globally consistent manner for each build.


def main(output_file):
  with open(output_file, 'w') as f:
    f.write(str(datetime.datetime.now()))


if __name__ == '__main__':
  if len(sys.argv) < 2:
    print "Missing output filename."
    sys.exit(1)
  sys.exit(main(sys.argv[1]))
