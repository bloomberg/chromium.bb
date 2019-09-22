# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compiled build api proto sanity check."""

from __future__ import print_function

import os

from chromite.api import compile_build_api_proto
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib


class ProtoGeneratedTest(cros_test_lib.TempDirTestCase):
  """Check if the generated proto is up to date."""

  def _GetGenHashes(self, directory):
    """Get the hashes of the generated files in the directory."""
    md5s = {}
    pb2s = []
    for dirpath, _dirnames, files in os.walk(directory):
      curpb = [os.path.join(dirpath, f) for f in files if f.endswith('_pb2.py')]
      if curpb:
        pb2s.extend(curpb)

    cmd = ['md5sum'] + pb2s
    output = cros_build_lib.run(cmd).output

    for line in output.splitlines():
      md5, filename = line.split()
      md5s[filename.replace(directory, '')] = md5

    return md5s

  def testGeneratedUpToDate(self):
    """Test the generated files match the raw proto.

    Fails when the generated proto files in the repo do not match a fresh
    generation.

    This is accomplished by creating `'file/path': 'md5sum'` mappings for each
    file in each the repo and the fresh generation. This approach is somewhat
    more expensive than other approaches, such as `diff -r`, but also means we
    can include other changes in those folders, such as non-empty __init__.py
    and README files.
    """
    generated = os.path.join(constants.SOURCE_ROOT, 'chromite', 'api', 'gen')
    current_hash = self._GetGenHashes(generated)

    # Generate it into a tempdir so we don't change the files if they're not up
    # to date.
    compile_build_api_proto.CompileProto(output=self.tempdir)
    # Get the hashes from the freshly generated directory.
    new_hash = self._GetGenHashes(self.tempdir)

    self.assertDictEqual(current_hash, new_hash,
                         'The freshly compiled proto has new changes. '
                         'Please look into that. '
                         'Run chromite/api/compile_build_api_proto to generate '
                         'the new proto.')
