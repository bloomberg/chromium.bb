#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script which gathers the JSON results merged from multiple
swarming shards of a step on the waterfall.

This is used to feed in the per-test times of previous runs of tests
to the browser_test_runner's sharding algorithm, to improve shard
distribution.
"""

import argparse
import json
import os
import subprocess
import sys
import urllib2

SWARMING_SERVICE = 'https://chromium-swarm.appspot.com'

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.dirname(os.path.dirname(os.path.dirname(THIS_DIR)))
SWARMING_CLIENT_DIR = os.path.join(SRC_DIR, 'tools', 'swarming_client')


def CheckSwarmingAuth():
  output = subprocess.check_output([
    'python',
    os.path.join(SWARMING_CLIENT_DIR, 'auth.py'),
    'check',
    '--service',
    SWARMING_SERVICE])
  if not output.startswith('user:'):
    print 'Must run:'
    print '  tools/swarming_client/auth.py login --service ' + SWARMING_SERVICE
    print 'and authenticate with @google.com credentials.'
    sys.exit(1)


def GetJsonForBuildSteps(bot, build):
  # Explorable via RPC explorer:
  # https://cr-buildbucket.appspot.com/rpcexplorer/services/
  #   buildbucket.v2.Builds/GetBuild

  # The Python docs are wrong. It's fine for this payload to be just
  # a JSON string.
  call_arg = json.dumps({ 'builder': { 'project': 'chromium',
                                       'bucket': 'ci',
                                       'builder': bot },
                          'buildNumber': build,
                          'fields': 'steps.*.name,steps.*.logs' })
  headers = {
    'content-type': 'application/json',
    'accept': 'application/json'
  }
  request = urllib2.Request(
    'https://cr-buildbucket.appspot.com/prpc/buildbucket.v2.Builds/GetBuild',
    call_arg,
    headers)
  conn = urllib2.urlopen(request)
  result = conn.read()
  conn.close()

  # Result is a multi-line string the first line of which is
  # deliberate garbage and the rest of which is a JSON payload.
  return json.loads(''.join(result.splitlines()[1:]))


def JsonLoadStrippingUnicode(url):
  def StripUnicode(obj):
    if isinstance(obj, unicode):
      try:
        return obj.encode('ascii')
      except UnicodeEncodeError:
        return obj

    if isinstance(obj, list):
      return map(StripUnicode, obj)

    if isinstance(obj, dict):
      new_obj = type(obj)(
          (StripUnicode(k), StripUnicode(v)) for k, v in obj.iteritems() )
      return new_obj

    return obj

  # The following fails with Python 2.7.6, but succeeds with Python 2.7.14.
  conn = urllib2.urlopen(url + '?format=raw')
  result = conn.read()
  conn.close()
  return StripUnicode(json.loads(result))


def FindStepJsonOutput(steps, step_name):
  # The format of this JSON-encoded protobuf is defined here:
  # https://chromium.googlesource.com/infra/luci/luci-go/+/master/
  #   buildbucket/proto/step.proto
  # It's easiest to just use the RPC explorer to fetch one and see
  # what's desired to extract.
  for step in steps:
    if 'name' not in step or 'logs' not in step:
      continue
    if step['name'].startswith(step_name):
      for log in step['logs']:
        if log.get('name') == 'json.output':
          return log.get('viewUrl')
  return None


def ExtractTestTimes(node, node_name, dest):
  if 'times' in node:
    dest[node_name] = sum(node['times']) / len(node['times'])
  else:
    # Currently the prefix names in the trie are dropped. Could
    # concatenate them if the naming convention is changed.
    for k in node.iterkeys():
      if isinstance(node[k], dict):
        ExtractTestTimes(node[k], k, dest)


def main():
  rest_args = sys.argv[1:]
  parser = argparse.ArgumentParser(
    description='Gather JSON results from a run of a Swarming test.',
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('-v', '--verbose', action='count', default=0,
                      help='Enable verbose output (specify multiple times '
                      'for more output)')
  parser.add_argument('--bot', type=str, default='Linux FYI Release (NVIDIA)',
                      help='Which bot to examine')
  parser.add_argument('--build', default=-1, type=int,
                      help='Which build to fetch (must be specified)')
  parser.add_argument('--step', type=str, default='webgl2_conformance_tests',
                      help='Which step to fetch (treated as a prefix)')
  parser.add_argument('--output', type=str, default='output.json',
                      help='Name of output file; contains only test run times')
  parser.add_argument('--full-output', type=str, default='',
                      help='Name of complete output file if desired')

  options = parser.parse_args(rest_args)

  CheckSwarmingAuth()

  build = options.build
  if build < 0:
    print 'Build number must be specified; check the bot\'s page'
    return 1

  build_json = GetJsonForBuildSteps(options.bot, build)
  if 'steps' not in build_json:
    print 'Returned Json data does not have "steps"'
    return 1

  if options.verbose:
    print 'Fetching information from bot %s, build %s' % (options.bot, build)

  json_output = FindStepJsonOutput(build_json['steps'], options.step)
  if not json_output:
    print 'Unable to find json.output from step starting with ' + options.step
    return 1
  if options.verbose:
    print 'json.output for step starting with %s: %s' % (options.step,
                                                         json_output)

  merged_json = JsonLoadStrippingUnicode(json_output)
  extracted_times = {'times':{}}
  ExtractTestTimes(merged_json, '', extracted_times['times'])

  if options.verbose:
    print 'Saving output to %s' % options.output
  with open(options.output, 'w') as f:
    json.dump(extracted_times, f, sort_keys=True, indent=2,
              separators=(',', ': '))

  if options.full_output:
    if options.verbose:
      print 'Saving full output to %s' % options.full_output
    with open(options.full_output, 'w') as f:
      json.dump(merged_json, f, sort_keys=True, indent=2,
                separators=(',', ': '))

  return 0


if __name__ == '__main__':
  sys.exit(main())
