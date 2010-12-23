# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for depot tools.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""


def CheckChangeOnUpload(input_api, output_api):
  return RunTests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return RunTests(input_api, output_api)


def RunTests(input_api, output_api):
  """Run all the shells scripts in the directory test.

  Also verify the GAE python SDK is available, fetches Rietveld if necessary and
  start a test instance to test against.
  """
  # They are not exposed from InputApi.
  from os import listdir, pathsep
  import socket
  import time

  # Shortcuts
  join = input_api.os_path.join
  error = output_api.PresubmitError

  # Paths
  sdk_path = input_api.os_path.abspath(join('..', '..', 'google_appengine'))
  dev_app = join(sdk_path, 'dev_appserver.py')
  rietveld = join('test', 'rietveld')
  django_path = join(rietveld, 'django')

  # Generate a friendly environment.
  env = input_api.environ.copy()
  env['LANGUAGE'] = 'en'
  if env.get('PYTHONPATH'):
    env['PYTHONPATH'] = (env['PYTHONPATH'].rstrip(pathsep) + pathsep +
        django_path)
  else:
    env['PYTHONPATH'] = django_path

  def call(*args, **kwargs):
    kwargs['env'] = env
    x = input_api.subprocess.Popen(*args, **kwargs)
    x.communicate()
    return x.returncode == 0

  def test_port(port):
    s = socket.socket()
    try:
      return s.connect_ex(('127.0.0.1', port)) == 0
    finally:
      s.close()

  # First, verify the Google AppEngine SDK is available.
  if not input_api.os_path.isfile(dev_app):
    return [error('Install google_appengine sdk in %s' % sdk_path)]

  # Second, checkout rietveld and django if not available.
  if not input_api.os_path.isdir(rietveld):
    print('Checking out rietveld...')
    if not call(['svn', 'co', '-q',
                 'http://rietveld.googlecode.com/svn/trunk@563',
                 rietveld]):
      return [error('Failed to checkout rietveld')]
  if not input_api.os_path.isdir(django_path):
    print('Checking out django...')
    if not call(
        ['svn', 'co', '-q',
         'http://code.djangoproject.com/'
             'svn/django/branches/releases/1.0.X/django@13637',
         django_path]):
      return [error('Failed to checkout django')]


  # Test to find an available port starting at 8080.
  port = 8080
  while test_port(port) and port < 65000:
    port += 1
  if port == 65000:
    return [error('Having issues finding an available port')]

  verbose = False
  if verbose:
    stdout = None
    stderr = None
  else:
    stdout = input_api.subprocess.PIPE
    stderr = input_api.subprocess.PIPE
  output = []
  test_server = input_api.subprocess.Popen(
      [dev_app, rietveld, '--port=%d' % port,
       '--datastore_path=' + join(rietveld, 'tmp.db'), '-c'],
      stdout=stdout, stderr=stderr, env=env)
  try:
    # Loop until port 127.0.0.1:port opens or the process dies.
    while not test_port(port):
      test_server.poll()
      if test_server.returncode is not None:
        output.append(error('Test rietveld instance failed early'))
        break
      time.sleep(0.001)

    test_path = input_api.os_path.abspath('test')
    for test in listdir(test_path):
      # push-from-logs and rename fails for now. Remove from this list once they
      # work.
      if (test in ('push-from-logs.sh', 'rename.sh', 'test-lib.sh') or
          not test.endswith('.sh')):
        continue
      print('Running %s' % test)
      if not call([join(test_path, test)], cwd=test_path, stdout=stdout):
        output.append(error('%s failed' % test))
  finally:
    test_server.kill()
  return output
