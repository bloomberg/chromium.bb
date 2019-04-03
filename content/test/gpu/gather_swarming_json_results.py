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
import logging
import sys
import urllib2


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


def FindStepLogURL(steps, step_name, log_name):
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
        if log.get('name') == log_name:
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
  parser.add_argument('-v', '--verbose', action='store_true', default=False,
                      help='Enable verbose output')
  parser.add_argument('--bot', default='Linux FYI Release (NVIDIA)',
                      help='Which bot to examine')
  parser.add_argument('--build', type=int,
                      help='Which build to fetch (must be specified)')
  parser.add_argument('--step', default='webgl2_conformance_tests',
                      help='Which step to fetch (treated as a prefix)')
  parser.add_argument('--output', metavar='FILE', default='output.json',
                      help='Name of output file; contains only test run times')
  parser.add_argument('--full-output', metavar='FILE',
                      help='Name of complete output file if desired')

  options = parser.parse_args(rest_args)
  if options.verbose:
    logging.basicConfig(level=logging.DEBUG)

  if options.build is None:
    logging.error('Build number must be specified; check the bot\'s page')
    return 1

  build_json = GetJsonForBuildSteps(options.bot, options.build)
  if 'steps' not in build_json:
    logging.error('Returned Json data does not have "steps"')
    return 1

  logging.debug('Fetching information from bot %s, build %s',
                options.bot, options.build)

  json_output = FindStepLogURL(build_json['steps'], options.step, 'json.output')
  if not json_output:
    logging.error('Unable to find json.output from step starting with %s',
                  options.step)
    return 1
  logging.debug('json.output for step starting with %s: %s',
                options.step, json_output)

  merged_json = JsonLoadStrippingUnicode(json_output)
  extracted_times = {'times':{}}
  ExtractTestTimes(merged_json, '', extracted_times['times'])

  logging.debug('Saving output to %s', options.output)
  with open(options.output, 'w') as f:
    json.dump(extracted_times, f, sort_keys=True, indent=2,
              separators=(',', ': '))

  if options.full_output is not None:
    logging.debug('Saving full output to %s', options.full_output)
    with open(options.full_output, 'w') as f:
      json.dump(merged_json, f, sort_keys=True, indent=2,
                separators=(',', ': '))

  return 0


if __name__ == '__main__':
  sys.exit(main())
