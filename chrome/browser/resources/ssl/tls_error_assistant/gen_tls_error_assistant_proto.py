#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
 Convert the ASCII tls_error_assistant.asciipb proto into a binary resource.
"""

import os
import sys

# Import the binary proto generator. Walks up to the root of the source tree
# which is six directories above, and the finds the protobufs directory from
# there.
proto_generator_path = os.path.normpath(os.path.join(os.path.abspath(__file__),
    *[os.path.pardir] * 6 + ['chrome/browser/resources/protobufs']))
sys.path.insert(0, proto_generator_path)
from binary_proto_generator import BinaryProtoGenerator


class TLSErrorAssistantProtoGenerator(BinaryProtoGenerator):
  def ImportProtoModule(self):
    import tls_error_assistant_pb2
    globals()['tls_error_assistant_pb2'] = tls_error_assistant_pb2

  def EmptyProtoInstance(self):
    return tls_error_assistant_pb2.TLSErrorAssistantConfig()

  def ValidatePb(self, opts, pb):
    assert pb.version_id > 0
    assert len(pb.captive_portal_cert) > 0

  def ProcessPb(self, opts, pb):
    binary_pb_str = pb.SerializeToString()
    outfile = os.path.join(opts.outdir, opts.outbasename)
    open(outfile, 'wb').write(binary_pb_str)


def main():
  return TLSErrorAssistantProtoGenerator().Run()

if __name__ == '__main__':
  sys.exit(main())
