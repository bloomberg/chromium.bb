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
import sys

TOP_DIR = 'native_client'

def NaclTopDir():
  cwd = os.getcwd()
  pos = cwd.find(TOP_DIR)
  if pos < 0:
    print 'ERROR: expected to be called from with %s' % TOP_DIR
  return cwd[:pos + len(TOP_DIR)]


sys.path.append(os.path.join(NaclTopDir(), 'tools'))
import code_hygiene


def FindOnPage(input_api, url, regex):
  """Given a url, download it and find the part matching a regex.
  Arguments:
    input_api: the limited set of input modules allowed in presubmit
    url: url to download
    regex: regex to match on
  Returns:
    A string extracted from the match, or None if no match or error.
  """
  try:
    connection = input_api.urllib2.urlopen(url)
    text = connection.read()
    connection.close()
    match = input_api.re.search(regex, text)
    if match:
      return match.group(1)
    else:
      return None
  except IOError, e:
    print str(e)
    return None


def CheckTreeIsOpen(input_api, output_api, url, url_text):
  """Similar to the one in presubmit_canned_checks except it shows an helpful
  status text instead.
  Arguments:
    input_api: the limited set of input modules allowed in presubmit
    output_api: the limited set of output modules allowed in presubmit
    url: url to get numerical tree status from
    url_text: url to get human readable tree status from
    regex: regex to match on
  Returns:
    A list of presubmit warnings.
  """
  assert(input_api.is_committing)
  # Check if tree is open.
  status = FindOnPage(input_api, url, '([0-9]+)')
  if status and int(status):
    return []
  # Try to find out what failed.
  message = FindOnPage(input_api, url_text,
                       r'\<div class\="Notice"\>(.*)\<\/div\>')
  if message:
    return [output_api.PresubmitPromptWarning("The tree is closed.",
                                              long_text=message.strip())]
  # Report unknown reason.
  return [output_api.PresubmitPromptWarning(
      "The tree status can't be checked.")]

def CheckEolStyle(input_api, output_api, affected_files):
  """Verifies the svn:eol-style is set to LF.
  Canned checks implementation can be found in depot_tools.
  """
  sources = lambda x: affected_files
  res = input_api.canned_checks.CheckChangeSvnEolStyle(
      input_api, output_api, sources)
  return res

def CheckChangeOnUpload(input_api, output_api):
  report = []
  affected_files = input_api.AffectedFiles(include_deletes=False)
  for filename in affected_files:
    filename = filename.AbsoluteLocalPath()
    errors, warnings = code_hygiene.CheckFile(filename, False)
    for e in errors:
      report.append(output_api.PresubmitError(e, items=errors[e]))
    for w in warnings:
      report.append(output_api.PresubmitPromptWarning(w, items=warnings[w]))
  report.extend(CheckEolStyle(input_api, output_api, affected_files))
  return report


def CheckChangeOnCommit(input_api, output_api):
  report = []
  report.extend(CheckChangeOnUpload(input_api, output_api))
  report.extend(CheckTreeIsOpen(
      input_api, output_api,
      'http://nativeclient-status.appspot.com/status',
      'http://nativeclient-status.appspot.com/current'))
  return report
