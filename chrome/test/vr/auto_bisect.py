#!/usr/bin/python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to bisect a VR perf regression.

This is only meant to be used on a developer's workstation, not on bots or any
other sort of automated infrastructure. This is to help reduce the amount of
work necessary to bisect VR perf regressions until the perf dashboard supports
automatic bisects on devices outside of the Telemetry lab.

As a result, this is a bit crude and makes a number of assumptions.
"""


import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time


class TempDir():
  """Context manager for temp dirs since Python 2 doesn't have one."""
  def __enter__(self):
    self._dirpath = tempfile.mkdtemp()
    return self._dirpath

  def __exit__(self, type, value, traceback):
    shutil.rmtree(self._dirpath)


def VerifyCwd():
  """Checks that the script is being run from the Chromium root directory.

  Not robust in the slightest, but should catch common issues like running the
  script from the directory that it's located in.
  """
  if os.path.basename(os.getcwd()) != 'src':
    raise RuntimeError('Script must be run from the Chromium root directory')


def ParseArgsAndAssertValid():
  """Parses all the provided arguments and ensures everything is valid."""
  parser = argparse.ArgumentParser();

  # Arguments related to the actual bisection
  parser.add_argument_group('bisect arguments')
  parser.add_argument('--good-revision', required=True,
                      help='A known good Git revision')
  parser.add_argument('--bad-revision', required=True,
                      help='A known bad Git revision')
  parser.add_argument('--metric', required=True,
                      help='The perf metric being bisected')
  parser.add_argument('--story', required=True,
                      help='The perf story to check the affected metric in')
  parser.add_argument('--good-value', type=float,
                      help='The value of the metric at the good revision. If '
                           'not defined, an extra test iteration will be run '
                           'to determine the value')
  parser.add_argument('--bad-value', type=float,
                      help='The value of the metric at the bad revision. If '
                           'not defined, an extra test iteration will be run '
                           'to determine the value')
  parser.add_argument('--clank-revision',
                      help='The Clank revision to sync to during the bisect. '
                           'Only necessary if the revision on ToT is not '
                           'compatible with the revision range of the bisect')
  parser.add_argument('--reset-before-sync', action='store_true',
                      default=False,
                      help='When set, runs "git reset --hard HEAD" before '
                           'syncing. This has the potential to accidentally '
                           'delete any uncommitted changes, but can help avoid '
                           'random bisect failures.')

  parser.add_argument_group('swarming arguments')
  parser.add_argument('--swarming-server', required=True,
                      help='The swarming server to trigger the test on')
  parser.add_argument('--isolate-server', required=True,
                      help='The isolate server to upload the test to')
  def dimension(arg):
    split_arg = arg.split(',')
    if len(split_arg) != 2:
      raise argparse.ArgumentError(
          'Expected two comma-separated strings for --dimension, '
          'received %d' % len(split_arg))
    return {split_arg[0]: split_arg[1]}
  parser.add_argument('--dimension', action='append', type=dimension,
                      default=[], dest='dimensions',
                      help='A comma-separated swarming dimension key/value '
                           'pair. At least one must be provided.')
  parser.add_argument('--isolate-target',
                      help='The target to isolate. Defaults to the build '
                           'target')

  parser.add_argument_group('compile arguments')
  parser.add_argument('-j', '--parallel-jobs', type=int, default=1000,
                      help='The number of parallel jobs ninja will compile '
                           'with')
  parser.add_argument('-l', '--load-limit', type=int, default=70,
                      help='Don\'t start new ninja jobs if the average CPU '
                           'is above this')
  parser.add_argument('--build-output-dir',
                      default=os.path.join('out', 'Release'),
                      help='The directory that builds will take place in. '
                           'Assumes that gn args have already been generated '
                           'for the provided directory. Must be relative to '
                           'the Chromium src/ directory, e.g. out/Release.')
  parser.add_argument('--build-target', required=True,
                      help='The target to build for testing')

  args, unknown_args = parser.parse_known_args()

  # Set defaults
  if not args.isolate_target:
    args.isolate_target = args.build_target

  # Make sure we have at least one swarming dimension
  if len(args.dimensions) == 0:
    raise RuntimeError('No swarming dimensions provided')

  return (args, unknown_args)


def VerifyInput(args, unknown_args):
  """Verifies with the user that the provided args are satisfactory.

  Args:
    args: The known args parsed by the argument parser
    unknown_args: The unknown args parsed by the argument parser
  """
  print '======'
  print 'This will start a bisect for a for:'
  print 'Metric: %s' % args.metric
  print 'Story: %s' % args.story
  if args.good_value == None and args.bad_value == None:
    print 'The good and bad values at %s and %s will be determined' % (
        args.good_revision, args.bad_revision)
  elif args.good_value == None:
    print ('The good value at %s will be determined, and the bad value of %f '
           'at %s will be used' % (args.good_revision, args.bad_value,
            args.bad_revision))
  elif args.bad_value == None:
    print ('The good value of %f at %s will be used, and the bad value at %s '
           'will be determined' % (args.good_value, args.good_revision,
            args.bad_revision))
  else:
    print 'Changing from %f at %s to %f at %s' % (args.good_value,
        args.good_revision, args.bad_value, args.bad_revision)
  print '======'
  print 'The test target %s will be built to %s' % (args.build_target,
                                                    args.build_output_dir)
  print '%d parallel jobs will be used with a load limit of %d' % (
      args.parallel_jobs, args.load_limit)
  print '======'
  print 'The target %s will be isolated and uploaded to %s' % (
      args.isolate_target, args.isolate_server)
  print 'The test will be triggered on %s with the following dimensions:' % (
      args.swarming_server)
  for pair in args.dimensions:
    for key, val in pair.iteritems():
      print '%s = %s' % (key, val)
  print '======'
  print 'The test will be run with these additional arguments:'
  for extra_arg in unknown_args:
    print extra_arg
  print '======'
  if args.reset_before_sync:
    print '**WARNING** This will run git reset --hard HEAD'
    print 'If you have any uncommitted changes, commit or stash them beforehand'
  if raw_input('Are these settings correct? y/N').lower() != 'y':
    print 'Aborting'
    sys.exit(1)


def SetupBisect(args):
  """Does all the one-time setup for a bisect.

  This includes creating a new branch and starting a bisect.

  Args
    args: The parsed args from argparse
  Returns:
    The name of the git branch created and the first revision to sync to
  """
  # bisect-year-month-day-hour-minute-second
  branch_name = 'bisect-' + time.strftime('%Y-%m-%d-%H-%M-%S')
  subprocess.check_output(['git', 'checkout', '-b', branch_name])
  subprocess.check_output(['git', 'bisect', 'start'])
  subprocess.check_output(['git', 'bisect', 'good', args.good_revision])
  output = subprocess.check_output(['git', 'bisect', 'bad', args.bad_revision])
  print output
  # Get the revision, which is between []
  revision = output.split('[', 1)[1].split(']', 1)[0]
  return (branch_name, revision)


def RunTestOnSwarming(args, unknown_args, output_dir):
  """Isolates the test target and runs it on swarming to get perf results.

  Args:
    args: The known args parsed by the argument parser
    unknown_args: The unknown args parsed by the argument parser
    output_dir: The directory to save swarming results to
  """
  print "=== Isolating and running target %s ===" % args.isolate_target
  print 'Isolating'
  subprocess.check_output(['python', os.path.join('tools', 'mb', 'mb.py'),
                           'isolate',
                           '//%s' % args.build_output_dir, args.isolate_target])
  print 'Uploading'
  output = subprocess.check_output([
      'python', os.path.join('tools', 'swarming_client', 'isolate.py'),
      'batcharchive', '--isolate-server', args.isolate_server,
      os.path.join(args.build_output_dir,
                   '%s.isolated.gen.json' % args.isolate_target)])
  isolate_hash = output.split(' ')[0]

  swarming_args = [
    'python', os.path.join('tools', 'swarming_client', 'swarming.py'), 'run',
    '--isolated', isolate_hash,
    '--isolate-server', args.isolate_server,
    '--swarming', args.swarming_server,
    '--task-output-dir', output_dir,
  ]
  for pair in args.dimensions:
    for key, val in pair.iteritems():
      swarming_args.extend(['--dimension', key, val])

  swarming_args.append('--')
  swarming_args.extend(unknown_args)
  swarming_args.extend([
      '--isolated-script-test-output',
      '${ISOLATED_OUTDIR}/output.json',
      '--isolated-script-test-perf-output',
      '${ISOLATED_OUTDIR}/perftest-output.json',
      '--output-format', 'chartjson',
      '--chromium-output-directory', args.build_output_dir])
  print 'Running test'
  subprocess.check_output(swarming_args)


def GetSwarmingResult(args, unknown_args, output_dir):
  """Extracts the value for the story/metric combo of interest from swarming.

  Args:
    args: The known args parsed from the argument parser
    unknown_args: The unknown args parsed from the argument parser
    output_dir: The directory where swarming results have been saved to

  Returns:
    The value for the story/metric combo that the last swarming run produced
  """
  with open(
      os.path.join(output_dir, '0', 'perftest-output.json'), 'r') as infile:
    perf_results = json.load(infile)
    all_results = perf_results.get(unicode('charts'), {}).get(
        unicode(args.metric), {}).get(unicode(args.story), {}).get(
        unicode('values'), [])
    if len(all_results) == 0:
      raise RuntimeError('Got no results for the story/metric combo. '
                         'Is there a typo in one of them?')
    result = all_results[0]
    print 'Got result %s' % str(result)
  return result


def RunBisectStep(args, unknown_args, revision, output_dir):
  """Runs a bisect step for a revision.

  This will run recursively until the culprit CL is found.

  Args:
    args: The known args parsed from the argument parser
    unknown_args: The unknown args parsed from the argument parser
    revision: The git revision to sync to and test
    output_dir: The directory to save swarming results to
  """
  result = GetValueAtRevision(args, unknown_args, revision, output_dir)
  output = ""
  # Regression was an increased value
  if args.bad_value > args.good_value:
    # If we're greater than the provided bad value or between good and bad, but
    # closer to bad, we're still bad
    if (result > args.bad_value or
        abs(args.bad_value - result) < abs(args.good_value - result)):
      print '=== Current revision is BAD ==='
      output = subprocess.check_output(['git', 'bisect', 'bad'])
    else:
      print '=== Current revision is GOOD ==='
      output = subprocess.check_output(['git', 'bisect', 'good'])
  # Regression was a decreased value
  else:
    # If we're smaller than the provided bad value or between good and bad, but
    # closer to bad, we're still bad
    if (result < args.bad_value or
        abs(args.bad_value - result) < abs(args.good_value - result)):
      print '=== Current revision is BAD ==='
      output = subprocess.check_output(['git', 'bisect', 'bad'])
    else:
      print '=== Current revision is GOOD ==='
      output = subprocess.check_output(['git', 'bisect', 'good'])

  print output
  if output.startswith('Bisecting:'):
    RunBisectStep(args, unknown_args, output.split('[', 1)[1].split(']', 1)[0],
        output_dir)


def SyncAndBuild(args, unknown_args, revision):
  """Syncs to the given revision and builds the test target.

  Args:
    args: The known args parsed by the argument parser
    unknown_args: The unknown args parsed by the argument parser
    revision: The revision to sync to and build
  """
  print '=== Syncing to revision %s and building ===' % revision
  # Sometimes random files show up as unstaged changes (???), so make sure that
  # isn't the case before we try to run gclient sync
  if args.reset_before_sync:
    subprocess.check_output(['git', 'reset', '--hard', 'HEAD'])
  print 'Syncing'
  output = subprocess.check_output(['gclient', 'sync', '-r',
      'src@%s' % revision])
  if ('error: Your local changes to the following files would be overwritten '
      'by checkout:' in output):
    raise RuntimeError('Could not run gclient sync due to uncommitted changes. '
        'If these changes are actually yours, please commit or stash them. If '
        'they are not, remove them and try again. If the issue persists, try '
        'running with --reset-before-sync')
  if args.clank_revision:
    os.chdir('clank')
    subprocess.check_output(['git', 'checkout', args.clank_revision])
    os.chdir('..')
  print 'Building'
  subprocess.check_output(['ninja', '-C', args.build_output_dir,
                           '-j', str(args.parallel_jobs),
                           '-l', str(args.load_limit), args.build_target])


def BisectRegression(args, unknown_args):
  """Runs all steps necessary to bisect a perf regression.

  Intermediate steps and the culprit CL will be printed to stdout.

  Args:
    args: The known args parsed by the argument parser
    unknown_args: The unknown args parsed by the argument parser
  """
  with TempDir() as output_dir:
    try:
      if args.good_value == None:
        args.good_value = GetValueAtRevision(args, unknown_args,
            args.good_revision, output_dir)
        print '=== Got initial good value of %f ===' % args.good_value
      if args.bad_value == None:
        args.bad_value = GetValueAtRevision(args, unknown_args,
            args.bad_revision, output_dir)
        print '=== Got initial bad value of %f ===' % args.bad_value
      branch_name, revision = SetupBisect(args)
      RunBisectStep(args, unknown_args, revision, output_dir)
    finally:
      subprocess.check_output(['git', 'bisect', 'reset'])


def GetValueAtRevision(args, unknown_args, revision, output_dir):
  """Builds and runs the test at a particular revision.

  Args:
    args: The known args parsed by the argument parser
    unknown_args: The unknown args parsed by the argument parser
    revision: The revision to sync to and build
    output_dir: The directory to store swarming results to

  Returns:
    The value of the story/metric combo at the given revision
  """
  SyncAndBuild(args, unknown_args, revision)
  RunTestOnSwarming(args, unknown_args, output_dir)
  return GetSwarmingResult(args, unknown_args, output_dir)

def main():
  VerifyCwd()
  args, unknown_args = ParseArgsAndAssertValid()
  VerifyInput(args, unknown_args)
  BisectRegression(args, unknown_args)

if __name__ == '__main__':
  main()
