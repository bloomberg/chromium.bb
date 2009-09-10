# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Strip a .proto of options not supported by open-source protobuf tools. """

import re
import sys

if __name__ == '__main__':
  if len(sys.argv) != 3:
    print "Usage: %s input_file output_file " % sys.argv[0]
    sys.exit(1)

  input_file = sys.argv[1]
  output_file = sys.argv[2]

  protobuf = open(input_file).read()

  # Comment out lines like "option java_api_version = 1;"
  protobuf = re.sub("(option .*api_version.*\=.*)", r"// \1", protobuf)

  # Comment out lines like "option java_java5_enums = false;"
  protobuf = re.sub("(option .*java_java5_enums.*\=.*)", r"// \1", protobuf)

  # Comment out the java package.
  protobuf = re.sub("(option .*java_package.*\=.*)", r"// \1", protobuf)

  open(output_file, "w").write(protobuf)

