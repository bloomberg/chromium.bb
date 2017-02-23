#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script which gathers and merges the JSON results from multiple
swarming shards of a step on the waterfall.

This is used to feed in the per-test times of previous runs of tests
to the browser_test_runner's sharding algorithm, to improve shard
distribution.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import urllib
import urllib2

SWARMING_SERVICE = 'https://chromium-swarm.appspot.com'

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.dirname(os.path.dirname(os.path.dirname(THIS_DIR)))
SWARMING_CLIENT_DIR = os.path.join(SRC_DIR, 'tools', 'swarming_client')

class Swarming:
  @staticmethod
  def CheckAuth():
    output = subprocess.check_output([
      os.path.join(SWARMING_CLIENT_DIR, 'auth.py'),
      'check',
      '--service',
      SWARMING_SERVICE])
    if not output.startswith('user:'):
      print 'Must run:'
      print '  tools/swarming_client/auth.py login --service ' + \
          SWARMING_SERVICE
      print 'and authenticate with @google.com credentials.'
      sys.exit(1)

  @staticmethod
  def Collect(taskIDs, output_dir, verbose):
    cmd = [
      os.path.join(SWARMING_CLIENT_DIR, 'swarming.py'),
      'collect',
      '-S',
      SWARMING_SERVICE,
      '--task-output-dir',
      output_dir] + taskIDs
    if verbose:
      print 'Collecting Swarming results:'
      print cmd
    if verbose > 1:
      # Print stdout from the collect command.
      stdout = None
    else:
      fnull = open(os.devnull, 'w')
      stdout = fnull
    subprocess.check_call(cmd, stdout=stdout, stderr=subprocess.STDOUT)

  @staticmethod
  def ExtractShardTaskIDs(urls):
    SWARMING_URL = 'https://chromium-swarm.appspot.com/user/task/'
    taskIDs = []
    for k,v in urls.iteritems():
      if not k.startswith('shard'):
        raise Exception('Illegally formatted \'urls\' key %s' % k)
      if not v.startswith(SWARMING_URL):
        raise Exception('Illegally formatted \'urls\' value %s' % v)
      taskIDs.append(v[len(SWARMING_URL):])
    return taskIDs

class Waterfall:
  def __init__(self, waterfall):
    self._waterfall = waterfall
    self.BASE_URL = 'http://build.chromium.org/p/'
    self.BASE_JSON_BUILDERS_URL = self.BASE_URL + '%s/json/builders'
    self.BASE_JSON_BUILDS_URL = self.BASE_JSON_BUILDERS_URL + '/%s/builds'

  def GetJsonFromUrl(self, url):
    conn = urllib2.urlopen(url)
    result = conn.read()
    conn.close()
    return json.loads(result)

  def GetBuildNumbersForBot(self, bot):
    builds_json = self.GetJsonFromUrl(
      self.BASE_JSON_BUILDS_URL %
      (self._waterfall, urllib.quote(bot)))
    build_numbers = [int(k) for k in builds_json.keys()]
    build_numbers.sort()
    return build_numbers

  def GetMostRecentlyCompletedBuildNumberForBot(self, bot):
    builds = self.GetBuildNumbersForBot(bot)
    return builds[len(builds) - 1]

  def GetJsonForBuild(self, bot, build):
    return self.GetJsonFromUrl(
      (self.BASE_JSON_BUILDS_URL + '/%d') %
      (self._waterfall, urllib.quote(bot), build))


def JsonLoadStrippingUnicode(file, **kwargs):
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

  return StripUnicode(json.load(file, **kwargs))


def Merge(dest, src):
  if isinstance(dest, list):
    if not isinstance(src, list):
      raise Exception('Both must be lists: ' + dest + ' and ' + src)
    return dest + src

  if isinstance(dest, dict):
    if not isinstance(src, dict):
      raise Exception('Both must be dicts: ' + dest + ' and ' + src)
    for k in src.iterkeys():
      if k not in dest:
        dest[k] = src[k]
      else:
        dest[k] = Merge(dest[k], src[k])
    return dest

  return src


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
  parser.add_argument('--waterfall', type=str, default='chromium.gpu.fyi',
                      help='Which waterfall to examine')
  parser.add_argument('--bot', type=str, default='Linux Release (NVIDIA)',
                      help='Which bot on the waterfall to examine')
  parser.add_argument('--build', default=-1, type=int,
                      help='Which build to fetch (-1 means most recent)')
  parser.add_argument('--step', type=str, default='webgl2_conformance_tests',
                      help='Which step to fetch (treated as a prefix)')
  parser.add_argument('--output', type=str, default='output.json',
                      help='Name of output file; contains only test run times')
  parser.add_argument('--full-output', type=str, default='',
                      help='Name of complete output file if desired')
  parser.add_argument('--leak-temp-dir', action='store_true', default=False,
                      help='Deliberately leak temporary directory')
  parser.add_argument('--start-from-temp-dir', type=str, default='',
                      help='Start from temporary directory (for debugging)')

  options = parser.parse_args(rest_args)

  if options.start_from_temp_dir:
    tmpdir = options.start_from_temp_dir
    shard_dirs = [f for f in os.listdir(tmpdir)
                  if os.path.isdir(os.path.join(tmpdir, f))]
    numTaskIDs = len(shard_dirs)
  else:
    Swarming.CheckAuth()

    waterfall = Waterfall(options.waterfall)
    build = options.build
    if build < 0:
      build = waterfall.GetMostRecentlyCompletedBuildNumberForBot(options.bot)

    build_json = waterfall.GetJsonForBuild(options.bot, build)

    if options.verbose:
      print 'Fetching information from %s, bot %s, build %s' % (
        options.waterfall, options.bot, build)

    taskIDs = []
    for s in build_json['steps']:
      if s['name'].startswith(options.step):
        # Found the step.
        #
        # The Swarming shards happen to be listed in the 'urls' property
        # of the step. Iterate down them.
        if 'urls' not in s or not s['urls']:
          # Note: we could also just download json.output if it exists.
          print ('%s on waterfall %s, bot %s, build %s doesn\'t '
                 'look like a Swarmed task') % (
                   s['name'], options.waterfall, options.bot, build)
          return 1
        taskIDs = Swarming.ExtractShardTaskIDs(s['urls'])
        if options.verbose:
          print 'Found Swarming task IDs for step %s' % s['name']

        break
    if not taskIDs:
      print 'Problem gathering the Swarming task IDs for %s' % options.step
      return 1

    # Collect the results.
    tmpdir = tempfile.mkdtemp()
    Swarming.Collect(taskIDs, tmpdir, options.verbose)
    numTaskIDs = len(taskIDs)

  # Shards' JSON outputs are in sequentially-numbered subdirectories
  # of the output directory.
  merged_json = None
  for i in xrange(numTaskIDs):
    with open(os.path.join(tmpdir, str(i), 'output.json')) as f:
      cur_json = JsonLoadStrippingUnicode(f)
      if not merged_json:
        merged_json = cur_json
      else:
        merged_json = Merge(merged_json, cur_json)
  extracted_times = {'times':{}}
  ExtractTestTimes(merged_json, '', extracted_times['times'])

  with open(options.output, 'w') as f:
    json.dump(extracted_times, f, sort_keys=True, indent=2,
              separators=(',', ': '))

  if options.full_output:
    json.dump(merged_json, f, sort_keys=True, indent=2,
              separators=(',', ': '))

  if options.leak_temp_dir:
    print 'Temporary directory: %s' % tmpdir
  else:
    shutil.rmtree(tmpdir)

  return 0


if __name__ == "__main__":
  sys.exit(main())
