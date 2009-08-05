#!/usr/bin/python2.4
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os

_dependent_dirs = set([
  "chrome/common/extensions/api",
  "chrome/common/extensions/docs/build",
  "chrome/common/extensions/docs/js",
  "chrome/common/extensions/docs/static",
  "chrome/common/extensions/docs/template",
]);

_rebuild_warning = """
This change modifies file(s) which the extension docs depend on. You must
rebuild the extension docs.

Build by running the build.py script in chrome/common/extensions/docs/build/.

Be sure to include any modified resulting static files
(/common/extension/docs/*.html) in your final changelist. 
"""

def CheckChange(input_api, output_api):
  for f in input_api.AffectedFiles():
    dir = os.path.dirname(f.LocalPath())
    if (dir in _dependent_dirs):
      return [output_api.PresubmitPromptWarning(_rebuild_warning)]
  return []

def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)